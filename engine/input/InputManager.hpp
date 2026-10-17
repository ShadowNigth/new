#pragma once

#include <cstdint>

namespace classicfps::core {
class Config;
}

namespace classicfps::platform {
class Window;
}

namespace classicfps::input {

struct InputConfig {
    float mouseSensitivity{0.15f};
    bool invertPitch{false};
    float keyboardTurnSpeedDegPerSec{100.0f};
};

// Deterministic movement and combat command suitable for local simulation and network prediction
struct PlayerCommand {
    std::uint32_t sequence{0};
    float forwardMove{0.0f}; // [-1.0, +1.0]
    float rightMove{0.0f};   // [-1.0, +1.0]
    float viewYawDegrees{-90.0f};
    float viewPitchDegrees{0.0f};
    bool jump{false};
    bool crouch{false};
    bool walk{false};
    bool fireHeld{false};
    bool fireJustPressed{false};
    bool reload{false};
    int weaponSlotSelect{-1};
};

class InputManager {
public:
    InputManager() = default;

    void loadFromConfig(const core::Config& config) noexcept;
    void setConfig(const InputConfig& cfg) noexcept { config_ = cfg; }
    [[nodiscard]] const InputConfig& config() const noexcept { return config_; }

    void setViewAngles(float yawDegrees, float pitchDegrees) noexcept;
    [[nodiscard]] float viewYawDegrees() const noexcept { return viewYawDegrees_; }
    [[nodiscard]] float viewPitchDegrees() const noexcept { return viewPitchDegrees_; }

    void setCursorLocked(platform::Window& window, bool locked) noexcept;
    [[nodiscard]] bool isCursorLocked() const noexcept { return cursorLocked_; }

    // Polls current keyboard/mouse state from Window and produces a deterministic PlayerCommand
    [[nodiscard]] PlayerCommand buildCommand(const platform::Window& window, float dtSeconds) noexcept;

    [[nodiscard]] bool hasReceivedManualInput() const noexcept { return manualInputActive_; }

private:
    InputConfig config_{};
    std::uint32_t nextSequence_{1};
    float viewYawDegrees_{-90.0f};
    float viewPitchDegrees_{0.0f};
    bool cursorLocked_{false};
    bool manualInputActive_{false};
};

} // namespace classicfps::input
