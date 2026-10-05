// Native unit tests for the YIN pitch detector.
// Run with: pio test -e native

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../wav_reader.h"
#include "pitch.h"

static constexpr float RATE = 16000;
static constexpr float PI_F = 3.14159265f;

static float cents(float hz, float ref) { return 1200 * log2f(hz / ref); }

// ---- Signal generators -----------------------------------------------------

using Signal = std::vector<int16_t>;

// Sum of harmonics: amps[k] is the amplitude of harmonic k+1 (0..1 of full scale).
static Signal harmonics(float hz, const std::vector<float>& amps, size_t n, float noise = 0) {
    Signal s(n);
    srand(1234);
    for (size_t i = 0; i < n; i++) {
        float t = i / RATE, v = 0;
        for (size_t k = 0; k < amps.size(); k++) v += amps[k] * sinf(2 * PI_F * hz * (k + 1) * t);
        if (noise > 0) v += noise * ((rand() / (float)RAND_MAX) * 2 - 1);
        s[i] = (int16_t)std::max(-32767.0f, std::min(32767.0f, v * 32767));
    }
    return s;
}

static Signal sine(float hz, float amp = 0.3f) { return harmonics(hz, {amp}, 4096); }

// Plucked-string-like: harmonics falling off as 1/k.
static Signal saw(float hz) {
    std::vector<float> amps;
    for (int k = 1; k * hz < RATE / 2 && k <= 30; k++) amps.push_back(0.3f / k);
    return harmonics(hz, amps, 4096);
}

static pitch::Result detect(const Signal& s, const pitch::Config& cfg = pitch::Config()) {
    pitch::Yin yin(cfg);
    TEST_ASSERT_TRUE_MESSAGE(s.size() >= yin.frameSize(), "signal shorter than frame");
    return yin.detect(s.data(), s.size());
}

static void expectPitch(const Signal& s, float hz, float toleranceCents,
                        const pitch::Config& cfg = pitch::Config()) {
    pitch::Result r = detect(s, cfg);
    char msg[96];
    snprintf(msg, sizeof msg, "expected %.2f Hz, got %.3f Hz (%s)", hz, r.hz,
             r.voiced ? "voiced" : "unvoiced");
    TEST_ASSERT_TRUE_MESSAGE(r.voiced, msg);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(toleranceCents, 0, cents(r.hz, hz), msg);
}

// ---- Synthetic tests -------------------------------------------------------

static void test_sines_across_range() {
    const float notes[] = {30.87f, 41.20f, 55.00f, 82.41f, 110.0f, 196.0f, 329.63f, 440.0f, 659.3f, 987.8f};
    for (float hz : notes) expectPitch(sine(hz), hz, hz < 500 ? 1.0f : 3.0f);
}

static void test_harmonic_rich_strings() {
    const float notes[] = {41.20f, 55.00f, 73.42f, 98.00f, 82.41f, 146.8f, 246.9f, 329.63f};
    for (float hz : notes) expectPitch(saw(hz), hz, 1.0f);
}

// A small mic barely hears 41 Hz: the waveform still repeats every 24 ms, so
// YIN should report 41.2, not the 82.4 Hz second harmonic.
static void test_missing_fundamental() {
    expectPitch(harmonics(41.2f, {0, 0.3f, 0.2f, 0.15f, 0.1f, 0.08f}, 4096), 41.2f, 1.0f);
}

static void test_weak_fundamental() {
    expectPitch(harmonics(41.2f, {0.01f, 0.3f, 0.25f, 0.1f}, 4096), 41.2f, 1.0f);
}

static void test_detuned_note_reads_correct_cents() {
    const float hz = 110.0f * powf(2, 12.0f / 1200);  // A2, 12 cents sharp
    pitch::Result r = detect(saw(hz));
    TEST_ASSERT_TRUE(r.voiced);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.0f, cents(r.hz, 110.0f));
}

static void test_noisy_note() {
    std::vector<float> amps = {0.2f, 0.15f, 0.1f, 0.05f};
    expectPitch(harmonics(82.41f, amps, 4096, 0.05f), 82.41f, 2.0f);
}

static void test_silence_is_unvoiced() {
    Signal s(4096, 0);
    TEST_ASSERT_FALSE(detect(s).voiced);
}

