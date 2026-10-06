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

struct Settings {
    uint8_t accent = theme::DEFAULT_ACCENT;  // theme::ACCENTS index
    uint16_t a4 = 440;           // Hz
    uint8_t instrument = 0;      // tuning::Instrument
    uint8_t guitarPreset = 0;    // tuning::preset index, remembered per instrument
    uint8_t bassPreset = 0;
    bool chromatic = false;      // ignore the preset and show any note
    uint8_t micGain = 7;         // ES8311 PGA step, 3 dB each
    bool showCat = true;

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
