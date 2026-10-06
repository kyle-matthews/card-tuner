#include "tuning.h"

#include <math.h>

namespace tuning {

static const char* const SHARP_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char* const FLAT_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

const char* nameOf(int midi, bool flats) { return (flats ? FLAT_NAMES : SHARP_NAMES)[((midi % 12) + 12) % 12]; }

// Integer division rounds toward zero; floor so MIDI 0..11 is octave -1.
int octaveOf(int midi) { return (int)floorf(midi / 12.0f) - 1; }

float hzOf(int midi, float a4) { return a4 * powf(2.0f, (midi - 69) / 12.0f); }

Note fromHz(float hz, float a4) {
    Note note;
    const float exact = 69 + 12 * log2f(hz / a4);
    note.midi = (int)lroundf(exact);
    note.cents = (exact - note.midi) * 100;
    note.name = nameOf(note.midi);
    note.octave = octaveOf(note.midi);
    return note;
}

// ---- Presets ----------------------------------------------------------------

// MIDI: E1 28, A1 33, D2 38, G2 43 | E2 40, A2 45, D3 50, G3 55, B3 59, E4 64
static const Preset GUITAR[] = {
    {"E Standard", "E STD", 6, {40, 45, 50, 55, 59, 64}},
    {"Drop D", "DROP D", 6, {38, 45, 50, 55, 59, 64}},
    {"Eb Standard", "Eb STD", 6, {39, 44, 49, 54, 58, 63}, true},
    {"D Standard", "D STD", 6, {38, 43, 48, 53, 57, 62}},
    {"Drop C", "DROP C", 6, {36, 43, 48, 53, 57, 62}},
    {"DADGAD", "DADGAD", 6, {38, 45, 50, 55, 57, 62}},
    {"Open G", "OPEN G", 6, {38, 43, 50, 55, 59, 62}},
    {"Open D", "OPEN D", 6, {38, 45, 50, 54, 57, 62}},
    {"7-string B", "7 STR", 7, {35, 40, 45, 50, 55, 59, 64}},
};

static const Preset BASS[] = {
    {"E Standard", "E STD", 4, {28, 33, 38, 43}},
    {"Drop D", "DROP D", 4, {26, 33, 38, 43}},
    {"Eb Standard", "Eb STD", 4, {27, 32, 37, 42}, true},
    {"D Standard", "D STD", 4, {26, 31, 36, 41}},
    {"5-string BEADG", "5 STR", 5, {23, 28, 33, 38, 43}},
    {"6-string BEADGC", "6 STR", 6, {23, 28, 33, 38, 43, 48}},
};

size_t presetCount(Instrument instrument) {
    return instrument == Instrument::Guitar ? sizeof(GUITAR) / sizeof(GUITAR[0])
                                            : sizeof(BASS) / sizeof(BASS[0]);
}

const Preset& preset(Instrument instrument, size_t index) {
    const size_t count = presetCount(instrument);
    const Preset* list = instrument == Instrument::Guitar ? GUITAR : BASS;
    return list[index < count ? index : 0];
}

const char* instrumentName(Instrument instrument) {
    return instrument == Instrument::Guitar ? "Guitar" : "Bass";
}

// A little below the lowest preset string (7-string B1 = 61.7 Hz, 5-string
// bass B0 = 30.9 Hz) so detuned strings still register.
float lowestHz(Instrument instrument) { return instrument == Instrument::Guitar ? 55.0f : 28.0f; }

int nearestString(const Preset& p, float midi, float maxCents) {
    int best = -1;
    float bestDist = maxCents / 100;
    for (size_t i = 0; i < p.count; i++) {
        const float dist = fabsf(midi - p.strings[i]);
        if (dist <= bestDist) {
            bestDist = dist;
            best = (int)i;
        }
    }
    return best;
}

}  // namespace tuning
