// Continuous mic capture into a ring buffer.
//
// The mic and speaker share one I2S bus, so begin() shuts the speaker down.
// Samples are mono int16 at SAMPLE_RATE. Each sample has an absolute index
// (0, 1, 2, ...) so readers can ask for exactly the range they want.

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace audio_in {

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t RING_SIZE = 8192;  // ~0.5 s, must be a power of two

bool begin();
void end();

// Call every loop: restarts capture if it ever stalls.
void poll();

// Total samples captured since begin(). The newest sample has index count-1.
uint32_t sampleCount();

// Copy n samples starting at absolute index `start`. Returns false if any of
// them have not been captured yet or have already been overwritten.
bool copy(uint32_t start, int16_t* dst, size_t n);

// Copy the newest n samples. Returns false until n samples exist.
bool latest(int16_t* dst, size_t n);

// ES8311 analog mic preamp: step 0..10, 3 dB per step (0..+30 dB).
void setPgaStep(uint8_t step);
uint8_t pgaStep();

// ES8311 digital high-pass filter (removes DC; may also cut deep bass).
void setHighPass(bool on);
bool highPass();

}  // namespace audio_in
