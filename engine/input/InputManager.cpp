#include "engine/input/InputManager.hpp"
#include "engine/core/Config.hpp"
#include "engine/platform/Window.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

namespace classicfps::input {

void InputManager::loadFromConfig(const core::Config& config) noexcept {
    config_.mouseSensitivity = std::clamp(config.getFloat("sensitivity", 0.15f), 0.01f, 5.0f);
    config_.invertPitch = config.getBool("m_invert_pitch", false);
    config_.keyboardTurnSpeedDegPerSec =
        std::clamp(config.getFloat("cl_yawspeed", 100.0f), 10.0f, 360.0f);
}

void InputManager::setViewAngles(float yawDegrees, float pitchDegrees) noexcept {
    viewYawDegrees_ = yawDegrees;
    viewPitchDegrees_ = std::clamp(pitchDegrees, -89.0f, 89.0f);
}

void InputManager::setCursorLocked(platform::Window& window, bool locked) noexcept {
    cursorLocked_ = locked;
    if (window.isInitialized()) {
        SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
    }
}

PlayerCommand InputManager::buildCommand(const platform::Window& window, float dtSeconds) noexcept {
    const auto& winIn = window.cameraInput();
    if (winIn.anyManualInput) {
        manualInputActive_ = true;
    }

    const float pitchSign = config_.invertPitch ? 1.0f : -1.0f;
    viewYawDegrees_ += winIn.mouseDeltaX * config_.mouseSensitivity +
                       winIn.keyTurnYaw * config_.keyboardTurnSpeedDegPerSec * dtSeconds;
    viewPitchDegrees_ += winIn.mouseDeltaY * config_.mouseSensitivity * pitchSign +
                         winIn.keyLookPitch * (config_.keyboardTurnSpeedDegPerSec * 0.7f) * dtSeconds;
    viewPitchDegrees_ = std::clamp(viewPitchDegrees_, -89.0f, 89.0f);

    while (viewYawDegrees_ > 180.0f) viewYawDegrees_ -= 360.0f;
    while (viewYawDegrees_ < -180.0f) viewYawDegrees_ += 360.0f;

    PlayerCommand cmd{};
    cmd.sequence = nextSequence_++;
    cmd.forwardMove = std::clamp(winIn.forwardAxis, -1.0f, 1.0f);
    cmd.rightMove = std::clamp(winIn.rightAxis, -1.0f, 1.0f);
    cmd.viewYawDegrees = viewYawDegrees_;
    cmd.viewPitchDegrees = viewPitchDegrees_;
    cmd.jump = (winIn.upAxis > 0.5f);
    cmd.crouch = (winIn.upAxis < -0.5f);
    cmd.walk = winIn.sprint;
    cmd.fireHeld = winIn.fireHeld;
    cmd.fireJustPressed = winIn.fireJustPressed;
    cmd.reload = winIn.reloadPressed;
    cmd.weaponSlotSelect = winIn.weaponSlotSelect;

    return cmd;
}

} // namespace classicfps::input
