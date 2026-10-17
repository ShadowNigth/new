#include "engine/debug/FrameStats.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace classicfps::debug {

FrameStats::FrameStats() noexcept {
    reset();
}

void FrameStats::reset() noexcept {
    samplesMs_.fill(0.0);
    sampleIndex_ = 0;
    sampleCount_ = 0;
    currentFps_ = 0.0;
    avgFrameMs_ = 0.0;
    minFrameMs_ = 0.0;
    maxFrameMs_ = 0.0;
    totalFrames_ = 0;
    totalSimSteps_ = 0;
}

void FrameStats::recordFrame(double frameDeltaSeconds, std::uint32_t simStepsThisFrame) noexcept {
    const double frameMs = std::max(0.0, frameDeltaSeconds * 1000.0);
    samplesMs_[sampleIndex_] = frameMs;
    sampleIndex_ = (sampleIndex_ + 1) % kSampleWindow;
    if (sampleCount_ < kSampleWindow) {
        ++sampleCount_;
    }

    ++totalFrames_;
    totalSimSteps_ += simStepsThisFrame;

    double sumMs = 0.0;
    double minMs = std::numeric_limits<double>::max();
    double maxMs = 0.0;

    for (std::size_t i = 0; i < sampleCount_; ++i) {
        const double v = samplesMs_[i];
        sumMs += v;
        minMs = std::min(minMs, v);
        maxMs = std::max(maxMs, v);
    }

    avgFrameMs_ = (sampleCount_ > 0) ? (sumMs / static_cast<double>(sampleCount_)) : 0.0;
    minFrameMs_ = (sampleCount_ > 0) ? minMs : 0.0;
    maxFrameMs_ = maxMs;
    currentFps_ = (avgFrameMs_ > 0.0001) ? (1000.0 / avgFrameMs_) : 0.0;
}

std::string FrameStats::summaryString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "FPS: " << currentFps_
        << " | Frame(ms) avg=" << avgFrameMs_
        << " min=" << minFrameMs_
        << " max=" << maxFrameMs_
        << " | Frames=" << totalFrames_
        << " Ticks=" << totalSimSteps_;
    return oss.str();
}

} // namespace classicfps::debug
