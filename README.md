# card-tuner

A pocket guitar and bass tuner for the **M5Stack Cardputer ADV**, starring **note#**, a little pixel cat who reacts to your tuning.

It uses the Cardputer ADV's built-in mic, so nothing needs to be bought or soldered.

## Features

- **Chromatic tuner** for guitar and bass, down to bass low E (~41 Hz) and below
- **Note, frequency and cents** readout with a sharp/flat needle
- **Tuning presets** (E standard, Drop D, DADGAD, 5/6-string bass, ...) showing the target string
- **Adjustable A4** reference (432–446 Hz)
- **Monochrome look:** black background with one accent colour, chosen in Settings (white, amber, phosphor green, cyan, pink, lavender, red)
- **note# the cat:** sleeps in silence, perks up when you play, frets when you're flat or sharp, and is delighted when you're in tune

## How it works

Pitch detection uses the **YIN** algorithm, which runs on the raw audio waveform. Tiny MEMS mics barely pick up a bass's fundamental, but the waveform still repeats at the fundamental's period, so YIN finds the right note where a simple FFT peak would read an octave high.

- 16 kHz sampling, a ~107 ms analysis frame (two of the longest periods searched), and sub-sample interpolation measured across several periods at once for sub-cent precision
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

## Testing

The pitch detector has no hardware dependencies, so it is tested on the PC against synthetic tones and real recordings. This needs a host C++ compiler on PATH (for example `winget install BrechtSanders.WinLibs.POSIX.UCRT`).

```
pio test -e native
```

### Recording test audio

The M1 firmware can stream 3 s of mic audio to the PC. With the device connected (and the serial monitor closed):

```
~/.platformio/penv/Scripts/python.exe tools/capture.py COM5 guitar_low_e --wait
```

Pluck the string, then press `r` on the Cardputer. The recording is saved to `recordings/guitar_low_e.wav`.

To see what the detector makes of a recording, frame by frame:

```
g++ -O2 -std=c++17 -Isrc tools/pitch_scan.cpp src/pitch.cpp -o .pio/pitch_scan
.pio/pitch_scan recordings/speaker_bass_e.wav
```

## Controls

### Tuner

| Key | Action |
|---|---|
| `g` / `b` | Guitar / bass |
| `,` / `/` (← / →) | Previous / next tuning preset |
| `c` | Chromatic mode (any note, no target string) |
| `-` / `=` | A4 pitch down / up (430–450 Hz) |
| `s` | Settings |
| `r` | Record 3 s of audio for `tools/capture.py` (development) |

With a preset, the tuner aims at the nearest string: the big note is that string, the cents are measured against it ("tune up" / "tune down" when more than 50 cents out), and the string row along the bottom boxes it. Strings that have been in tune get a dot.

### Settings

`;` / `.` (↑ / ↓) choose a row, `,` / `/` (← / →) or Enter change it, Esc or `s` to go back. Colour, A4 pitch, instrument, tuning, mic gain and showing note# are saved to flash and survive a power cycle.

**Presets.** Guitar: E standard, Drop D, Eb standard, D standard, Drop C, DADGAD, Open G, Open D, 7-string. Bass: E standard, Drop D, Eb standard, D standard, 5-string BEADG, 6-string BEADGC.

### Over serial (development)

`l` toggles a per-frame detector log, `s` sends a screenshot (`tools/screenshot.py`), `d` cycles demo readings, `S` opens settings. Any other character acts as that key on the keyboard.

## Project layout

```
src/
  main.cpp          setup/loop
  audio_in.*        mic → ring buffer
  pitch.*           YIN pitch detection (no hardware deps, unit-testable on PC)
  tuning.*          frequency → note/cents, tuning presets
  tracker.*         smoothing, outlier rejection, hold and in-tune detection
  theme.*           accent colour and its dim/faint shades
  debug_dump.*      stream raw mic audio over serial for tools/capture.py
  tuner_ui.*        tuner screen
  cat.*             note# sprites and animation
  settings.*        persistent settings (NVS)
  settings_ui.*     settings screen
test/
  test_pitch/       native unit tests on synthetic and recorded signals
  test_tuning/      native unit tests for note/cents conversion
  test_tracker/     native unit tests for smoothing, hold and in-tune
tools/
  capture.py        save a mic recording from the device as WAV
  pitch_scan.cpp    print detector output frame by frame for a WAV
  screenshot.py     save the device's screen as a PNG
recordings/         test audio captured on the device
```

## Roadmap

- [x] **M0** Scaffold: text on screen, key presses register, beep
- [x] **M1** Mic level meter and a raw sample dump (bass strings recorded through a speaker; real bass and guitar recordings still to do)
- [x] **M2** YIN pitch detection passing native unit tests (synthetic tones 31–988 Hz, missing fundamental, bass E/A/D/G recordings)
- [x] **M3** Tuner v1 on device: note, Hz, cents (detector runs in 5 ms per frame using ESP-DSP SIMD)
- [x] **M4** Tuner UI: theme system, needle, in-tune inversion, smoothing (median + easing), outlier rejection, hold
- [x] **M5** Settings screen (accent colour, A4, instrument, preset, mic gain) saved to NVS; tuning presets with target string
- [ ] **M6** note# the cat: 1-bit pixel sprites reacting to tuning state, splash screen
- [ ] *Maybe later:* reference tone playback through the speaker/headphones
