#pragma once

#include "engine/input/InputManager.hpp"
#include "engine/physics/CollisionWorld.hpp"
#include "engine/renderer/Camera.hpp"

#include <cstdint>
#include <glm/glm.hpp>

namespace classicfps::core {
class Config;
}

namespace classicfps::physics {

struct MovementConfig {
    float gravity{20.0f};
    float jumpSpeed{6.8f};
    float runSpeed{6.5f};
    float walkSpeed{3.4f};
    float crouchSpeed{2.4f};
    float accelerate{10.0f};
    float airAccelerate{2.5f};
    float friction{6.5f};
    float stopSpeed{1.8f};
    float stepHeight{0.45f};
    float maxSlopeCos{0.70f}; // ~45.5 degrees

    float halfWidth{0.35f};
    float standingHeight{1.80f};
    float crouchHeight{1.00f};
    float standingEyeHeight{1.62f};
    float crouchEyeHeight{0.85f};
    float eyeTransitionSpeed{10.0f};
};

struct PlayerState {
    glm::vec3 feetPosition{0.0f, 0.0f, 4.8f};
    glm::vec3 prevFeetPosition{0.0f, 0.0f, 4.8f};
    glm::vec3 velocity{0.0f};
    float yawDegrees{-90.0f};
    float pitchDegrees{0.0f};
    bool onGround{false};
    bool crouching{false};
    float eyeHeight{1.62f};
    float prevEyeHeight{1.62f};
    glm::vec3 groundNormal{0.0f, 1.0f, 0.0f};
    std::uint32_t lastCommandSequence{0};
};

class FPSController {
public:
    FPSController() = default;

    void loadFromConfig(const core::Config& config) noexcept;
    void setConfig(const MovementConfig& cfg) noexcept { config_ = cfg; }
    [[nodiscard]] const MovementConfig& config() const noexcept { return config_; }

    void spawn(const glm::vec3& feetPosition, float yawDegrees = -90.0f, float pitchDegrees = 0.0f) noexcept;

    void simulateStep(
        const input::PlayerCommand& cmd,
        const CollisionWorld& world,
        float fixedDt) noexcept;

    void syncCamera(renderer::FirstPersonCamera& camera, double interpolationAlpha) const noexcept;

    [[nodiscard]] const PlayerState& state() const noexcept { return state_; }
    [[nodiscard]] PlayerState& state() noexcept { return state_; }
    [[nodiscard]] float currentHullHeight() const noexcept {
        return state_.crouching ? config_.crouchHeight : config_.standingHeight;
    }
    [[nodiscard]] glm::vec3 interpolatedEyePosition(double alpha) const noexcept;

private:
    void updateCrouch(bool wishCrouch, const CollisionWorld& world, float dt) noexcept;
    void categorizePosition(const CollisionWorld& world, bool stickToSlope) noexcept;
    void applyFriction(float dt) noexcept;
    void applyAcceleration(const glm::vec3& wishDir, float wishSpeed, float accel, float dt) noexcept;
    void slideMove(const CollisionWorld& world, float dt) noexcept;
    void stepSlideMove(const CollisionWorld& world, float dt) noexcept;

    MovementConfig config_{};
    PlayerState state_{};
};

} // namespace classicfps::physics
