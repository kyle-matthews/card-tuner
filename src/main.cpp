// card-tuner — Milestone 1: mic level meter and raw sample dump.
//
// Shows the mic level (RMS and peak), a live waveform, and lets you adjust the
// codec's preamp gain and high-pass filter. Pressing `r` (or sending `r` over
// serial) captures a couple of seconds of audio and streams it over serial for
// tools/capture.py to save as a WAV file.

#include <M5Cardputer.h>
#include <math.h>

#include "audio_in.h"

static M5Canvas canvas(&M5Cardputer.Display);

// Placeholder for the theme system (Milestone 4): one accent colour on black.
static constexpr uint16_t BG = TFT_BLACK;
static constexpr uint16_t ACCENT = TFT_WHITE;
static constexpr uint16_t DIM = 0x7BEF;    // ~50% grey
static constexpr uint16_t FAINT = 0x31A6;  // ~20% grey

static constexpr size_t ANALYSIS_LEN = 1024;  // 64 ms at 16 kHz
static int16_t analysis[ANALYSIS_LEN];

struct Level {
    float rmsDb = -120;
    int peak = 0;
    float dc = 0;
};
static Level level;
static float peakHoldDb = -120;

// ---- Sample dump ----------------------------------------------------------

struct Dump {
    int16_t* buf = nullptr;
    size_t len = 0;
    size_t got = 0;
    uint32_t start = 0;
    bool active = false;
};
static Dump dump;
static String dumpStatus;

static void startDump() {
    if (dump.active) return;
    if (!Serial) {
        dumpStatus = "no serial host";
        return;
    }
    static constexpr size_t WANT[] = {48000, 32000};  // 3 s, or 2 s if short on RAM
    for (size_t want : WANT) {
        dump.buf = (int16_t*)malloc(want * sizeof(int16_t));
        if (dump.buf) {
            dump.len = want;
            break;
        }
    }
    if (!dump.buf) {
        dumpStatus = "out of memory";
        return;
    }
    dump.got = 0;
    dump.start = audio_in::sampleCount();
    dump.active = true;
    dumpStatus = "recording...";
}

static void sendDump() {
    Serial.printf("#DUMP_BEGIN rate=%lu samples=%u pga=%u hpf=%d\n",
                  (unsigned long)audio_in::SAMPLE_RATE, (unsigned)dump.len,
                  (unsigned)audio_in::pgaStep(), (int)audio_in::highPass());
    char line[16 * 7 + 2];
    for (size_t i = 0; i < dump.len; i += 16) {
        int n = 0;
        for (size_t j = i; j < i + 16 && j < dump.len; j++) {
            n += snprintf(line + n, sizeof(line) - n, j == i ? "%d" : ",%d", dump.buf[j]);
        }
        Serial.println(line);
    }
    Serial.println("#DUMP_END");
}

static void serviceDump() {
    if (!dump.active) return;
    size_t ready = audio_in::sampleCount() - (dump.start + dump.got);
    size_t take = min(ready, dump.len - dump.got);
    take = min(take, (size_t)2048);
    if (take && !audio_in::copy(dump.start + dump.got, dump.buf + dump.got, take)) {
        dumpStatus = "fell behind, try again";
        free(dump.buf);
        dump.buf = nullptr;
        dump.active = false;
        return;
    }
    dump.got += take;
    if (dump.got == dump.len) {
        dumpStatus = "sending...";
        sendDump();
        dumpStatus = String("sent ") + dump.len + " samples";
        free(dump.buf);
        dump.buf = nullptr;
        dump.active = false;
    }
}

// ---- Analysis & drawing ---------------------------------------------------

