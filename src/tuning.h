// Frequency <-> note conversions and tuning presets. No hardware dependencies.

#pragma once
#include <stddef.h>

namespace tuning {

struct Note {
    int midi = 0;        // nearest MIDI note number (A4 = 69, low E on bass = 28)
    float cents = 0;     // offset from that note, -50..+50
    const char* name;    // "C", "C#", ... "B"
    int octave = 0;      // scientific pitch octave (A4 -> 4)
};

// Nearest note to `hz`, given the A4 reference pitch.
Note fromHz(float hz, float a4 = 440.0f);

// Frequency of a MIDI note.
float hzOf(int midi, float a4 = 440.0f);

// Note name of a MIDI note, without octave: sharps ("D#") by default, or
// flats ("Eb") for tunings that are written with flats.
const char* nameOf(int midi, bool flats = false);

// Scientific pitch octave of a MIDI note.
int octaveOf(int midi);

// ---- Presets ----------------------------------------------------------------

enum class Instrument { Guitar, Bass };

constexpr size_t MAX_STRINGS = 7;

struct Preset {
    const char* name;       // full name for settings ("E Standard")
    const char* shortName;  // for the tuner header ("E STD")
    size_t count;
    int strings[MAX_STRINGS];  // MIDI notes, lowest string first
    bool flats;                // spell notes with flats (Eb standard)
};

size_t presetCount(Instrument instrument);
const Preset& preset(Instrument instrument, size_t index);
const char* instrumentName(Instrument instrument);

// Lowest note the detector needs to search for this instrument (Hz).
float lowestHz(Instrument instrument);

// Index of the string nearest to `midi` (a fractional MIDI note), or -1 if
// the note is more than `maxCents` from every string.
int nearestString(const Preset& preset, float midi, float maxCents = 600);

}  // namespace tuning
