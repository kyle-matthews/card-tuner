#include "settings_ui.h"

#include "settings.h"
#include "theme.h"
#include "tuning.h"

namespace settings_ui {

namespace {

enum Row { COLOUR, THEME, A4, INSTRUMENT, TUNING, MIC, CAT, SCREEN_OFF, ROW_COUNT };
const char* const LABELS[ROW_COUNT] = {"Colour",   "Theme", "A4 pitch", "Instrument",
                                       "Tuning",   "Mic gain", "note#", "Screen off"};

int selected = 0;

int wrap(int value, int count) { return ((value % count) + count) % count; }

String valueText(int row) {
    const settings::Settings& s = settings::get();
    switch (row) {
        case COLOUR: return theme::ACCENTS[s.accent].name;
        case THEME: return s.light ? "Light" : "Dark";
        case A4: return String(s.a4) + " Hz";
        case INSTRUMENT: return tuning::instrumentName(s.inst());
        case TUNING: return s.chromatic ? String("Chromatic") : String(s.preset().name);
        case MIC: return String("+") + s.micGain * 3 + " dB";
        case CAT: return s.showCat ? "Shown" : "Hidden";
        case SCREEN_OFF: return settings::SCREEN_OFF_OPTIONS[s.screenOff].label;
    }
    return "";
}

// Step the selected row's value by -1 or +1.
void step(int row, int dir) {
    settings::Settings& s = settings::get();
    switch (row) {
        case COLOUR:
            s.accent = wrap(s.accent + dir, theme::ACCENT_COUNT);
            break;
        case A4:
            s.a4 = constrain(s.a4 + dir, settings::A4_MIN, settings::A4_MAX);
            break;
        case INSTRUMENT:
            s.instrument = wrap(s.instrument + dir, 2);
            break;
        case TUNING: {
            // Chromatic sits before the first preset.
            const int count = tuning::presetCount(s.inst()) + 1;
            const int position = wrap((s.chromatic ? 0 : s.presetIndex() + 1) + dir, count);
            s.chromatic = position == 0;
            if (position) s.presetIndex() = position - 1;
            break;
        }
        case MIC:
            s.micGain = constrain(s.micGain + dir, 0, 10);
            break;
        case CAT:
            s.showCat = !s.showCat;
            break;
        case SCREEN_OFF:
            s.screenOff = wrap(s.screenOff + dir, settings::SCREEN_OFF_COUNT);
            break;
        case THEME:
            s.light = !s.light;
            break;
    }
}

}  // namespace

void open() { selected = 0; }

Action handleKey(char c) {
    switch (c) {
        case ';':
            selected = wrap(selected - 1, ROW_COUNT);
            return Action::None;
        case '.':
            selected = wrap(selected + 1, ROW_COUNT);
            return Action::None;
        case ',':
            step(selected, -1);
            return Action::Changed;
        case '/':
        case '\n':
            step(selected, +1);
            return Action::Changed;
        case '`':
        case 's':
            return Action::Close;
    }
    return Action::None;
}

void draw(M5Canvas& c) {
    c.fillSprite(theme::bg());
    c.setFont(&fonts::Font0);
    c.setTextSize(1);

    c.setTextColor(theme::accent());
    c.setTextDatum(top_left);
    c.drawString("SETTINGS", 6, 3);
    c.drawFastHLine(6, 13, 228, theme::faint());

    constexpr int ROW_Y = 16, ROW_H = 13;
    for (int row = 0; row < ROW_COUNT; row++) {
        const int y = ROW_Y + row * ROW_H;
        const bool sel = row == selected;
        if (sel) c.fillRoundRect(4, y, 232, ROW_H - 2, 3, theme::accent());
        const uint16_t ink = sel ? theme::bg() : theme::dim();

        c.setTextColor(ink);
        c.setTextDatum(middle_left);
        c.drawString(LABELS[row], 10, y + ROW_H / 2 - 1);

        c.setTextDatum(middle_right);
        c.setTextColor(sel ? theme::bg() : theme::accent());
        const String value = sel ? "< " + valueText(row) + " >" : valueText(row);
        c.drawString(value, 230, y + ROW_H / 2 - 1);
    }

    c.setTextColor(theme::faint());
    c.setTextDatum(bottom_left);
    c.drawString("; . choose   , / change   esc done", 6, 134);
}

}  // namespace settings_ui
