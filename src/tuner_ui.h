// The tuner screen: note name, cents, frequency and a needle meter, a row
// showing the preset's strings, and a spot for note# the cat (Milestone 6).

#pragma once
#include <M5GFX.h>

#include "tracker.h"
#include "tuning.h"

namespace tuner_ui {

struct Status {
    String mode = "CHROMATIC";               // shown top left
    float a4 = 440;
    const tuning::Preset* preset = nullptr;  // nullptr in chromatic mode
    uint32_t tunedStrings = 0;               // bit per string that has been in tune
    bool showCat = true;
    String toast;                            // brief message top right, empty for none
    String footer;                           // bottom line in chromatic mode
    bool footerActive = false;               // highlight the footer (e.g. while recording)
};

// What the tuner is aiming at for this reading: the nearest preset string,
// or the nearest note in chromatic mode.
struct Target {
    int string = -1;     // preset string index, -1 if none
    int midi = 0;        // note being tuned to
    float cents = 0;     // offset from it (can exceed +-50 with a preset)
    bool inTune = false;
};
Target targetFor(const tracker::Reading& reading, const tuning::Preset* preset);

void draw(M5Canvas& canvas, const tracker::Reading& reading, const Status& status);

}  // namespace tuner_ui
