// Minimal reader for the 16-bit mono WAVs written by tools/capture.py.
// Shared by the native tests; looks in recordings/ relative to where the
// test binary runs.

#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

inline std::vector<int16_t> readWav(const std::string& name) {
    const char* dirs[] = {"recordings/", "../recordings/", "../../recordings/"};
    for (const char* dir : dirs) {
        FILE* f = fopen((std::string(dir) + name).c_str(), "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> buf(size);
        size_t got = fread(buf.data(), 1, size, f);
        fclose(f);
        for (size_t i = 12; i + 8 <= got;) {
            uint32_t len;
            memcpy(&len, &buf[i + 4], 4);
            if (memcmp(&buf[i], "data", 4) == 0) {
                std::vector<int16_t> s(std::min<size_t>(len, got - i - 8) / 2);
                memcpy(s.data(), &buf[i + 8], s.size() * 2);
                return s;
            }
            i += 8 + len;
        }
    }
    return {};
}