static void measure() {
    if (!audio_in::latest(analysis, ANALYSIS_LEN)) return;
    double sum = 0;
    for (int16_t s : analysis) sum += s;
    float mean = sum / ANALYSIS_LEN;
    double sq = 0;
    int peak = 0;
    for (int16_t s : analysis) {
        float v = s - mean;
        sq += v * v;
        peak = max(peak, abs((int)s));
    }
    float rms = sqrt(sq / ANALYSIS_LEN);
    level.rmsDb = rms > 0 ? 20 * log10f(rms / 32768.0f) : -120;
    level.peak = peak;
    level.dc = mean;

    peakHoldDb = max(peakHoldDb - 0.5f, level.rmsDb);
}

static void drawMeter(int x, int y, int w, int h) {
    constexpr float FLOOR_DB = -90;
    auto toX = [&](float db) {
        float t = (db - FLOOR_DB) / -FLOOR_DB;
        return x + (int)(constrain(t, 0.0f, 1.0f) * w);
    };
    canvas.drawRect(x - 1, y - 1, w + 2, h + 2, FAINT);
    canvas.fillRect(x, y, toX(level.rmsDb) - x, h, ACCENT);
    canvas.drawFastVLine(toX(peakHoldDb), y, h, DIM);
    for (int db = -80; db < 0; db += 10) canvas.drawFastVLine(toX(db), y + h, 3, FAINT);
}

static void drawWaveform(int x, int y, int w, int h) {
    int mid = y + h / 2;
    canvas.drawFastHLine(x, mid, w, FAINT);
    int step = ANALYSIS_LEN / w;  // decimate to fit
    if (step < 1) step = 1;
    // Auto-scale so quiet signals are still visible.
    float scale = 32768.0f / max(level.peak * 1.2f, 256.0f);
    int prevY = mid;
    for (int i = 0; i < w; i++) {
        float v = (analysis[i * step] - level.dc) / 32768.0f;
        int py = mid - (int)(v * scale * (h / 2));
        py = constrain(py, y, y + h - 1);
        if (i) canvas.drawLine(x + i - 1, prevY, x + i, py, ACCENT);
        prevY = py;
    }
}

static void draw() {
    canvas.fillSprite(BG);
    canvas.setTextSize(1);

    canvas.setTextColor(ACCENT, BG);
    canvas.setCursor(4, 3);
    canvas.print("M1 mic test");
    canvas.setTextColor(DIM, BG);
    canvas.setCursor(130, 3);
    canvas.printf("PGA +%2udB  HPF %s", audio_in::pgaStep() * 3,
                  audio_in::highPass() ? "on" : "off");

    canvas.setTextColor(ACCENT, BG);
    canvas.setCursor(4, 16);
    canvas.printf("rms %6.1f dBFS  peak %5d%s", level.rmsDb, level.peak,
                  level.peak >= 32000 ? " CLIP" : "");
    drawMeter(4, 28, 232, 10);

    drawWaveform(4, 44, 232, 62);

    canvas.setTextColor(DIM, BG);
    canvas.setCursor(4, 112);
    canvas.print("-/= gain  h hpf  r record");
    canvas.setCursor(4, 124);
    canvas.setTextColor(dump.active ? ACCENT : DIM, BG);
    canvas.print(dumpStatus.length() ? dumpStatus : String("ready"));

    canvas.pushSprite(0, 0);
}

// ---- Input ----------------------------------------------------------------

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
        case 'h':
            audio_in::setHighPass(!audio_in::highPass());
            break;
        case 'r':
            startDump();
            break;
    }
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = enable keyboard
    Serial.begin(115200);

    M5Cardputer.Display.setRotation(1);
    canvas.setColorDepth(16);
    canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());

    if (!audio_in::begin()) dumpStatus = "mic failed to start";
    Serial.println("card-tuner M1 started");
}

void loop() {
    M5Cardputer.update();
    audio_in::poll();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        for (char c : M5Cardputer.Keyboard.keysState().word) handleKey(c);
    }
    while (Serial.available()) handleKey(Serial.read());

    serviceDump();

    static uint32_t lastDraw = 0;
    if (millis() - lastDraw >= 33) {
        lastDraw = millis();
        measure();
        draw();
    }
}
