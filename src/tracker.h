// Turns the detector's raw per-frame readings into a steady display reading.
//
// Raw YIN output jitters by a few cents frame to frame, and the occasional
// frame is plain wrong (a key click, an octave blip, a second string ringing).
// The tracker:
//   - only switches to a new note once it has been seen for a few frames,
//   - ignores lone outliers far from the current note,
//   - takes a median of recent frames, then eases the display towards it,
//   - holds the last reading for a moment after the note fades,
//   - declares "in tune" only once the note has stayed close for a while.
// No hardware dependencies; unit tested on the PC (test/test_tracker).

#pragma once
#include <stddef.h>
#include <stdint.h>

#include "pitch.h"

namespace tracker {

struct Config {
    float a4 = 440;
    int confirmFrames = 3;       // frames a new note must persist before it's shown
    float jumpCents = 60;        // further than this from the current note = new note or outlier
    float smoothing = 0.2f;      // 0..1 per frame: how fast the display follows the median
    uint32_t staleMs = 150;      // no voiced frames for this long: reading is "held"
    uint32_t holdMs = 1500;      // ...and after this long, back to idle
    float inTuneCents = 3;       // within this many cents...
    uint32_t inTuneMs = 300;     // ...for this long counts as in tune
    float outOfTuneCents = 5;    // and it stays in tune until it drifts past this
};

enum class State {
    Idle,  // nothing to show
    Live,  // tracking a note that is sounding now
    Held,  // the note just stopped; showing the last reading
};

struct Reading {
    State state = State::Idle;
    float hz = 0;
    int midi = 0;      // nearest note
    float cents = 0;   // smoothed offset from that note
    bool inTune = false;
};

class Tracker {
public:
    explicit Tracker(const Config& config = Config()) : config_(config) {}

    // Feed one detector frame; returns the reading to display.
    const Reading& update(const pitch::Result& frame, uint32_t nowMs);

    const Reading& reading() const { return reading_; }
    void setA4(float a4);

private:
    static constexpr size_t MEDIAN_LEN = 7;

    float toMidi(float hz) const;
    void restartAt(float midi);
    void updateInTune(uint32_t nowMs);
    void publish();

    Config config_;
    Reading reading_;

    bool tracking_ = false;
    float smoothed_ = 0;  // fractional MIDI note number
    float history_[MEDIAN_LEN] = {};
    size_t historyCount_ = 0;
    size_t historyNext_ = 0;

    bool hasCandidate_ = false;
    float candidate_ = 0;
    int candidateFrames_ = 0;

    uint32_t lastVoicedMs_ = 0;
    uint32_t closeSinceMs_ = 0;  // when the note last came within inTuneCents
    bool close_ = false;
};

}  // namespace tracker
