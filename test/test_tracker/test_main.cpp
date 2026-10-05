// Native unit tests for the reading tracker (smoothing, outliers, hold, in-tune).
// Run with: pio test -e native

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unity.h>

#include "../wav_reader.h"
#include "pitch.h"
#include "tracker.h"

static constexpr uint32_t HOP_MS = 25;

static pitch::Result voiced(float hz) {
    pitch::Result r;
    r.voiced = true;
    r.hz = hz;
    r.confidence = 0.95f;
    r.rms = 100;
    return r;
}

static pitch::Result silent() { return pitch::Result(); }

static float centsSharp(float hz, float cents) { return hz * powf(2, cents / 1200); }

// Feed `frames` frames of the same input, returning the time after the last.
static uint32_t feed(tracker::Tracker& t, const pitch::Result& r, int frames, uint32_t now) {
    for (int i = 0; i < frames; i++, now += HOP_MS) t.update(r, now);
    return now;
}

static void test_starts_idle() {
    tracker::Tracker t;
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Idle);
}

static void test_new_note_needs_confirmation() {
    tracker::Tracker t;
    uint32_t now = feed(t, voiced(110), 2, 0);
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Idle);
    feed(t, voiced(110), 1, now);
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Live);
    TEST_ASSERT_EQUAL_INT(45, t.reading().midi);  // A2
}

static void test_smooths_jitter() {
    tracker::Tracker t;
    srand(7);
    uint32_t now = 0;
    float worst = 0;
    for (int i = 0; i < 80; i++, now += HOP_MS) {
        float jitter = ((rand() / (float)RAND_MAX) * 2 - 1) * 6;  // +-6 cents raw
        t.update(voiced(centsSharp(41.2034f, jitter)), now);
        if (i > 20) worst = fmaxf(worst, fabsf(t.reading().cents));
    }
    printf("raw jitter +-6 c -> displayed within +-%.2f c\n", worst);
    TEST_ASSERT_LESS_THAN_FLOAT(2.0f, worst);
}

static void test_ignores_lone_octave_blip() {
    tracker::Tracker t;
    uint32_t now = feed(t, voiced(82.41f), 20, 0);
    now = feed(t, voiced(164.8f), 1, now);  // one frame an octave up
    TEST_ASSERT_EQUAL_INT(40, t.reading().midi);  // still E2
    now = feed(t, voiced(82.41f), 1, now);
    TEST_ASSERT_EQUAL_INT(40, t.reading().midi);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0, t.reading().cents);
}

static void test_switches_to_a_new_note_that_persists() {
    tracker::Tracker t;
    uint32_t now = feed(t, voiced(82.41f), 20, 0);  // E2
    now = feed(t, voiced(110.0f), 2, now);          // A2, not yet confirmed
    TEST_ASSERT_EQUAL_INT(40, t.reading().midi);
    feed(t, voiced(110.0f), 1, now);
    TEST_ASSERT_EQUAL_INT(45, t.reading().midi);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0, t.reading().cents);  // no lag from the old note
}

static void test_holds_then_goes_idle() {
    tracker::Tracker t;
    uint32_t now = feed(t, voiced(55), 10, 0);
    now = feed(t, silent(), 2, now);  // 50 ms: brief dropout, still live
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Live);
    now = feed(t, silent(), 8, now);  // 250 ms: held
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Held);
    TEST_ASSERT_EQUAL_INT(33, t.reading().midi);  // still shows A1
    feed(t, silent(), 60, now);  // 1.75 s: idle
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Idle);
}

static void test_held_note_resumes() {
    tracker::Tracker t;
    uint32_t now = feed(t, voiced(55), 10, 0);
    now = feed(t, silent(), 10, now);
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Held);
    feed(t, voiced(55), 1, now);
    TEST_ASSERT_TRUE(t.reading().state == tracker::State::Live);
}

static void test_in_tune_after_settling() {
    tracker::Tracker t;
    uint32_t now = feed(t, voiced(centsSharp(110, 2)), 8, 0);  // 200 ms
    TEST_ASSERT_FALSE(t.reading().inTune);
    feed(t, voiced(centsSharp(110, 2)), 12, now);  // 500 ms in total
    TEST_ASSERT_TRUE(t.reading().inTune);
}

