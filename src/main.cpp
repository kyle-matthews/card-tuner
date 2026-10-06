// card-tuner — a guitar and bass tuner for the M5Stack Cardputer ADV.
//
// Mic -> YIN detector every 25 ms -> tracker (smoothing, outlier rejection,
// hold, in-tune) -> tuner screen in one accent colour on black.
//
// Tuner keys: g/b guitar/bass, , / previous/next tuning, c chromatic,
// -/= A4 pitch, s settings, n say hi to note#, r record a WAV for
// tools/capture.py.
// Serial: `l` toggles a per-frame detector log, `s` sends a screenshot
// (tools/screenshot.py), `d` cycles demo readings, `S` opens settings, `H`
// replays the splash, `R` resets settings to defaults; any other character
// acts as that key on the keyboard.

#include <M5Cardputer.h>
#include <math.h>

#include "audio_in.h"
#include "cat.h"
#include "debug_dump.h"
#include "pitch.h"
#include "settings.h"
#include "settings_ui.h"
#include "theme.h"
#include "tracker.h"
#include "tuner_ui.h"
#include "tuning.h"

static M5Canvas canvas(&M5Cardputer.Display);

static constexpr uint32_t HOP_SAMPLES = 400;  // analyse every 25 ms
static constexpr uint32_t TOAST_MS = 1500;

static pitch::Yin* yin = nullptr;  // rebuilt when the instrument changes
static float yinMinHz = 0;
static tracker::Tracker noteTracker;
static int16_t* frame = nullptr;
static uint32_t nextFrameEnd = 0;

enum class Screen { Tuner, Settings };
static Screen screen = Screen::Tuner;

static uint32_t tunedStrings = 0;  // strings that have been in tune since the preset changed
static uint32_t detectMicros = 0;
static bool serialLog = false;
static String toast;
static uint32_t toastUntil = 0;

static void showToast(const String& text) {
    toast = text;
    toastUntil = millis() + TOAST_MS;
}

// ---- Screen off ---------------------------------------------------------------
// The backlight is the biggest power draw, so the screen turns off after a
// while with no key presses and no notes. The mic keeps listening: playing a
// note, or pressing a key, turns it back on.

static bool screenOn = true;
static uint32_t lastActivityMs = 0;
static uint8_t screenBrightness = 0;

static void screenWake() {
    lastActivityMs = millis();
    if (screenOn) return;
    screenOn = true;
    M5Cardputer.Display.wakeup();
    M5Cardputer.Display.setBrightness(screenBrightness);
    Serial.println("screen on");
}

static void screenSleepIfIdle() {
    const uint32_t timeout = settings::SCREEN_OFF_OPTIONS[settings::get().screenOff].ms;
    if (!screenOn || !timeout || millis() - lastActivityMs < timeout) return;
    screenOn = false;
    screenBrightness = M5Cardputer.Display.getBrightness();
    M5Cardputer.Display.setBrightness(0);
    M5Cardputer.Display.sleep();
    Serial.println("screen off");
}

// ---- Settings ---------------------------------------------------------------

static const tuning::Preset* activePreset() {
    const settings::Settings& s = settings::get();
    return s.chromatic ? nullptr : &s.preset();
}

static String modeLabel() {
    const settings::Settings& s = settings::get();
    String label = s.inst() == tuning::Instrument::Guitar ? "GUITAR" : "BASS";
    label += "  ";
    label += s.chromatic ? "CHROMATIC" : s.preset().shortName;
    return label;
}

// Push the current settings into every subsystem.
static void applySettings() {
    const settings::Settings& s = settings::get();
    theme::setAccent(s.accent);
    noteTracker.setA4(s.a4);
    if (audio_in::pgaStep() != s.micGain) audio_in::setPgaStep(s.micGain);

    // Search only as low as the instrument needs: guitar frames are much
    // shorter, so they are faster and less prone to low-octave mistakes.
    const float minHz = tuning::lowestHz(s.inst());
    if (minHz != yinMinHz) {
        pitch::Config cfg;
        cfg.minHz = minHz;
        cfg.minRms = 20;  // quiet-room noise floor is ~8 at +21 dB
        delete yin;
        yin = new pitch::Yin(cfg);
        yinMinHz = minHz;
    }
}

static void settingsChanged(bool resetTuned = true) {
    if (resetTuned) tunedStrings = 0;
    applySettings();
    settings::changed();
}

// ---- Analysis ---------------------------------------------------------------

