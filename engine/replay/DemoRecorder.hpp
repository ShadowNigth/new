#pragma once

#include "engine/input/InputManager.hpp"
#include "engine/physics/CollisionWorld.hpp"
#include "engine/physics/FPSController.hpp"

#include <glm/vec3.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::replay {

constexpr std::uint32_t kDemoMagic = 0x4D444643U; // "CFDM"
constexpr std::uint16_t kDemoFormatVersion = 1;

enum DemoEventFlags : std::uint32_t {
    DemoEvent_None = 0U,
    DemoEvent_FiredWeapon = (1U << 0U),
    DemoEvent_ReloadedWeapon = (1U << 1U),
    DemoEvent_HitTarget = (1U << 2U),
    DemoEvent_PlantedBomb = (1U << 3U),
    DemoEvent_DefusedBomb = (1U << 4U)
};

struct DemoHeader {
    std::string mapName{"de_arena"};
    float tickRateHz{100.0f};
    glm::vec3 spawnFeetPosition{0.0f};
    float spawnYawDegrees{0.0f};
    float spawnPitchDegrees{0.0f};
};

struct DemoTickFrame {
    std::uint32_t tickIndex{0};
    input::PlayerCommand command{};
    glm::vec3 recordedFeetPosition{0.0f};
    glm::vec3 recordedVelocity{0.0f};
    std::uint32_t eventFlags{DemoEvent_None};
};

struct DeterminismReport {
    bool deterministic{false};
    std::size_t ticksVerified{0};
    float maxPositionErrorMeters{0.0f};
};

class DemoRecorder {
public:
    void beginRecording(
        std::string_view mapName,
        float tickRateHz,
        const glm::vec3& spawnFeetPosition,
        float spawnYawDegrees,
        float spawnPitchDegrees = 0.0f);

    void recordTick(
        const input::PlayerCommand& command,
        const glm::vec3& feetPosition,
        const glm::vec3& velocity,
        std::uint32_t eventFlags = DemoEvent_None);

    void stopRecording() noexcept { recording_ = false; }

    [[nodiscard]] std::vector<std::uint8_t> serializeToBytes() const;
    [[nodiscard]] bool saveToFile(const std::string& filePath) const;

    [[nodiscard]] bool isRecording() const noexcept { return recording_; }
    [[nodiscard]] const DemoHeader& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<DemoTickFrame>& frames() const noexcept { return frames_; }

private:
    bool recording_{false};
    DemoHeader header_{};
    std::vector<DemoTickFrame> frames_;
};

class DemoPlayer {
public:
    [[nodiscard]] bool deserializeFromBytes(const std::vector<std::uint8_t>& bytes);
    [[nodiscard]] bool loadFromFile(const std::string& filePath);

    void resetPlayback() noexcept;
    void seekToTick(std::size_t tickIndex) noexcept;
    void setPlaybackSpeed(float speedMultiplier) noexcept;

    [[nodiscard]] const DemoTickFrame* stepPlayback(float dt) noexcept;

    [[nodiscard]] DeterminismReport verifyDeterminism(
        const physics::CollisionWorld& world,
        float maxAllowedErrorMeters = 1e-4f) const;

    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }
    [[nodiscard]] bool isFinished() const noexcept { return currentTick_ >= frames_.size(); }
    [[nodiscard]] std::size_t currentTickIndex() const noexcept { return currentTick_; }
    [[nodiscard]] std::size_t totalTicks() const noexcept { return frames_.size(); }
    [[nodiscard]] float playbackSpeed() const noexcept { return playbackSpeed_; }
    [[nodiscard]] const DemoHeader& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<DemoTickFrame>& frames() const noexcept { return frames_; }

private:
    bool loaded_{false};
    DemoHeader header_{};
    std::vector<DemoTickFrame> frames_;
    std::size_t currentTick_{0};
    float playbackSpeed_{1.0f};
    float timeAccumulator_{0.0f};
};

} // namespace classicfps::replay
