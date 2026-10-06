// note#, the tuner's cat. 1-bit pixel art in the accent colour that reacts
// to the tuning: asleep in silence, worried when flat or sharp, startled when
// far off, delighted when in tune.

#pragma once
#include <M5GFX.h>

#include "tracker.h"

namespace cat {

enum class Mood {
    Asleep,     // silence for a while
    Idle,       // awake, waiting, blinking now and then
    Listening,  // a note is close to pitch
    Flat,       // looks worriedly towards the flat side
    Sharp,      // ...and the sharp side
    Startled,   // way off (more than 50 cents from the target)
    Happy,      // in tune
    Content,    // the in-tune note just stopped ringing
};

// Feed the latest reading (cents and in-tune relative to the target string).
void update(tracker::State state, float cents, bool inTune, uint32_t nowMs);
Mood mood();

// Draw note# with its top-left corner at (x, y). Each art pixel is `scale`
// screen pixels; the art is 24x24.
void draw(M5Canvas& canvas, int x, int y, int scale, uint32_t nowMs);
void draw(M5Canvas& canvas, int x, int y, int scale, Mood mood, uint32_t nowMs);

// Speech bubble: note# says something for a moment.
void say(const char* text, uint32_t nowMs);
// Draw the bubble (if any) pointing at a cat drawn at (catX, catY) and `scale`.
void drawBubble(M5Canvas& canvas, int catX, int catY, int scale, uint32_t nowMs);

}  // namespace cat
