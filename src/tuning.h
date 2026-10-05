// Frequency <-> note conversions. No hardware dependencies.

#pragma once

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

// Note name of a MIDI note, without octave.
const char* nameOf(int midi);

}  // namespace tuning