static void test_not_in_tune_when_off() {
    tracker::Tracker t;
    feed(t, voiced(centsSharp(110, 10)), 40, 0);
    TEST_ASSERT_FALSE(t.reading().inTune);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 10, t.reading().cents);
}

static void test_in_tune_has_hysteresis() {
    tracker::Tracker t;
    uint32_t now = feed(t, voiced(110), 20, 0);
    TEST_ASSERT_TRUE(t.reading().inTune);
    now = feed(t, voiced(centsSharp(110, 4)), 20, now);  // between 3 and 5: stays in tune
    TEST_ASSERT_TRUE(t.reading().inTune);
    feed(t, voiced(centsSharp(110, 8)), 20, now);  // past 5: out
    TEST_ASSERT_FALSE(t.reading().inTune);
}

static void test_a4_reference() {
    tracker::Config cfg;
    cfg.a4 = 432;
    tracker::Tracker t(cfg);
    feed(t, voiced(432), 10, 0);
    TEST_ASSERT_EQUAL_INT(69, t.reading().midi);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0, t.reading().cents);
    t.setA4(440);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -31.77f, t.reading().cents);
}

// Full pipeline on a real recording at the device's 25 ms hop: once the note
// has settled, the displayed cents must move smoothly, never jump.
static void checkRecording(const char* file, int expectedMidi, float settleS, float endS) {
    std::vector<int16_t> s = readWav(file);
    TEST_ASSERT_TRUE_MESSAGE(!s.empty(), file);
    pitch::Config pcfg;
    pcfg.minRms = 10;
    pitch::Yin yin(pcfg);
    tracker::Tracker t;
    float prev = NAN, worstStep = 0;
    int wrongNote = 0, frames = 0;
    for (size_t end = yin.frameSize(); end <= s.size() && end <= endS * 16000; end += 400) {
        const uint32_t now = end / 16;
        const tracker::Reading& r = t.update(yin.detect(&s[end - yin.frameSize()], yin.frameSize()), now);
        if (now < settleS * 1000) continue;
        frames++;
        if (r.state == tracker::State::Idle || r.midi != expectedMidi) {
            wrongNote++;
            prev = NAN;
            continue;
        }
        if (!isnan(prev)) worstStep = fmaxf(worstStep, fabsf(r.cents - prev));
        prev = r.cents;
    }
    printf("%s: %d/%d frames on the right note, worst frame-to-frame step %.2f c\n", file,
           frames - wrongNote, frames, worstStep);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, wrongNote, file);
    TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(1.5f, worstStep, file);
}

static void test_recording_bass_e() { checkRecording("speaker_bass_e.wav", 28, 0.8f, 3.0f); }
static void test_recording_bass_a() { checkRecording("speaker_bass_a.wav", 33, 0.8f, 3.0f); }
static void test_recording_bass_d() { checkRecording("speaker_bass_d.wav", 38, 1.0f, 2.15f); }
static void test_recording_bass_g() { checkRecording("speaker_bass_g.wav", 43, 0.8f, 3.0f); }

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_idle);
    RUN_TEST(test_new_note_needs_confirmation);
    RUN_TEST(test_smooths_jitter);
    RUN_TEST(test_ignores_lone_octave_blip);
    RUN_TEST(test_switches_to_a_new_note_that_persists);
    RUN_TEST(test_holds_then_goes_idle);
    RUN_TEST(test_held_note_resumes);
    RUN_TEST(test_in_tune_after_settling);
    RUN_TEST(test_not_in_tune_when_off);
    RUN_TEST(test_in_tune_has_hysteresis);
    RUN_TEST(test_a4_reference);
    RUN_TEST(test_recording_bass_e);
    RUN_TEST(test_recording_bass_a);
    RUN_TEST(test_recording_bass_d);
    RUN_TEST(test_recording_bass_g);
    return UNITY_END();
}
