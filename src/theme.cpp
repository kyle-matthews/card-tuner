#include "theme.h"

namespace theme {

const Accent ACCENTS[] = {
    {"White", 0xFF, 0xFF, 0xFF},    {"Amber", 0xFF, 0xB0, 0x00},  {"Phosphor", 0x33, 0xFF, 0x66},
    {"Cyan", 0x00, 0xE5, 0xFF},     {"Pink", 0xFF, 0x6E, 0xC7},   {"Lavender", 0xB9, 0xA3, 0xFF},
    {"Red", 0xFF, 0x40, 0x40},
};
const size_t ACCENT_COUNT = sizeof(ACCENTS) / sizeof(ACCENTS[0]);

namespace {

size_t current = 0;
bool light = false;
uint16_t background = 0, full = 0, half = 0, fifth = 0;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// `percent` of the way from colour `from` to colour `to`.
uint16_t blend(const Accent& from, const Accent& to, unsigned percent) {
    auto mix = [percent](uint8_t a, uint8_t b) { return (uint8_t)(a + ((int)b - a) * (int)percent / 100); };
    return rgb565(mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b));
}

void rebuild() {
    static const Accent BLACK = {"Black", 0, 0, 0};
    const Accent& paper = light ? ACCENTS[current] : BLACK;
    const Accent& ink = light ? BLACK : ACCENTS[current];
    background = blend(paper, ink, 0);
    full = blend(paper, ink, 100);
    half = blend(paper, ink, 50);
    fifth = blend(paper, ink, 22);
}

struct Init {
    Init() { setAccent(DEFAULT_ACCENT); }
} init;

}  // namespace

void setAccent(size_t index) {
    current = index % ACCENT_COUNT;
    rebuild();
}

size_t accentIndex() { return current; }

void setLight(bool on) {
    light = on;
    rebuild();
}

bool isLight() { return light; }

uint16_t bg() { return background; }
uint16_t accent() { return full; }
uint16_t dim() { return half; }
uint16_t faint() { return fifth; }

}  // namespace theme
