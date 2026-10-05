# card-tuner

A pocket guitar and bass tuner for the **M5Stack Cardputer ADV**, starring **note#**, a little pixel cat who reacts to your tuning.

It uses the Cardputer ADV's built-in mic, so nothing needs to be bought or soldered.

## Features (planned)

- **Chromatic tuner** for guitar and bass, down to bass low E (~41 Hz) and below
- **Note, frequency and cents** readout with a sharp/flat needle
- **Tuning presets** (E standard, Drop D, DADGAD, 4/5-string bass, ...) showing the target string
- **Adjustable A4** reference (432–446 Hz)
- **Monochrome look:** black background with one accent colour, chosen in Settings (white, amber, phosphor green, cyan, pink, lavender, red)
- **note# the cat:** sleeps in silence, perks up when you play, frets when you're flat or sharp, and is delighted when you're in tune

## How it works

Pitch detection uses the **YIN** algorithm, which runs on the raw audio waveform. Tiny MEMS mics barely pick up a bass's fundamental, but the waveform still repeats at the fundamental's period, so YIN finds the right note where a simple FFT peak would read an octave high.

- 16 kHz sampling, 2048-sample analysis window, parabolic interpolation for sub-cent precision
- Noise gate, confidence threshold, median smoothing and an octave-jump guard
- Guitar/bass mode narrows the search range for speed and stability

## Hardware

- M5Stack Cardputer ADV (ESP32-S3, ES8311 codec, MEMS mic, 240×135 display, I2C keyboard)

## Building

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension.
2. Open this folder in VS Code.
3. Connect the Cardputer ADV over USB-C.
4. Run **PlatformIO: Upload** (or `pio run -t upload`), then **PlatformIO: Serial Monitor** (115200 baud).

If upload fails, put the device in download mode: hold **G0** while plugging in USB.

## Controls (planned)

| Key | Action |
|---|---|
| `G` / `B` | Guitar / bass mode |
| `←` / `→` | Previous / next tuning preset |
| `+` / `-` | Adjust A4 |
| `C` | Chromatic mode (no target string) |
| `S` | Settings |
| `N` | Say hi to note# |

## Project layout

```
src/
  main.cpp          setup/loop
  audio_in.*        mic → ring buffer
  pitch.*           YIN pitch detection (no hardware deps, unit-testable on PC)
  tuning.*          frequency → note/cents, tuning presets
  tuner_ui.*        tuner screen
  cat.*             note# sprites and animation
  input.*           keyboard mapping
  settings.*        persistent settings (NVS)
test/
  test_pitch/       native unit tests on synthetic and recorded signals
```

## Roadmap

- [ ] **M0** Scaffold: text on screen, key presses register, beep
- [ ] **M1** Mic level meter and a raw sample dump (record a real bass low E for testing)
- [ ] **M2** YIN pitch detection passing native unit tests (41 Hz, 82 Hz, 330 Hz, no octave errors)
- [ ] **M3** Tuner v1 on device: note, Hz, cents
- [ ] **M4** Tuner UI: theme system, needle, in-tune inversion, smoothing, noise gate
- [ ] **M5** Settings screen (accent colour, A4, instrument, preset) saved to NVS
- [ ] **M6** note# the cat: 1-bit pixel sprites reacting to tuning state, splash screen
- [ ] *Maybe later:* reference tone playback through the speaker/headphones
