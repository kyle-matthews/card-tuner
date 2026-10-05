#include "pitch.h"

#include <math.h>
#include <stdint.h>

#if defined(ESP_PLATFORM)
#include "dsps_dotprod.h"
#endif

namespace pitch {

// Dot product: the inner loop of the detector, so on the ESP32-S3 it uses
// ESP-DSP's hand-optimised SIMD routine (about 10x faster than a plain loop).
static inline float dot(const float* a, const float* b, size_t n) {
    float result = 0;
#if defined(ESP_PLATFORM)
    dsps_dotprod_f32(a, b, &result, (int)n);
#else
    for (size_t i = 0; i < n; i++) result += a[i] * b[i];
#endif
    return result;
}

Yin::Yin(const Config& config) : config_(config) {
    tauMin_ = (size_t)floorf(config.sampleRate / config.maxHz);
    if (tauMin_ < 2) tauMin_ = 2;
    tauMax_ = (size_t)ceilf(config.sampleRate / config.minHz);
    // Integrate over about two of the longest periods: enough to be stable on
    // bass, short enough to follow a note as it changes. A multiple of four
    // keeps the SIMD dot product on its fast path.
    window_ = (2 * tauMax_ + 3) & ~(size_t)3;

    const size_t stride = (frameSize() + 3) & ~(size_t)3;
    laneStore_.resize(4 * stride + 4);
    float* base = laneStore_.data();
    while ((uintptr_t)base & 15) base++;
    for (size_t k = 0; k < 4; k++) lanes_[k] = base + k * stride;
    diff_.resize(tauMax_ + 2);
    cmnd_.resize(tauMax_ + 2);
}

Result Yin::detect(const int16_t* samples, size_t n) {
    Result result;
    const size_t len = frameSize();
    if (n < len) return result;

    // 1. Remove DC and measure loudness.
    float sum = 0;  // float, not double: the ESP32-S3 FPU is single precision
    for (size_t i = 0; i < len; i++) sum += samples[i];
    const float mean = sum / len;
    float* x = lanes_[0];
    float sq = 0;
    for (size_t i = 0; i < len; i++) {
        x[i] = samples[i] - mean;
        sq += x[i] * x[i];
    }
    result.rms = sqrtf(sq / len);
    if (result.rms < config_.minRms) return result;

    // 2. Difference function: how unlike the signal is to itself shifted by
    //    tau. Expanding sum((a - b)^2) = sum(a^2) + sum(b^2) - 2 sum(a*b)
    //    turns the inner loop into one dot product; the shifted window's
    //    energy sum(b^2) slides along one sample per tau.
    const size_t lastTau = tauMax_ + 1;  // one extra for interpolation
    for (size_t k = 1; k < 4; k++) {
        for (size_t i = 0; i + k < len; i++) lanes_[k][i] = x[i + k];
    }
    const float energy0 = dot(x, x, window_);
    float energyTau = energy0;
    diff_[0] = 0;
    for (size_t tau = 1; tau <= lastTau; tau++) {
        energyTau += x[tau + window_ - 1] * x[tau + window_ - 1] - x[tau - 1] * x[tau - 1];
        const float* shifted = lanes_[tau & 3] + (tau & ~(size_t)3);  // == x + tau, aligned
        const float d = energy0 + energyTau - 2 * dot(x, shifted, window_);
        diff_[tau] = d > 0 ? d : 0;  // rounding can dip just below zero
    }

    // 3. Cumulative mean normalised difference: removes the bias towards
    //    tau = 0 and puts dips on a 0..1-ish scale.
    cmnd_[0] = 1;
    float running = 0;
    for (size_t tau = 1; tau <= lastTau; tau++) {
        running += diff_[tau];
        cmnd_[tau] = running > 0 ? diff_[tau] * tau / running : 1;
    }

    // 4. Absolute threshold: take the first dip below the threshold, then walk
    //    down to the bottom of that dip. Taking the *first* (shortest period)
    //    is what stops YIN reporting an octave too low.
    size_t best = 0;
    for (size_t tau = tauMin_; tau <= tauMax_; tau++) {
        if (cmnd_[tau] < config_.threshold) {
            while (tau + 1 <= tauMax_ && cmnd_[tau + 1] < cmnd_[tau]) tau++;
            best = tau;
            break;
        }
    }
    if (best == 0) return result;  // no clear period: noise or a chord

    // 5. Sub-sample precision. Interpolating one period's dip has a small
    //    bias, so where the search range allows, measure k whole periods at
    //    once and divide by k: the error shrinks by a factor of k.
    float tau = interpolate(best);
    for (size_t k = tauMax_ / best; k >= 2; k--) {
        const size_t centre = (size_t)lroundf(tau * k);
        size_t dip = centre;
        for (size_t t = centre - 2; t <= centre + 2 && t <= tauMax_; t++) {
            if (diff_[t] < diff_[dip]) dip = t;
        }
        // Only trust it if the multiple is itself a clear dip.
        if (dip > centre - 2 && dip < centre + 2 && cmnd_[dip] < 2 * config_.threshold) {
            tau = interpolate(dip) / k;
            break;
        }
    }

    result.voiced = true;
    result.hz = config_.sampleRate / tau;
    result.confidence = 1 - cmnd_[best];
    if (result.confidence < 0) result.confidence = 0;
    return result;
}

// Parabolic interpolation on d(tau) around a local minimum.
float Yin::interpolate(size_t tau) const {
    if (tau < 1 || tau + 1 >= diff_.size()) return (float)tau;
    const float s0 = diff_[tau - 1];
    const float s1 = diff_[tau];
    const float s2 = diff_[tau + 1];
    const float denom = s0 - 2 * s1 + s2;
    if (denom <= 0) return (float)tau;
    return tau + 0.5f * (s0 - s2) / denom;
}

}  // namespace pitch
