// User settings, saved to flash (NVS) so they survive a power cycle.
//
// Changes are written a couple of seconds after the last edit rather than on
// every key press, to spare the flash.

#pragma once
#include <stdint.h>

#include "theme.h"
#include "tuning.h"

namespace settings {

constexpr int A4_MIN = 430;
constexpr int A4_MAX = 450;

// Turn the screen off after this long without a key press or a note.
struct ScreenOffOption {
    const char* label;
    uint32_t ms;  // 0 = never
};
constexpr ScreenOffOption SCREEN_OFF_OPTIONS[] = {
    {"Never", 0}, {"30 s", 30000}, {"1 min", 60000}, {"2 min", 120000}, {"5 min", 300000},
};
constexpr size_t SCREEN_OFF_COUNT = sizeof(SCREEN_OFF_OPTIONS) / sizeof(SCREEN_OFF_OPTIONS[0]);

struct Settings {
    uint8_t accent = theme::DEFAULT_ACCENT;  // theme::ACCENTS index
    uint16_t a4 = 440;           // Hz
    uint8_t instrument = 0;      // tuning::Instrument
    uint8_t guitarPreset = 0;    // tuning::preset index, remembered per instrument
    uint8_t bassPreset = 0;
    bool chromatic = false;      // ignore the preset and show any note
    uint8_t micGain = 7;         // ES8311 PGA step, 3 dB each
    bool showCat = true;
    // New fields go at the end: older saved settings are shorter and simply
    // leave them at their defaults.
    uint8_t screenOff = 2;       // SCREEN_OFF_OPTIONS index

    tuning::Instrument inst() const { return (tuning::Instrument)instrument; }
    uint8_t& presetIndex() { return inst() == tuning::Instrument::Guitar ? guitarPreset : bassPreset; }
    const tuning::Preset& preset() const {
        return tuning::preset(inst(), inst() == tuning::Instrument::Guitar ? guitarPreset : bassPreset);
    }
};

// The live settings. Edit freely, then call changed().
Settings& get();

void load();
void changed();  // schedule a save
void service();  // call every loop: performs scheduled saves

}  // namespace settings
