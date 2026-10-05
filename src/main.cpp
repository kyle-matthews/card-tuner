// card-tuner — Milestone 3: tuner v1 on the device.
//
// Runs the YIN detector on the live mic every 50 ms and shows the note, its
// frequency and how many cents sharp or flat it is. Unsmoothed on purpose so
// we can see how the raw detector behaves; smoothing comes in Milestone 4.
//
// Keys: -/= mic gain, r record a WAV (tools/capture.py).
// Serial: send `l` to toggle a per-frame log of detector output.

#include <M5Cardputer.h>
#include <math.h>

#include "audio_in.h"
#include "debug_dump.h"
#include "pitch.h"
#include "tuning.h"

static M5Canvas canvas(&M5Cardputer.Display);

// Placeholder for the theme system (Milestone 4): one accent colour on black.
static constexpr uint16_t BG = TFT_BLACK;
static constexpr uint16_t ACCENT = TFT_WHITE;
static constexpr uint16_t DIM = 0x7BEF;    // ~50% grey
static constexpr uint16_t FAINT = 0x31A6;  // ~20% grey

static constexpr uint32_t HOP_SAMPLES = 800;   // analyse every 50 ms
static constexpr uint32_t HOLD_MS = 1500;      // keep showing the last note this long

static pitch::Config yinConfig() {
    pitch::Config cfg;
    cfg.minRms = 20;  // quiet-room noise floor is ~8 at +21 dB
    return cfg;
}
static pitch::Yin yin(yinConfig());
static int16_t* frame = nullptr;
static uint32_t nextFrameEnd = 0;

static pitch::Result last;          // most recent detector output
static pitch::Result lastVoiced;    // most recent voiced output
static uint32_t lastVoicedMs = 0;
static uint32_t detectMicros = 0;
static bool serialLog = false;

// ---- Analysis ---------------------------------------------------------------

static void analyse() {
    const uint32_t count = audio_in::sampleCount();
    if (count < nextFrameEnd) return;
    // If we fell behind, skip ahead to the newest audio rather than catch up.
    const uint32_t end = (count - nextFrameEnd > HOP_SAMPLES) ? count : nextFrameEnd;
    nextFrameEnd = end + HOP_SAMPLES;
    if (!audio_in::copy(end - yin.frameSize(), frame, yin.frameSize())) return;

    const uint32_t t0 = micros();
    last = yin.detect(frame, yin.frameSize());
    detectMicros = micros() - t0;

    if (last.voiced) {
        lastVoiced = last;
        lastVoicedMs = millis();
    }
    if (serialLog && !debug_dump::active()) {
        if (last.voiced) {
            tuning::Note n = tuning::fromHz(last.hz);
            Serial.printf("%lu %8.3f Hz %-2s%d %+6.1f c conf %.2f rms %6.1f %4lu us\n",
                          (unsigned long)millis(), last.hz, n.name, n.octave, n.cents, last.confidence,
                          last.rms, (unsigned long)detectMicros);
        } else {
            Serial.printf("%lu -- rms %6.1f %4lu us\n", (unsigned long)millis(), last.rms,
                          (unsigned long)detectMicros);
        }
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

// ---- Drawing ----------------------------------------------------------------

static void drawCentsBar(int x, int y, int w, int h, float cents, uint16_t colour) {
    const int mid = x + w / 2;
    canvas.drawFastHLine(x, y + h / 2, w, FAINT);
    for (int c = -50; c <= 50; c += 10) {
        const int tx = mid + c * (w / 2) / 50;
        const int th = (c == 0) ? h : (c % 50 == 0 ? h / 2 : h / 4);
        canvas.drawFastVLine(tx, y + (h - th) / 2, th, c == 0 ? DIM : FAINT);
    }
    const int mx = mid + (int)(constrain(cents, -50.0f, 50.0f) * (w / 2) / 50);
    canvas.fillRect(mx - 2, y, 5, h, colour);
}

static void draw() {
    canvas.fillSprite(BG);
    canvas.setFont(&fonts::Font0);

    canvas.setTextSize(1);
    canvas.setTextColor(DIM, BG);
    canvas.setCursor(4, 3);
    canvas.print("M3 tuner");
    canvas.setCursor(196, 3);
    canvas.printf("+%udB", audio_in::pgaStep() * 3);

    const bool holding = lastVoicedMs && millis() - lastVoicedMs < HOLD_MS;
    if (holding) {
        const tuning::Note n = tuning::fromHz(lastVoiced.hz);
        // Fresh readings in full colour; held readings dimmed.
        const uint16_t colour = last.voiced ? ACCENT : DIM;

        canvas.setTextColor(colour, BG);
        canvas.setTextSize(7);
        canvas.setCursor(8, 20);
        canvas.print(n.name);
        canvas.setTextSize(2);
        canvas.setCursor(8 + 42 * strlen(n.name) + 2, 20 + 56 - 16);
        canvas.print(n.octave);

        canvas.setTextSize(2);
        canvas.setCursor(124, 26);
        canvas.printf("%.2f Hz", lastVoiced.hz);
        canvas.setTextSize(3);
        canvas.setCursor(124, 52);
        canvas.printf("%+.0fc", n.cents);

        drawCentsBar(10, 88, 220, 14, n.cents, colour);
    } else {
        canvas.setTextColor(FAINT, BG);
        canvas.setTextSize(7);
        canvas.setCursor(8, 20);
        canvas.print("-");
        canvas.setTextSize(1);
        canvas.setTextColor(DIM, BG);
        canvas.setCursor(124, 40);
        canvas.print("play a note...");
        drawCentsBar(10, 88, 220, 14, 0, BG);
    }

    canvas.setTextSize(1);
    canvas.setTextColor(DIM, BG);
    canvas.setCursor(4, 112);
    canvas.printf("yin %.1fms  conf %.2f  rms %.0f", detectMicros / 1000.0f,
                  last.voiced ? last.confidence : 0.0f, last.rms);
    canvas.setCursor(4, 124);
    canvas.setTextColor(debug_dump::active() ? ACCENT : FAINT, BG);
    canvas.print(debug_dump::status().length() ? debug_dump::status() : String("-/= gain  r record"));

    canvas.pushSprite(0, 0);
}

// ---- Input ------------------------------------------------------------------

static void handleKey(char c) {
    switch (c) {
        case '=':
        case '+':
            audio_in::setPgaStep(audio_in::pgaStep() + 1);
            break;
        case '-':
        case '_':
            if (audio_in::pgaStep() > 0) audio_in::setPgaStep(audio_in::pgaStep() - 1);
            break;
        case 'r':
            debug_dump::start();
            break;
        case 'l':
            serialLog = !serialLog;
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
    Serial.printf("card-tuner M3 started, frame %u samples\n", (unsigned)yin.frameSize());
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
        draw();
    }
}
