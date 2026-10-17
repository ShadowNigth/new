#include "engine/core/Clock.hpp"

#include <algorithm>

namespace classicfps::core {

EngineClock::EngineClock(double tickRateHz, double maxFrameDeltaSeconds) noexcept
    : maxFrameDelta_(std::max(0.001, maxFrameDeltaSeconds)) {
    setTickRate(tickRateHz);
    reset();
}

void EngineClock::reset() noexcept {
    startTime_ = SteadyClock::now();
    lastFrameTime_ = startTime_;
    accumulator_ = 0.0;
    renderDelta_ = 0.0;
    rawFrameDelta_ = 0.0;
    totalWallTime_ = 0.0;
    totalSimTime_ = 0.0;
    tickCount_ = 0;
    frameCount_ = 0;
}

void EngineClock::setTickRate(double tickRateHz) noexcept {
    const double clampedRate = std::clamp(tickRateHz, 10.0, 1000.0);
    fixedDelta_ = 1.0 / clampedRate;
}

double EngineClock::beginFrame() noexcept {
    const TimePoint now = SteadyClock::now();
    const std::chrono::duration<double> elapsed = now - lastFrameTime_;
    lastFrameTime_ = now;
    advanceManual(elapsed.count());
    return renderDelta_;
}

void EngineClock::advanceManual(double elapsedSeconds) noexcept {
    rawFrameDelta_ = std::max(0.0, elapsedSeconds);
    renderDelta_ = std::min(rawFrameDelta_, maxFrameDelta_);
    totalWallTime_ += rawFrameDelta_;
    accumulator_ += renderDelta_;
    ++frameCount_;
}

bool EngineClock::shouldStepSimulation() const noexcept {
    return accumulator_ >= fixedDelta_;
}

void EngineClock::consumeSimulationStep() noexcept {
    if (accumulator_ >= fixedDelta_) {
        accumulator_ -= fixedDelta_;
        totalSimTime_ += fixedDelta_;
        ++tickCount_;
    }
}

double EngineClock::interpolationAlpha() const noexcept {
    if (fixedDelta_ <= 0.0) {
        return 0.0;
    }
    return std::clamp(accumulator_ / fixedDelta_, 0.0, 1.0);
}

} // namespace classicfps::core
