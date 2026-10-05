// Stream a few seconds of mic audio over serial for tools/capture.py.
// Triggered by `r` on the keyboard or over serial.

#pragma once
#include <Arduino.h>

namespace debug_dump {

void start();

// Call every loop: copies new samples and sends the dump once complete.
void service();

bool active();

// Short human-readable status ("recording...", "sent 48000 samples", ...).
const String& status();

}  // namespace debug_dump
