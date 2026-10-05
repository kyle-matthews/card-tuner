// Print the pitch detector's output frame by frame for a WAV recording.
//
// Build and run (from the project root, with g++ on PATH):
//   g++ -O2 -std=c++17 -Isrc tools/pitch_scan.cpp src/pitch.cpp -o .pio/pitch_scan
//   .pio/pitch_scan recordings/speaker_bass_e.wav [minHz] [threshold]

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "pitch.h"

static const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s file.wav [minHz] [threshold]\n", argv[0]);
        return 1;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    std::vector<uint8_t> buf;
    uint8_t chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) buf.insert(buf.end(), chunk, chunk + n);
    fclose(f);

    std::vector<int16_t> s;
    for (size_t i = 12; i + 8 <= buf.size();) {
        uint32_t len;
        memcpy(&len, &buf[i + 4], 4);
        if (memcmp(&buf[i], "data", 4) == 0) {
            s.resize(std::min<size_t>(len, buf.size() - i - 8) / 2);
            memcpy(s.data(), &buf[i + 8], s.size() * 2);
            break;
        }
        i += 8 + len;
    }

    pitch::Config cfg;
    if (argc > 2) cfg.minHz = atof(argv[2]);
    if (argc > 3) cfg.threshold = atof(argv[3]);
    pitch::Yin yin(cfg);
    printf("%s: %zu samples, frame %zu (%.0f ms)\n", argv[1], s.size(), yin.frameSize(),
           yin.frameSize() * 1000.0 / cfg.sampleRate);

    for (size_t start = 0; start + yin.frameSize() <= s.size(); start += 800) {
        pitch::Result r = yin.detect(&s[start], yin.frameSize());
        printf("%5.2fs rms %6.1f  ", start / cfg.sampleRate, r.rms);
        if (!r.voiced) {
            printf("--\n");
            continue;
        }
        float midi = 69 + 12 * log2f(r.hz / 440.0f);
        int nearest = (int)lroundf(midi);
        printf("%8.3f Hz  %-2s%d %+6.1f c  conf %.2f\n", r.hz, NOTE_NAMES[((nearest % 12) + 12) % 12],
               nearest / 12 - 1, (midi - nearest) * 100, r.confidence);
    }
    return 0;
}
