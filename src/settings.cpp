#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

#include "theme.h"

namespace settings {

namespace {

constexpr const char* NAMESPACE = "cardtuner";
constexpr const char* KEY = "settings";
constexpr uint8_t VERSION = 1;  // bump when Settings changes shape
constexpr uint32_t SAVE_DELAY_MS = 2000;

struct Stored {
    uint8_t version;
    Settings values;
};

Settings current;
bool dirty = false;
uint32_t changedAt = 0;

// Repair anything out of range (e.g. a preset list that got shorter).
void sanitise(Settings& s) {
    if (s.accent >= theme::ACCENT_COUNT) s.accent = theme::DEFAULT_ACCENT;
    if (s.a4 < A4_MIN || s.a4 > A4_MAX) s.a4 = 440;
    if (s.instrument > 1) s.instrument = 0;
    if (s.guitarPreset >= tuning::presetCount(tuning::Instrument::Guitar)) s.guitarPreset = 0;
    if (s.bassPreset >= tuning::presetCount(tuning::Instrument::Bass)) s.bassPreset = 0;
    if (s.micGain > 10) s.micGain = 7;
    if (s.screenOff >= SCREEN_OFF_COUNT) s.screenOff = Settings().screenOff;
}

}  // namespace

Settings& get() { return current; }

void load() {
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) return;  // nothing saved yet: keep defaults
    // Saved settings may be shorter than Settings (saved before a field was
    // added); the missing fields keep their defaults.
    Stored stored{VERSION, Settings()};
    const size_t len = prefs.getBytesLength(KEY);
    if (len > sizeof(stored.version) && len <= sizeof(stored) && prefs.getBytes(KEY, &stored, len) == len &&
        stored.version == VERSION) {
        current = stored.values;
        sanitise(current);
    }
    prefs.end();
}

void changed() {
    dirty = true;
    changedAt = millis();
}

void service() {
    if (!dirty || millis() - changedAt < SAVE_DELAY_MS) return;
    dirty = false;
    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) return;
    Stored stored{VERSION, current};
    prefs.putBytes(KEY, &stored, sizeof(stored));
    prefs.end();
}

}  // namespace settings
