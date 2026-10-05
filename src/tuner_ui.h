// The tuner screen: note name, cents, frequency and a needle meter, plus a
// reserved spot for note# the cat (Milestone 6).

#pragma once
#include <M5GFX.h>

#include "tracker.h"

namespace tuner_ui {

struct Status {
    const char* mode = "CHROMATIC";  // shown top left
    float a4 = 440;
    String toast;                    // brief message top right (e.g. "+21dB"), empty for none
    String footer;                   // bottom line hint or status
    bool footerActive = false;       // highlight the footer (e.g. while recording)
};

void draw(M5Canvas& canvas, const tracker::Reading& reading, const Status& status);

}  // namespace tuner_ui
