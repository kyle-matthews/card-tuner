#include "tuner_ui.h"

#include <math.h>

#include "cat.h"
#include "theme.h"

namespace tuner_ui {

namespace {

using tracker::State;

// Layout (240 x 135).
constexpr int CAT_X = 6, CAT_Y = 18, CAT_SIZE = 48;
constexpr int NOTE_X = 60, NOTE_Y = 16, NOTE_W = 92, NOTE_H = 60;
constexpr int INFO_X = 160;
constexpr int METER_X = 18, METER_W = 204, METER_Y = 82, METER_H = 26;
constexpr int METER_MID = METER_X + METER_W / 2;
constexpr int STRINGS_Y = 117, STRINGS_H = 17;
constexpr float IN_TUNE_ZONE_CENTS = 3;

// The needle eases towards its target each frame so it glides rather than
// jumps between detector updates.
float needleCents = 0;
bool needleValid = false;

// Square-root scale: more room near zero, where fine tuning happens.
int centsToX(float cents) {
    const float c = fmaxf(-50, fminf(50, cents));
    const float t = copysignf(sqrtf(fabsf(c) / 50), c);
    return METER_MID + (int)lroundf(t * (METER_W / 2));
}

uint16_t inkFor(const tracker::Reading& r) {
    switch (r.state) {
        case State::Live: return theme::accent();
        case State::Held: return theme::dim();
        default: return theme::faint();
    }
}

// Battery icon and percentage, right-aligned at `right`. Dim normally, full
// colour when low, blinking when nearly empty.
void drawBattery(M5Canvas& c, int right, int level) {
    if (level < 0) return;  // unknown
    if (level <= 10 && (millis() / 500) % 2) return;
    const uint16_t ink = level <= 15 ? theme::accent() : theme::dim();

    constexpr int W = 13, H = 7, Y = 3;
    const int x = right - W - 2;
    c.drawRect(x, Y, W, H, ink);
    c.fillRect(x + W, Y + 2, 2, H - 4, ink);  // terminal nub
    const int fill = (W - 4) * level / 100;
    if (fill > 0) c.fillRect(x + 2, Y + 2, fill, H - 4, ink);

    c.setTextColor(ink);
    c.setTextDatum(top_right);
    c.drawString(String(level) + "%", x - 3, Y);
}

void drawHeader(M5Canvas& c, const Status& s) {
    c.setTextSize(1);
    c.setTextColor(theme::dim());
    c.setTextDatum(top_left);
    c.drawString(s.mode, 6, 3);
    c.setTextDatum(top_right);
    if (s.toast.length()) {
        c.setTextColor(theme::accent());
        c.drawString(s.toast, 234, 3);
    } else {
        const String a4 = String("A") + (int)lroundf(s.a4);
        c.drawString(a4, 234, 3);
        drawBattery(c, 234 - c.textWidth(a4) - 6, s.battery);
    }
}


void drawNote(M5Canvas& c, const tracker::Reading& r, const Target& t) {
    const bool showing = r.state != State::Idle;
    uint16_t ink = inkFor(r);
    if (t.inTune) {
        // Inverted: solid accent box, black note.
        c.fillRoundRect(NOTE_X, NOTE_Y, NOTE_W, NOTE_H, 6, theme::accent());
        ink = theme::bg();
    }

    const char* name = showing ? tuning::nameOf(t.midi, t.flats) : "-";
    const char accidental = name[1];  // '#', 'b' or none

    // Letter: 6x8 font at 7x = 42x56.
    c.setTextColor(ink);
    c.setTextDatum(top_left);
    c.setTextSize(7);
    const int letterX = NOTE_X + (accidental ? 8 : 18);
    c.drawString(String(name[0]), letterX, NOTE_Y + 3);

    if (showing) {
        c.setTextSize(3);
        if (accidental) c.drawString(String(accidental), letterX + 44, NOTE_Y + 4);
        c.setTextSize(2);
        c.drawString(String(tuning::octaveOf(t.midi)), letterX + 46, NOTE_Y + 42);
    }
}

void drawInfo(M5Canvas& c, const tracker::Reading& r, const Target& t) {
    c.setTextDatum(top_left);
    if (r.state == State::Idle) {
        c.setTextSize(1);
        c.setTextColor(theme::dim());
        c.drawString("play a", INFO_X, 34);
        c.drawString("note...", INFO_X, 46);
        return;
    }
    const int cents = (int)lroundf(t.cents);

    c.setTextSize(3);
    c.setTextColor(inkFor(r));
    char buf[16];
    snprintf(buf, sizeof buf, cents == 0 ? "0" : "%+d", cents);
    c.drawString(buf, INFO_X, 20);

    c.setTextSize(1);
    if (fabsf(t.cents) > 50) {
        // Far from the string: say which way to turn.
        c.setTextColor(inkFor(r));
        c.drawString(t.cents < 0 ? "tune up" : "tune down", INFO_X, 46);
    } else {
        c.setTextColor(theme::dim());
        c.drawString("cents", INFO_X, 46);
    }
    c.setTextColor(theme::dim());
    snprintf(buf, sizeof buf, "%.2f Hz", r.hz);
    c.drawString(buf, INFO_X, 62);
}

void drawMeter(M5Canvas& c, const tracker::Reading& r, const Target& t) {
    const int zoneL = centsToX(-IN_TUNE_ZONE_CENTS), zoneR = centsToX(IN_TUNE_ZONE_CENTS);

    // In-tune zone: outlined, filled solid when in tune.
    if (t.inTune) {
        c.fillRect(zoneL, METER_Y, zoneR - zoneL + 1, METER_H, theme::accent());
    } else {
        c.drawRect(zoneL, METER_Y, zoneR - zoneL + 1, METER_H, theme::faint());
    }

    // Scale.
    c.drawFastHLine(METER_X, METER_Y + METER_H, METER_W, theme::faint());
    const int ticks[] = {5, 10, 20, 30, 40, 50};
    for (int tick : ticks) {
        const int h = (tick % 10 == 0) ? 6 : 3;
        c.drawFastVLine(centsToX(tick), METER_Y + METER_H - h, h, theme::dim());
        c.drawFastVLine(centsToX(-tick), METER_Y + METER_H - h, h, theme::dim());
    }
    c.setTextSize(1);
    c.setTextColor(theme::dim());
    c.setTextDatum(middle_center);
    c.drawString("b", METER_X - 10, METER_Y + METER_H / 2);
    c.drawString("#", METER_X + METER_W + 10, METER_Y + METER_H / 2);

    // Needle.
    if (r.state == State::Idle) {
        needleValid = false;
        return;
    }
    if (!needleValid) {
        needleCents = t.cents;
        needleValid = true;
    }
    needleCents += 0.5f * (t.cents - needleCents);
    const int x = centsToX(needleCents);
    const uint16_t ink = inkFor(r);
    // Inside the filled in-tune zone the needle is cut out in black; the head
    // sits above the zone, so it always uses the ink colour.
    c.fillRect(x - 1, METER_Y + 4, 3, METER_H - 4, t.inTune ? theme::bg() : ink);
    c.fillTriangle(x - 5, METER_Y - 2, x + 5, METER_Y - 2, x, METER_Y + 4, ink);
}

// One cell per string: the target is boxed (solid when in tune), strings
// that have been in tune this session get a dot underneath.
void drawStrings(M5Canvas& c, const tracker::Reading& r, const Target& t, const Status& s) {
    const tuning::Preset& p = *s.preset;
    const int cellW = 228 / (int)p.count;
    const int x0 = 6 + (228 - cellW * (int)p.count) / 2;
    c.setTextSize(1);
    c.setTextDatum(middle_center);
    for (size_t i = 0; i < p.count; i++) {
        const int x = x0 + (int)i * cellW;
        const bool target = r.state != State::Idle && (int)i == t.string;
        uint16_t ink = theme::dim();
        if (target && t.inTune) {
            c.fillRoundRect(x + 2, STRINGS_Y, cellW - 4, STRINGS_H - 4, 3, theme::accent());
            ink = theme::bg();
        } else if (target) {
            c.drawRoundRect(x + 2, STRINGS_Y, cellW - 4, STRINGS_H - 4, 3, inkFor(r));
            ink = inkFor(r);
        }
        // The top string is written lowercase when it shares the bottom
        // string's name (the high e in E A D G B e).
        String name = tuning::nameOf(p.strings[i], p.flats);
        if (i == p.count - 1 && name == tuning::nameOf(p.strings[0], p.flats)) name.toLowerCase();
        c.setTextColor(ink);
        c.drawString(name, x + cellW / 2, STRINGS_Y + (STRINGS_H - 4) / 2);
        if (s.tunedStrings & (1u << i)) c.fillCircle(x + cellW / 2, STRINGS_Y + STRINGS_H - 1, 1, theme::accent());
    }
}

void drawFooter(M5Canvas& c, const Status& s) {
    c.setTextSize(1);
    c.setTextDatum(bottom_left);
    c.setTextColor(s.footerActive ? theme::accent() : theme::faint());
    c.drawString(s.footer, 6, 134);
}

}  // namespace

Target targetFor(const tracker::Reading& r, const tuning::Preset* preset) {
    Target t;
    t.midi = r.midi;
    t.cents = r.cents;
    t.inTune = r.state == State::Live && r.inTune;
    if (!preset || r.state == State::Idle) return t;
    t.flats = preset->flats;

    const float exact = r.midi + r.cents / 100;
    t.string = tuning::nearestString(*preset, exact);
    if (t.string < 0) return t;  // nowhere near any string: show the plain note
    t.midi = preset->strings[t.string];
    t.cents = (exact - t.midi) * 100;
    t.inTune = t.inTune && r.midi == t.midi;
    return t;
}

void draw(M5Canvas& canvas, const tracker::Reading& reading, const Status& status) {
    const Target target = targetFor(reading, status.preset);
    canvas.fillSprite(theme::bg());
    canvas.setFont(&fonts::Font0);
    drawHeader(canvas, status);
    const uint32_t now = millis();
    if (status.showCat) {
        cat::update(reading.state, target.cents, target.inTune, now);
        cat::draw(canvas, CAT_X, CAT_Y, CAT_SIZE / 24, now);
    }
    drawNote(canvas, reading, target);
    drawInfo(canvas, reading, target);
    drawMeter(canvas, reading, target);
    if (status.preset) {
        drawStrings(canvas, reading, target, status);
    } else {
        drawFooter(canvas, status);
    }
    if (status.showCat) cat::drawBubble(canvas, CAT_X, CAT_Y, CAT_SIZE / 24, now);  // on top of everything
}

}  // namespace tuner_ui
