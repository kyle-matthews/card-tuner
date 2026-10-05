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
uint16_t full = 0, half = 0, fifth = 0;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t scaled(const Accent& a, unsigned percent) {
    return rgb565(a.r * percent / 100, a.g * percent / 100, a.b * percent / 100);
}

struct Init {
    Init() { setAccent(0); }
} init;

}  // namespace

void setAccent(size_t index) {
    current = index % ACCENT_COUNT;
    const Accent& a = ACCENTS[current];
    full = scaled(a, 100);
    half = scaled(a, 50);
    fifth = scaled(a, 22);
}

size_t accentIndex() { return current; }

uint16_t bg() { return 0x0000; }
uint16_t accent() { return full; }
uint16_t dim() { return half; }
uint16_t faint() { return fifth; }

}  // namespace theme
