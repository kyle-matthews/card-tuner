#include "tuning.h"

#include <math.h>

namespace tuning {

static const char* const NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const char* nameOf(int midi) { return NAMES[((midi % 12) + 12) % 12]; }

float hzOf(int midi, float a4) { return a4 * powf(2.0f, (midi - 69) / 12.0f); }

Note fromHz(float hz, float a4) {
    Note note;
    const float exact = 69 + 12 * log2f(hz / a4);
    note.midi = (int)lroundf(exact);
    note.cents = (exact - note.midi) * 100;
    note.name = nameOf(note.midi);
    // Integer division rounds toward zero; floor so MIDI 0..11 is octave -1.
    note.octave = (int)floorf(note.midi / 12.0f) - 1;
    return note;
}

}  // namespace tuning