static void analyse() {
    const size_t len = yin->frameSize();
    const uint32_t count = audio_in::sampleCount();
    if (count < len) return;
    if (count < nextFrameEnd) return;
    // If we fell behind, skip ahead to the newest audio rather than catch up.
    const uint32_t end = (count - nextFrameEnd > HOP_SAMPLES) ? count : nextFrameEnd;
    nextFrameEnd = end + HOP_SAMPLES;
    if (!audio_in::copy(end - len, frame, len)) return;

    const uint32_t t0 = micros();
    const pitch::Result raw = yin->detect(frame, len);
    detectMicros = micros() - t0;
    const tracker::Reading& shown = noteTracker.update(raw, millis());
    if (shown.state == tracker::State::Live) screenWake();  // playing counts as activity

    const tuner_ui::Target target = tuner_ui::targetFor(shown, activePreset());
    if (target.inTune && target.string >= 0) tunedStrings |= 1u << target.string;

    if (serialLog && !debug_dump::active()) {
        Serial.printf("%lu ", (unsigned long)millis());
        if (raw.voiced) {
            const tuning::Note n = tuning::fromHz(raw.hz, settings::get().a4);
            Serial.printf("raw %8.3f Hz %-2s%d %+6.1f c conf %.2f", raw.hz, n.name, n.octave, n.cents,
                          raw.confidence);
        } else {
            Serial.printf("raw %-38s", "--");
        }
        static const char* const STATES[] = {"idle", "live", "held"};
        Serial.printf(" rms %6.1f %4lu us | %s %-2s %+5.1f c%s\n", raw.rms, (unsigned long)detectMicros,
                      STATES[(int)shown.state], shown.state == tracker::State::Idle ? "" : tuning::nameOf(target.midi, target.flats),
                      target.cents, target.inTune ? " IN TUNE" : "");
    }
}

// Boot-time check that the detector gives the right answer at a usable speed
// on this hardware: synthetic plucked-string tones, results over serial.
static void selfTest() {
    pitch::Config cfg;  // full range, the slowest case
    pitch::Yin test(cfg);
    const float notes[] = {41.20f, 82.41f, 329.63f};
    for (float hz : notes) {
        for (size_t i = 0; i < test.frameSize(); i++) {
            float v = 0;
            for (int k = 1; k <= 8; k++) v += sinf(2 * PI * hz * k * i / audio_in::SAMPLE_RATE) / k;
            frame[i] = (int16_t)(v * 6000);
        }
        const uint32_t t0 = micros();
        const pitch::Result r = test.detect(frame, test.frameSize());
        const uint32_t us = micros() - t0;
        Serial.printf("self-test %7.2f Hz -> %8.3f Hz (%+.2f c) in %lu us\n", hz, r.hz,
                      r.voiced ? 1200 * log2f(r.hz / hz) : 0.0f, (unsigned long)us);
    }
}

// ---- Demo readings (for screenshots without live audio) -------------------

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
        case 1: return reading(State::Live, 82.45f, 40, 1.0f, true);     // low E in tune
        case 2: return reading(State::Live, 108.0f, 45, -31.8f, false);  // A string flat
        case 3: return reading(State::Live, 141.0f, 49, 30.0f, false);   // D string, way flat
        case 4: return reading(State::Live, 197.6f, 55, 14.0f, false);   // G string sharp
        case 5: return reading(State::Held, 247.2f, 59, 2.0f, false);    // B string, held
        default: return tracker::Reading();
    }
}

// ---- Input ------------------------------------------------------------------

static void handleTunerKey(char c) {
    settings::Settings& s = settings::get();
    switch (c) {
        case 'g':
        case 'b': {
            const tuning::Instrument inst = c == 'g' ? tuning::Instrument::Guitar : tuning::Instrument::Bass;
            if (s.inst() != inst) {
                s.instrument = (uint8_t)inst;
                settingsChanged();
            }
            showToast(tuning::instrumentName(inst));
            break;
        }
        case ',':
        case '/': {
            const int count = tuning::presetCount(s.inst());
            s.presetIndex() = (s.presetIndex() + (c == '/' ? 1 : count - 1)) % count;
            s.chromatic = false;
            settingsChanged();
            showToast(s.preset().name);
            break;
        }
        case 'c':
            s.chromatic = !s.chromatic;
            settingsChanged();
            showToast(s.chromatic ? "Chromatic" : s.preset().name);
            break;
        case '-':
        case '_':
        case '=':
        case '+':
            s.a4 = constrain(s.a4 + ((c == '-' || c == '_') ? -1 : 1), settings::A4_MIN, settings::A4_MAX);
            settingsChanged(false);
            showToast(String("A4 = ") + s.a4 + " Hz");
            break;
        case 's':
            settings_ui::open();
            screen = Screen::Settings;
            break;
        case 'n': {
            static const char* const SAYINGS[] = {
                "hi!",
                "mrrp!",
                "purr",
                "meow#",
                "tune me!",
                "Dare ya to play Stairway",
                "Palm muting is rad",
                "Bet you're glad you don't own a floating tremolo",
                "That's not out of tune, that's jazz",
                "Wonderwall? Again?",
                "Change your strings. I can smell them.",
                "One more pedal won't hurt",
                "Turn it up to 11",
                "Drop D: for when E is too much effort",
                "Bassists are people too",
                "Solos are just scales with attitude",
                "Free Bird!",
                "I'd tune too, but no thumbs",
                "Less tuning, more shredding",
            };
            constexpr size_t COUNT = sizeof(SAYINGS) / sizeof(SAYINGS[0]);
            // Random, but never the same one twice in a row.
            static size_t last = COUNT;
            size_t pick;
            do {
                pick = esp_random() % COUNT;
            } while (pick == last);
            last = pick;
            cat::say(SAYINGS[pick], millis());
            break;
        }
        case 'r':
            debug_dump::start();
            break;
    }
}

