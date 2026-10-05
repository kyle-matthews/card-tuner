// card-tuner — Milestone 4: the tuner UI.
//
// Mic -> YIN detector every 25 ms -> tracker (smoothing, outlier rejection,
// hold, in-tune) -> tuner screen in one accent colour on black.
//
// Keys: -/= mic gain, c cycle accent colour, r record a WAV (tools/capture.py).
// Serial: `l` toggles a per-frame log of detector and tracker output, `s` sends
// a screenshot (tools/screenshot.py), `d` cycles demo readings for screenshots.

#include <M5Cardputer.h>
#include <math.h>

#include "audio_in.h"
#include "debug_dump.h"
#include "pitch.h"
#include "theme.h"
#include "tracker.h"
#include "tuner_ui.h"
#include "tuning.h"

static M5Canvas canvas(&M5Cardputer.Display);

static constexpr uint32_t HOP_SAMPLES = 400;  // analyse every 25 ms
static constexpr uint32_t TOAST_MS = 1500;

static pitch::Config yinConfig() {
    pitch::Config cfg;
    cfg.minRms = 20;  // quiet-room noise floor is ~8 at +21 dB
    return cfg;
}
static pitch::Yin yin(yinConfig());
static tracker::Tracker noteTracker;
static int16_t* frame = nullptr;
static uint32_t nextFrameEnd = 0;

static uint32_t detectMicros = 0;
static bool serialLog = false;
static String toast;
static uint32_t toastUntil = 0;

// Demo readings, so every UI state can be screenshotted without live audio.
static int demoIndex = 0;  // 0 = off
static tracker::Reading reading(tracker::State state, float hz, int midi, float cents, bool inTune) {
    tracker::Reading r;
    r.state = state;
    r.hz = hz;
    r.midi = midi;
    r.cents = cents;
    r.inTune = inTune;
    return r;
}
static tracker::Reading demoReading() {
    using tracker::State;
    switch (demoIndex) {
        case 1: return reading(State::Live, 82.45f, 40, 1.0f, true);     // E2 in tune
        case 2: return reading(State::Live, 108.0f, 45, -31.8f, false);  // A2 flat
        case 3: return reading(State::Live, 41.6f, 28, 16.6f, false);    // E1 sharp
        case 4: return reading(State::Live, 116.5f, 46, -0.6f, false);   // A#2 settling
        case 5: return reading(State::Held, 55.1f, 33, 3.1f, false);     // A1 held
        default: return tracker::Reading();
    }
}

static void showToast(const String& text) {
    toast = text;
    toastUntil = millis() + TOAST_MS;
}

// ---- Analysis ---------------------------------------------------------------

static void analyse() {
    const uint32_t count = audio_in::sampleCount();
    if (count < nextFrameEnd) return;
    // If we fell behind, skip ahead to the newest audio rather than catch up.
    const uint32_t end = (count - nextFrameEnd > HOP_SAMPLES) ? count : nextFrameEnd;
    nextFrameEnd = end + HOP_SAMPLES;
    if (!audio_in::copy(end - yin.frameSize(), frame, yin.frameSize())) return;

    const uint32_t t0 = micros();
    const pitch::Result raw = yin.detect(frame, yin.frameSize());
    detectMicros = micros() - t0;
    const tracker::Reading& shown = noteTracker.update(raw, millis());

    if (serialLog && !debug_dump::active()) {
        Serial.printf("%lu ", (unsigned long)millis());
        if (raw.voiced) {
            const tuning::Note n = tuning::fromHz(raw.hz);
            Serial.printf("raw %8.3f Hz %-2s%d %+6.1f c conf %.2f", raw.hz, n.name, n.octave, n.cents,
                          raw.confidence);
        } else {
            Serial.printf("raw %-38s", "--");
        }
        static const char* const STATES[] = {"idle", "live", "held"};
        Serial.printf(" rms %6.1f %4lu us | %s %-2s %+5.1f c%s\n", raw.rms, (unsigned long)detectMicros,
                      STATES[(int)shown.state], shown.state == tracker::State::Idle ? "" : tuning::nameOf(shown.midi),
                      shown.cents, shown.inTune ? " IN TUNE" : "");
    }
}

// Boot-time check that the detector gives the right answer at a usable speed
// on this hardware: synthetic plucked-string tones, results over serial.
static void selfTest() {
    const float notes[] = {41.20f, 82.41f, 329.63f};
    for (float hz : notes) {
        for (size_t i = 0; i < yin.frameSize(); i++) {
            float v = 0;
            for (int k = 1; k <= 8; k++) v += sinf(2 * PI * hz * k * i / audio_in::SAMPLE_RATE) / k;
            frame[i] = (int16_t)(v * 6000);
        }
        const uint32_t t0 = micros();
        const pitch::Result r = yin.detect(frame, yin.frameSize());
        const uint32_t us = micros() - t0;
        Serial.printf("self-test %7.2f Hz -> %8.3f Hz (%+.2f c) in %lu us\n", hz, r.hz,
                      r.voiced ? 1200 * log2f(r.hz / hz) : 0.0f, (unsigned long)us);
    }
}

// ---- Input ------------------------------------------------------------------

static void handleKey(char c) {
    switch (c) {
        case '=':
        case '+':
            audio_in::setPgaStep(audio_in::pgaStep() + 1);
            showToast(String("mic +") + audio_in::pgaStep() * 3 + "dB");
            break;
        case '-':
        case '_':
            if (audio_in::pgaStep() > 0) audio_in::setPgaStep(audio_in::pgaStep() - 1);
            showToast(String("mic +") + audio_in::pgaStep() * 3 + "dB");
            break;
        case 'c':
            theme::setAccent(theme::accentIndex() + 1);
            showToast(theme::ACCENTS[theme::accentIndex()].name);
            break;
        case 'r':
            debug_dump::start();
            break;
        case 'l':
            serialLog = !serialLog;
            break;
        case 's':
            debug_dump::screenshot(canvas.getBuffer(), canvas.width(), canvas.height());
            break;
        case 'd':
            demoIndex = (demoIndex + 1) % 6;
            break;
    }
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = enable keyboard
    Serial.setTxBufferSize(4096);
    Serial.begin(115200);

    M5Cardputer.Display.setRotation(1);
    canvas.setColorDepth(16);
    canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());

    frame = (int16_t*)malloc(yin.frameSize() * sizeof(int16_t));
    selfTest();
    if (!audio_in::begin()) Serial.println("mic failed to start");
    nextFrameEnd = yin.frameSize();
    Serial.printf("card-tuner M4 started, frame %u samples\n", (unsigned)yin.frameSize());
}

void loop() {
    M5Cardputer.update();
    audio_in::poll();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        for (char c : M5Cardputer.Keyboard.keysState().word) handleKey(c);
    }
    while (Serial.available()) handleKey(Serial.read());

    debug_dump::service();
    analyse();

    static uint32_t lastDraw = 0;
    if (millis() - lastDraw >= 33) {
        lastDraw = millis();
        tuner_ui::Status status;
        if (millis() < toastUntil) status.toast = toast;
        status.footerActive = debug_dump::active();
        status.footer = debug_dump::status().length() ? debug_dump::status() : String("-/= mic  c colour  r rec");
        tuner_ui::draw(canvas, demoIndex ? demoReading() : noteTracker.reading(), status);
        canvas.pushSprite(0, 0);
    }
}
