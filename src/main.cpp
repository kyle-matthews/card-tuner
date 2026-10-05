// card-tuner — Milestone 0: prove the toolchain, display, keyboard and speaker.
//
// Shows a black screen with accent-coloured text, echoes the last key pressed,
// and beeps on every key press.

#include <M5Cardputer.h>

static M5Canvas canvas(&M5Cardputer.Display);

// Placeholder for the theme system (Milestone 4): one accent colour on black.
static constexpr uint16_t BG = TFT_BLACK;
static constexpr uint16_t ACCENT = TFT_WHITE;

static String lastKey = "-";
static uint32_t keyCount = 0;

static String describeKeys(const Keyboard_Class::KeysState& keys) {
    String s;
    if (keys.fn) s += "fn+";
    if (keys.ctrl) s += "ctrl+";
    if (keys.alt) s += "alt+";
    if (keys.opt) s += "opt+";
    for (char c : keys.word) s += c;
    if (keys.enter) s += "enter";
    if (keys.del) s += "del";
    if (keys.tab) s += "tab";
    return s.length() ? s : String("?");
}

static void draw() {
    canvas.fillSprite(BG);
    canvas.setTextColor(ACCENT, BG);

    canvas.setTextSize(2);
    canvas.setCursor(8, 8);
    canvas.print("card-tuner");

    canvas.setTextSize(1);
    canvas.setCursor(8, 32);
    canvas.print("Milestone 0 - press any key");

    canvas.setCursor(8, 60);
    canvas.print("last key:");
    canvas.setTextSize(3);
    canvas.setCursor(8, 74);
    canvas.print(lastKey);

    canvas.setTextSize(1);
    canvas.setCursor(8, 120);
    canvas.printf("presses: %lu   batt: %d%%", (unsigned long)keyCount,
                  (int)M5Cardputer.Power.getBatteryLevel());

    canvas.pushSprite(0, 0);
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = enable keyboard
    Serial.begin(115200);

    M5Cardputer.Display.setRotation(1);
    canvas.setColorDepth(16);
    canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());

    M5Cardputer.Speaker.setVolume(96);
    M5Cardputer.Speaker.tone(880, 80);

    Serial.println("card-tuner M0 started");
    draw();
}

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        lastKey = describeKeys(M5Cardputer.Keyboard.keysState());
        keyCount++;
        M5Cardputer.Speaker.tone(1320, 30);
        Serial.printf("key: %s\n", lastKey.c_str());
        draw();
    }

    // Refresh occasionally so the battery reading stays current.
    static uint32_t lastDraw = 0;
    if (millis() - lastDraw > 5000) {
        lastDraw = millis();
        draw();
    }
}
