// The settings screen: arrow keys to pick a row and change its value.
// Edits apply straight away (colour previews live) and are saved by settings.

#pragma once
#include <M5GFX.h>

namespace settings_ui {

enum class Action { None, Changed, Close };

void open();
// Keys: ';' up, '.' down, ',' left, '/' right (the Cardputer's arrow keys),
// '\n' Enter, '`' Esc.
Action handleKey(char c);
void draw(M5Canvas& canvas);

}  // namespace settings_ui
