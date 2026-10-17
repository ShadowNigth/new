#pragma once

#include <chrono>
#include <cstdint>

namespace classicfps::core {

class EngineClock {
public:
    using SteadyClock = std::chrono::steady_clock;
    using TimePoint   = SteadyClock::time_point;

    explicit EngineClock(double tickRateHz = 100.0, double maxFrameDeltaSeconds = 0.25) noexcept;

    void reset() noexcept;
    void setTickRate(double tickRateHz) noexcept;

    // Measures elapsed real time since last frame and updates the accumulator.
    double beginFrame() noexcept;

    // Deterministic step for unit testing and offline replay.
    void advanceManual(double elapsedSeconds) noexcept;

    // Returns true if another fixed simulation tick should execute in this frame.
    [[nodiscard]] bool shouldStepSimulation() const noexcept;

    // Consumes one fixed simulation step from the accumulator.
    void consumeSimulationStep() noexcept;

    [[nodiscard]] double fixedDeltaTime() const noexcept { return fixedDelta_; }
    [[nodiscard]] double renderDeltaTime() const noexcept { return renderDelta_; }
    [[nodiscard]] double rawFrameDeltaTime() const noexcept { return rawFrameDelta_; }
    [[nodiscard]] double totalWallTime() const noexcept { return totalWallTime_; }
    [[nodiscard]] double totalSimulationTime() const noexcept { return totalSimTime_; }
    [[nodiscard]] double interpolationAlpha() const noexcept;
    [[nodiscard]] std::uint64_t tickCount() const noexcept { return tickCount_; }
    [[nodiscard]] std::uint64_t frameCount() const noexcept { return frameCount_; }

private:
    TimePoint startTime_{};
    TimePoint lastFrameTime_{};
    double fixedDelta_{0.01};
    double maxFrameDelta_{0.25};
    double accumulator_{0.0};
    double renderDelta_{0.0};
    double rawFrameDelta_{0.0};
    double totalWallTime_{0.0};
    double totalSimTime_{0.0};
    std::uint64_t tickCount_{0};
    std::uint64_t frameCount_{0};
};

} // namespace classicfps::core