static void handleKey(char c) {
    if (screen == Screen::Settings) {
        switch (settings_ui::handleKey(c)) {
            case settings_ui::Action::Changed:
                settingsChanged();
                break;
            case settings_ui::Action::Close:
                screen = Screen::Tuner;
                break;
            case settings_ui::Action::None:
                break;
        }
        return;
    }
    handleTunerKey(c);
}

static void splash();

static void handleSerial(char c) {
    switch (c) {
        case 'l':
            serialLog = !serialLog;
            break;
        case 's':
            debug_dump::screenshot(canvas.getBuffer(), canvas.width(), canvas.height());
            break;
        case 'd':
            demoIndex = (demoIndex + 1) % 6;
            break;
        case 'S':
            handleKey('s');
            break;
        case 'H':
            splash();
            break;
        case 'R':  // reset all settings to defaults
            settings::get() = settings::Settings();
            settingsChanged();
            showToast("Defaults");
            break;
        default:
            handleKey(c);
    }
}

// ---- Main -------------------------------------------------------------------

// Boot splash: note# wakes up and says hi. Any key skips it.
static void splash() {
    constexpr uint32_t DURATION_MS = 2000, HI_AT_MS = 600;
    constexpr int SCALE = 3, CAT_X = (240 - 24 * SCALE) / 2, CAT_Y = 8;
    const uint32_t start = millis();
    bool saidHi = false;
    while (millis() - start < DURATION_MS) {
        const uint32_t now = millis();
        if (!saidHi && now - start >= HI_AT_MS) {
            cat::say("hi!", now);
            saidHi = true;
        }
        canvas.fillSprite(theme::bg());
        cat::draw(canvas, CAT_X, CAT_Y, SCALE, saidHi ? cat::Mood::Happy : cat::Mood::Idle, now);
        cat::drawBubble(canvas, CAT_X, CAT_Y, SCALE, now);
        canvas.setFont(&fonts::Font0);
        canvas.setTextSize(2);
        canvas.setTextDatum(bottom_center);
        canvas.setTextColor(saidHi ? theme::accent() : theme::dim());
        canvas.drawString("note# says hi", 120, 126);
        canvas.pushSprite(0, 0);

        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) break;
        if (Serial.available()) {
            // A serial `s` screenshots the splash; anything else skips it.
            if (Serial.read() != 's') break;
            debug_dump::screenshot(canvas.getBuffer(), canvas.width(), canvas.height());
        }
        delay(16);
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

    // Big enough for the longest frame (full bass range).
    {
        pitch::Yin widest{pitch::Config()};
        frame = (int16_t*)malloc(widest.frameSize() * sizeof(int16_t));
    }
    selfTest();

    settings::load();
    applySettings();
    // The mic warms up for about a second after power-up, so start it before
    // the splash and let the splash cover the wait.
    if (!audio_in::begin()) Serial.println("mic failed to start");
    audio_in::setPgaStep(settings::get().micGain);
    if (settings::get().showCat) splash();
    lastActivityMs = millis();
    nextFrameEnd = yin->frameSize();
    Serial.printf("card-tuner started: %s, A4 %u Hz\n", modeLabel().c_str(), settings::get().a4);
}

void loop() {
    M5Cardputer.update();
    audio_in::poll();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        const bool wasOn = screenOn;
        screenWake();
        if (wasOn) {  // the key that wakes the screen does nothing else
            const auto keys = M5Cardputer.Keyboard.keysState();
            for (char c : keys.word) handleKey(c);
            if (keys.enter) handleKey('\n');
        }
    }
    while (Serial.available()) {
        const char c = Serial.read();
        if (c != 's' && c != 'l') screenWake();  // screenshots and logging don't wake it
        handleSerial(c);
    }
    screenSleepIfIdle();

    debug_dump::service();
    settings::service();
    analyse();

    static uint32_t lastDraw = 0;
    if (screenOn && millis() - lastDraw >= 33) {
        lastDraw = millis();
        if (screen == Screen::Settings) {
            settings_ui::draw(canvas);
        } else {
            const settings::Settings& s = settings::get();
            tuner_ui::Status status;
            status.mode = modeLabel();
            status.a4 = s.a4;
            status.preset = activePreset();
            status.tunedStrings = tunedStrings;
            status.showCat = s.showCat;
            if (millis() < toastUntil) status.toast = toast;
            status.footerActive = debug_dump::active();
            status.footer = debug_dump::status().length() ? debug_dump::status() : String("s settings  c chromatic");
            tuner_ui::draw(canvas, demoIndex ? demoReading() : noteTracker.reading(), status);
        }
        canvas.pushSprite(0, 0);
    }
}
