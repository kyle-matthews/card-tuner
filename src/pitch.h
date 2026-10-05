// YIN pitch detection (de Cheveigné & Kawahara, 2002).
//
// Works on the raw waveform rather than the spectrum, so it finds the
// fundamental period even when a small mic barely picks up the fundamental
// itself (bass low E at 41 Hz). No hardware dependencies, so it is unit
// tested on the PC (test/test_pitch).

#pragma once
#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace pitch {

struct Config {
    float sampleRate = 16000;
    float minHz = 28;    // lowest pitch searched (5-string bass low B is 30.9 Hz)
    float maxHz = 1000;  // highest pitch searched
    // A dip in the normalised difference below this counts as a period.
    // Lower is stricter: fewer false detections, more missed notes.
    float threshold = 0.15f;
    // Frames quieter than this RMS (in int16 units) are reported unvoiced.
    float minRms = 10;
};

struct Result {
    bool voiced = false;
    float hz = 0;
    // 0..1, higher is more certain (1 minus the normalised difference dip).
    float confidence = 0;
    float rms = 0;
};

class Yin {
public:
    explicit Yin(const Config& config = Config());
    Yin(const Yin&) = delete;  // holds pointers into its own buffers
    Yin& operator=(const Yin&) = delete;

    // Samples needed per call: the analysis window plus the longest period
    // (plus one, for interpolating around the longest period).
    size_t frameSize() const { return window_ + tauMax_ + 1; }

    // Analyse `n` samples (at least frameSize(); extra samples are ignored).
    Result detect(const int16_t* samples, size_t n);

    const Config& config() const { return config_; }

private:
    float interpolate(size_t tau) const;

    Config config_;
    size_t tauMin_;
    size_t tauMax_;
    size_t window_;
    // DC-removed input, stored four times, each copy shifted one sample
    // further, so that x + tau is always 16-byte aligned for the SIMD dot
    // product: x + tau == lanes_[tau % 4] + (tau - tau % 4).
    std::vector<float> laneStore_;
    float* lanes_[4];
    std::vector<float> diff_;  // difference function d(tau)
    std::vector<float> cmnd_;  // cumulative mean normalised difference d'(tau)
};

}  // namespace pitch
