// One accent colour on black. Every screen draws with these four shades, so
// changing the accent recolours the whole app.

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

// RGB565 colours for the current accent.
uint16_t bg();      // black
uint16_t accent();  // full: note, needle, note#
uint16_t dim();     // ~50%: readouts, ticks
uint16_t faint();   // ~20%: gridlines, inactive things

}  // namespace theme
