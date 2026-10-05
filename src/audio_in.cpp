#include "audio_in.h"

#include <M5Cardputer.h>

#include <atomic>

namespace audio_in {

namespace {

constexpr size_t CHUNK = 256;  // samples per capture request (16 ms)
constexpr size_t MASK = RING_SIZE - 1;
static_assert((RING_SIZE & MASK) == 0, "RING_SIZE must be a power of two");

constexpr uint8_t ES8311_ADDR = 0x18;
constexpr uint8_t REG_ADC_PGA = 0x14;  // bit4 = Mic1p/Mic1n input, bits3:0 = gain
constexpr uint8_t REG_ADC_HPF = 0x1C;  // 0x6A = M5Unified default (EQ bypass + HPF)
constexpr uint8_t HPF_ON = 0x6A;
constexpr uint8_t HPF_OFF = 0x4A;

int16_t ring[RING_SIZE];
int16_t chunks[2][CHUNK];
std::atomic<uint32_t> written{0};
bool running = false;
uint8_t pga = 4;  // +12 dB starting point
bool hpf = true;

void writeCodec(uint8_t reg, uint8_t value) {
    M5Cardputer.In_I2C.writeRegister8(ES8311_ADDR, reg, value, 100000);
}

// Runs on the mic capture task each time a chunk is full.
void onChunk(void*, void* data, size_t length) {
    auto* samples = static_cast<int16_t*>(data);
    uint32_t w = written.load(std::memory_order_relaxed);
    for (size_t i = 0; i < length; i++) ring[(w + i) & MASK] = samples[i];
    written.store(w + length, std::memory_order_release);
    M5Cardputer.Mic.record(samples, length);  // re-queue the same buffer
}

void queueBoth() {
    M5Cardputer.Mic.record(chunks[0], CHUNK, SAMPLE_RATE);
    M5Cardputer.Mic.record(chunks[1], CHUNK, SAMPLE_RATE);
}

}  // namespace

bool begin() {
    M5Cardputer.Speaker.end();

    auto cfg = M5Cardputer.Mic.config();
    cfg.sample_rate = SAMPLE_RATE;
    cfg.over_sampling = 2;
    cfg.magnification = 2;  // net x1: the library divides by over_sampling*2
    cfg.noise_filter_level = 0;
    M5Cardputer.Mic.config(cfg);
    M5Cardputer.Mic.setBufferReleaseCallback(nullptr, onChunk);

    if (!M5Cardputer.Mic.begin()) return false;

    // The library's codec setup ran inside begin(); apply our overrides.
    setPgaStep(pga);
    setHighPass(hpf);

    written.store(0);
    running = true;
    queueBoth();
    return true;
}

void end() {
    running = false;
    M5Cardputer.Mic.end();
}

void poll() {
    if (running && M5Cardputer.Mic.isRecording() == 0) queueBoth();
}

uint32_t sampleCount() { return written.load(std::memory_order_acquire); }

bool copy(uint32_t start, int16_t* dst, size_t n) {
    uint32_t w = sampleCount();
    if (n > RING_SIZE - CHUNK) return false;
    if ((int32_t)(w - (start + n)) < 0) return false;          // not captured yet
    if ((int32_t)(w - start) > (int32_t)(RING_SIZE - CHUNK)) return false;  // overwritten
    for (size_t i = 0; i < n; i++) dst[i] = ring[(start + i) & MASK];
    return true;
}

bool latest(int16_t* dst, size_t n) {
    uint32_t w = sampleCount();
    if (w < n) return false;
    return copy(w - n, dst, n);
}

void setPgaStep(uint8_t step) {
    if (step > 10) step = 10;
    pga = step;
    writeCodec(REG_ADC_PGA, 0x10 | pga);
}

uint8_t pgaStep() { return pga; }

void setHighPass(bool on) {
    hpf = on;
    writeCodec(REG_ADC_HPF, hpf ? HPF_ON : HPF_OFF);
}

bool highPass() { return hpf; }

}  // namespace audio_in
