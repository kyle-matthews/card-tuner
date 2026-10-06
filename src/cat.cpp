#include "cat.h"

#include <math.h>

#include "theme.h"

namespace cat {

namespace {

// ---- Art ----------------------------------------------------------------------
// 24x24. 'X' = accent, '+' = dim, anything else = background.
// The head, ears and body never change; only rows 7..15 (the face) do.

constexpr int SIZE = 24;
constexpr int FACE_TOP = 7, FACE_ROWS = 9;

const char* const TOP[FACE_TOP] = {
    "..X..................X..",
    "..XX................XX..",
    "..X+X..............X+X..",
    "..X++X............X++X..",
    "..X+++XXXXXXXXXXXX+++X..",
    ".X....................X.",
    ".X....................X.",
};

const char* const BOTTOM[SIZE - FACE_TOP - FACE_ROWS] = {
    ".X....................X.",
    "..X..................X..",
    "...XXXXXXXXXXXXXXXXXX...",
    "..X..................X..",
    ".X....................X.",
    ".X....XX........XX....X.",
    ".X...X..X......X..X...X.",
    ".XXXXXXXXXXXXXXXXXXXXXX.",
};

using Face = const char* const[FACE_ROWS];

Face NEUTRAL = {
    "X......................X",
    "X......................X",
    "X.....XX........XX.....X",
    "X.....XX........XX.....X",
    "X.....XX........XX.....X",
    "X+++.......XX.......+++X",
    "X.........X..X.........X",
    "X+++.......XX.......+++X",
    "X......................X",
};

Face BLINK = {
    "X......................X",
    "X......................X",
    "X......................X",
    "X....XXXX......XXXX....X",
    "X......................X",
    "X+++.......XX.......+++X",
    "X.........X..X.........X",
    "X+++.......XX.......+++X",
    "X......................X",
};

Face HAPPY = {  // ^ ^ and an open mouth
    "X......................X",
    "X......................X",
    "X.....XX........XX.....X",
    "X....X..X......X..X....X",
    "X......................X",
    "X+++.......XX.......+++X",
    "X.........XXXX.........X",
    "X+++......X++X......+++X",
    "X..........XX..........X",
};

Face CONTENT = {  // u u
    "X......................X",
    "X......................X",
    "X......................X",
    "X....X..X......X..X....X",
    "X.....XX........XX.....X",
    "X+++.......XX.......+++X",
    "X.........X..X.........X",
    "X+++.......XX.......+++X",
    "X......................X",
};

Face WORRIED = {  // brows up in the middle, eyes to the left, frown
    "X......X........X......X",
    "X....XX..........XX....X",
    "X....XX........XX......X",
    "X....XX........XX......X",
    "X....XX........XX......X",
    "X+++.......XX.......+++X",
    "X..........XX..........X",
    "X+++......X..X......+++X",
    "X......................X",
};

Face STARTLED = {  // wide eyes, little "o" mouth
    "X......................X",
    "X.....XX........XX.....X",
    "X....X..X......X..X....X",
    "X....X.XX......XX.X....X",
    "X.....XX........XX.....X",
    "X+++.......XX.......+++X",
    "X..........XX..........X",
    "X+++......X..X......+++X",
    "X..........XX..........X",
};

// Small four-point sparkle, 5x5.
const char* const SPARKLE[5] = {"..X..", "..X..", "XX.XX", "..X..", "..X.."};

// ---- State --------------------------------------------------------------------

constexpr uint32_t SLEEP_AFTER_MS = 8000;
constexpr float WORRIED_CENTS = 8;

Mood currentMood = Mood::Idle;
uint32_t quietSinceMs = 0;
bool wasInTune = false;

const char* bubbleText = nullptr;
uint32_t bubbleUntilMs = 0;

void drawRow(M5Canvas& c, const char* row, int x, int y, int scale, bool mirror) {
    for (int col = 0; col < SIZE; col++) {
        const char px = row[mirror ? SIZE - 1 - col : col];
        if (px == 'X' || px == '+') {
            c.fillRect(x + col * scale, y, scale, scale, px == 'X' ? theme::accent() : theme::dim());
        }
    }
}

void drawArt(M5Canvas& c, const Face& face, int x, int y, int scale, bool mirror) {
    for (int r = 0; r < FACE_TOP; r++) drawRow(c, TOP[r], x, y + r * scale, scale, mirror);
    for (int r = 0; r < FACE_ROWS; r++) drawRow(c, face[r], x, y + (FACE_TOP + r) * scale, scale, mirror);
    for (int r = 0; r < SIZE - FACE_TOP - FACE_ROWS; r++) {
        drawRow(c, BOTTOM[r], x, y + (FACE_TOP + FACE_ROWS + r) * scale, scale, mirror);
    }
}

void drawSparkle(M5Canvas& c, int x, int y, uint16_t colour) {
    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 5; col++) {
            if (SPARKLE[r][col] == 'X') c.drawPixel(x + col, y + r, colour);
        }
    }
}

}  // namespace

