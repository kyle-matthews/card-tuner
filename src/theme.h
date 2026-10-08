// One accent colour plus black. Every screen draws with these four shades, so
// changing the accent (or flipping to light mode) recolours the whole app.
//
// Dark mode: black background, drawing in the accent colour.
// Light mode: the same scheme inverted, an accent background with black ink.

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace theme {

struct Accent {
    const char* name;
    uint8_t r, g, b;
};

extern const Accent ACCENTS[];
extern const size_t ACCENT_COUNT;
constexpr size_t DEFAULT_ACCENT = 1;  // Amber

void setAccent(size_t index);
size_t accentIndex();

void setLight(bool light);
bool isLight();

// RGB565 colours for the current accent and mode.
uint16_t bg();      // background
uint16_t accent();  // full ink: note, needle, note#
uint16_t dim();     // ~50% of the way from background to ink: readouts, ticks
uint16_t faint();   // ~20%: gridlines, inactive things

}  // namespace theme
