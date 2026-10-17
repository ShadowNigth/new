#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace classicfps::debug {

class FrameStats {
public:
    static constexpr std::size_t kSampleWindow = 120;

    FrameStats() noexcept;

    void reset() noexcept;
    void recordFrame(double frameDeltaSeconds, std::uint32_t simStepsThisFrame) noexcept;

    [[nodiscard]] double currentFps() const noexcept { return currentFps_; }
    [[nodiscard]] double averageFrameMs() const noexcept { return avgFrameMs_; }
    [[nodiscard]] double minFrameMs() const noexcept { return minFrameMs_; }
    [[nodiscard]] double maxFrameMs() const noexcept { return maxFrameMs_; }
    [[nodiscard]] std::uint64_t totalFrames() const noexcept { return totalFrames_; }
    [[nodiscard]] std::uint64_t totalSimSteps() const noexcept { return totalSimSteps_; }

    [[nodiscard]] std::string summaryString() const;

private:
    std::array<double, kSampleWindow> samplesMs_{};
    std::size_t sampleIndex_{0};
    std::size_t sampleCount_{0};
    double currentFps_{0.0};
    double avgFrameMs_{0.0};
    double minFrameMs_{0.0};
    double maxFrameMs_{0.0};
    std::uint64_t totalFrames_{0};
    std::uint64_t totalSimSteps_{0};
};

} // namespace classicfps::debug