void update(tracker::State state, float cents, bool inTune, uint32_t nowMs) {
    using tracker::State;
    if (state == State::Live) quietSinceMs = nowMs;
    if (state == State::Live) wasInTune = inTune;

    switch (state) {
        case State::Idle:
            currentMood = nowMs - quietSinceMs > SLEEP_AFTER_MS ? Mood::Asleep : Mood::Idle;
            break;
        case State::Held:
            currentMood = wasInTune ? Mood::Content : Mood::Idle;
            break;
        case State::Live:
            if (inTune) {
                currentMood = Mood::Happy;
            } else if (fabsf(cents) > 50) {
                currentMood = Mood::Startled;
            } else if (cents < -WORRIED_CENTS) {
                currentMood = Mood::Flat;
            } else if (cents > WORRIED_CENTS) {
                currentMood = Mood::Sharp;
            } else {
                currentMood = Mood::Listening;
            }
            break;
    }
}

Mood mood() { return currentMood; }

void draw(M5Canvas& c, int x, int y, int scale, uint32_t nowMs) { draw(c, x, y, scale, currentMood, nowMs); }

void draw(M5Canvas& c, int x, int y, int scale, Mood m, uint32_t nowMs) {
    // A speech bubble makes note# happy for a moment.
    if (bubbleText && nowMs < bubbleUntilMs) m = Mood::Happy;

    const Face* face = &NEUTRAL;
    bool mirror = false;
    int dx = 0, dy = 0;
    switch (m) {
        case Mood::Asleep:
            face = &BLINK;
            dy = (nowMs / 1200) % 2;  // slow breathing
            break;
        case Mood::Idle:
            // Blink for 150 ms every 3.5 s.
            if (nowMs % 3500 < 150) face = &BLINK;
            break;
        case Mood::Listening:
            if (nowMs % 4000 < 120) face = &BLINK;
            break;
        case Mood::Flat:
            face = &WORRIED;
            dx = -scale;  // lean towards the needle
            break;
        case Mood::Sharp:
            face = &WORRIED;
            mirror = true;
            dx = scale;
            break;
        case Mood::Startled:
            face = &STARTLED;
            dy = -scale;
            break;
        case Mood::Happy:
            face = &HAPPY;
            dy = (nowMs / 220) % 2 ? -scale : 0;  // bounce
            break;
        case Mood::Content:
            face = &CONTENT;
            break;
    }
    drawArt(c, *face, x + dx, y + dy, scale, mirror);

    const int size = SIZE * scale;
    c.setFont(&fonts::Font0);
    switch (m) {
        case Mood::Asleep: {
            // Three z's drifting up and to the right, one after another.
            c.setTextDatum(bottom_left);
            for (int i = 0; i < 3; i++) {
                const uint32_t phase = (nowMs / 60 + i * 20) % 60;  // 0..59
                c.setTextSize(phase > 30 ? 2 : 1);
                c.setTextColor(phase > 45 ? theme::faint() : theme::dim());
                // Just right of the head, below the header.
                c.drawString("z", x + size + 2 + phase / 6, y + 30 - phase / 3);
            }
            break;
        }
        case Mood::Startled:
            c.setTextDatum(top_left);
            c.setTextSize(2);
            c.setTextColor(theme::accent());
            c.drawString("!", x + size - 4, y - 2);
            break;
        case Mood::Happy: {
            // Sparkles around note#, twinkling in turn.
            const int spots[4][2] = {{-6, 4}, {size + 1, 10}, {-4, size - 14}, {size, size - 20}};
            for (int i = 0; i < 4; i++) {
                if ((nowMs / 180 + i) % 3 != 0) drawSparkle(c, x + spots[i][0], y + spots[i][1], theme::accent());
            }
            break;
        }
        default:
            break;
    }
}

void say(const char* text, uint32_t nowMs) {
    bubbleText = text;
    bubbleUntilMs = nowMs + 1500;
}

void drawBubble(M5Canvas& c, int catX, int catY, int scale, uint32_t nowMs) {
    if (!bubbleText || nowMs >= bubbleUntilMs) return;
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    const int w = c.textWidth(bubbleText) + 10, h = 14;
    const int bx = catX + 24 * scale - 4, by = catY - 2 * scale;  // just right of the ears
    c.fillRoundRect(bx, by, w, h, 4, theme::bg());
    c.drawRoundRect(bx, by, w, h, 4, theme::accent());
    // Tail pointing down-left at note#'s head.
    c.fillTriangle(bx + 4, by + h - 1, bx + 10, by + h - 1, bx + 2, by + h + 4, theme::bg());
    c.drawLine(bx + 4, by + h - 1, bx + 2, by + h + 4, theme::accent());
    c.drawLine(bx + 10, by + h - 1, bx + 2, by + h + 4, theme::accent());
    c.setTextColor(theme::accent());
    c.setTextDatum(middle_center);
    c.drawString(bubbleText, bx + w / 2, by + h / 2);
}

}  // namespace cat