static void test_white_noise_is_unvoiced() {
    Signal s(4096);
    srand(42);
    for (auto& v : s) v = (int16_t)((rand() % 6001) - 3000);
    TEST_ASSERT_FALSE(detect(s).voiced);
}

static void test_guitar_range_config() {
    pitch::Config cfg;
    cfg.minHz = 70;
    pitch::Yin yin(cfg);
    TEST_ASSERT_LESS_THAN(1000, yin.frameSize());  // much smaller frame than bass
    expectPitch(saw(82.41f), 82.41f, 1.0f, cfg);
    expectPitch(saw(329.63f), 329.63f, 1.0f, cfg);
}

// ---- Recordings ------------------------------------------------------------

struct Recording {
    const char* file;
    float hz;      // nominal string pitch
    float startS;  // part of the recording where the note rings clearly
    float endS;
};

// Run the detector every 50 ms over the ringing note. Real strings start a
// little sharp and settle, so rather than demand one exact pitch, require that
// the detector finds the note nearly every frame, never strays to another note
// or octave, and never jumps abruptly between frames.
static void checkRecording(const Recording& rec) {
    Signal s = readWav(rec.file);
    char msg[200];
    snprintf(msg, sizeof msg, "could not read recordings/%s", rec.file);
    TEST_ASSERT_TRUE_MESSAGE(!s.empty(), msg);

    pitch::Yin yin;
    std::vector<float> found;
    int frames = 0;
    float worstOffset = 0, worstJump = 0, prev = 0;
    for (size_t start = (size_t)(rec.startS * RATE);
         start + yin.frameSize() <= (size_t)(rec.endS * RATE) && start + yin.frameSize() <= s.size();
         start += (size_t)(0.05f * RATE)) {
        frames++;
        pitch::Result r = yin.detect(&s[start], yin.frameSize());
        if (!r.voiced) {
            prev = 0;
            continue;
        }
        found.push_back(r.hz);
        worstOffset = std::max(worstOffset, fabsf(cents(r.hz, rec.hz)));
        if (prev) worstJump = std::max(worstJump, fabsf(cents(r.hz, prev)));
        prev = r.hz;
    }
    TEST_ASSERT_GREATER_THAN(10, frames);

    snprintf(msg, sizeof msg, "%s: %d/%d frames voiced, worst offset %.1f c from %.2f Hz, worst jump %.1f c",
             rec.file, (int)found.size(), frames, worstOffset, rec.hz, worstJump);
    printf("%s\n", msg);

    TEST_ASSERT_TRUE_MESSAGE(found.size() >= frames * 0.9, msg);  // finds the note
    TEST_ASSERT_TRUE_MESSAGE(worstOffset < 30, msg);               // always the right note
    TEST_ASSERT_TRUE_MESSAGE(worstJump < 8, msg);                  // no abrupt jumps
}

static void test_recording_speaker_bass_e() { checkRecording({"speaker_bass_e.wav", 41.20f, 0.4f, 2.9f}); }
static void test_recording_speaker_bass_a() { checkRecording({"speaker_bass_a.wav", 55.00f, 0.4f, 2.9f}); }
static void test_recording_speaker_bass_d() { checkRecording({"speaker_bass_d.wav", 73.42f, 0.7f, 2.15f}); }
static void test_recording_speaker_bass_g() { checkRecording({"speaker_bass_g.wav", 98.00f, 0.4f, 2.9f}); }

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sines_across_range);
    RUN_TEST(test_harmonic_rich_strings);
    RUN_TEST(test_missing_fundamental);
    RUN_TEST(test_weak_fundamental);
    RUN_TEST(test_detuned_note_reads_correct_cents);
    RUN_TEST(test_noisy_note);
    RUN_TEST(test_silence_is_unvoiced);
    RUN_TEST(test_white_noise_is_unvoiced);
    RUN_TEST(test_guitar_range_config);
    RUN_TEST(test_recording_speaker_bass_e);
    RUN_TEST(test_recording_speaker_bass_a);
    RUN_TEST(test_recording_speaker_bass_d);
    RUN_TEST(test_recording_speaker_bass_g);
    return UNITY_END();
}
