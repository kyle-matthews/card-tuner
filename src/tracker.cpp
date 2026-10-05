#include "tracker.h"

#include <math.h>

#include <algorithm>

namespace tracker {

float Tracker::toMidi(float hz) const { return 69 + 12 * log2f(hz / config_.a4); }

void Tracker::setA4(float a4) {
    // Keep the current reading continuous: shift internal state by the change.
    const float shift = 12 * log2f(config_.a4 / a4);
    config_.a4 = a4;
    smoothed_ += shift;
    candidate_ += shift;
    for (size_t i = 0; i < historyCount_; i++) history_[i] += shift;
    publish();
}

void Tracker::restartAt(float midi) {
    tracking_ = true;
    smoothed_ = midi;
    history_[0] = midi;
    historyCount_ = 1;
    historyNext_ = 1;
    hasCandidate_ = false;
    close_ = false;
}

const Reading& Tracker::update(const pitch::Result& frame, uint32_t nowMs) {
    if (!frame.voiced) {
        hasCandidate_ = false;
        if (tracking_) {
            const uint32_t silentMs = nowMs - lastVoicedMs_;
            if (silentMs >= config_.holdMs) {
                tracking_ = false;
                reading_ = Reading();
                return reading_;
            }
            if (silentMs >= config_.staleMs) {
                reading_.state = State::Held;
                reading_.inTune = false;
                close_ = false;
            }
        }
        return reading_;
    }

    const float midi = toMidi(frame.hz);
    const bool nearCurrent = tracking_ && fabsf(midi - smoothed_) * 100 <= config_.jumpCents;

    if (nearCurrent) {
        hasCandidate_ = false;
        history_[historyNext_] = midi;
        historyNext_ = (historyNext_ + 1) % MEDIAN_LEN;
        if (historyCount_ < MEDIAN_LEN) historyCount_++;

        float sorted[MEDIAN_LEN];
        std::copy(history_, history_ + historyCount_, sorted);
        std::sort(sorted, sorted + historyCount_);
        const float median = sorted[historyCount_ / 2];
        smoothed_ += config_.smoothing * (median - smoothed_);
    } else {
        // Far from what we're showing (or showing nothing): a new note, or an
        // outlier. Only believe it once it has persisted.
        if (hasCandidate_ && fabsf(midi - candidate_) * 100 <= config_.jumpCents) {
            candidateFrames_++;
            candidate_ = midi;
        } else {
            hasCandidate_ = true;
            candidate_ = midi;
            candidateFrames_ = 1;
        }
        if (candidateFrames_ < config_.confirmFrames) {
            // Not confirmed: leave the display alone, but an outlier while a
            // note is ringing doesn't count as silence.
            if (tracking_) lastVoicedMs_ = nowMs;
            return reading_;
        }
        restartAt(midi);
    }

    lastVoicedMs_ = nowMs;
    reading_.state = State::Live;
    publish();
    updateInTune(nowMs);
    return reading_;
}

void Tracker::publish() {
    if (!tracking_) return;
    reading_.midi = (int)lroundf(smoothed_);
    reading_.cents = (smoothed_ - reading_.midi) * 100;
    reading_.hz = config_.a4 * powf(2, (smoothed_ - 69) / 12);
}

void Tracker::updateInTune(uint32_t nowMs) {
    const float off = fabsf(reading_.cents);
    if (reading_.inTune) {
        if (off > config_.outOfTuneCents) {
            reading_.inTune = false;
            close_ = false;
        }
        return;
    }
    if (off <= config_.inTuneCents) {
        if (!close_) {
            close_ = true;
            closeSinceMs_ = nowMs;
        }
        if (nowMs - closeSinceMs_ >= config_.inTuneMs) reading_.inTune = true;
    } else {
        close_ = false;
    }
}

}  // namespace tracker
