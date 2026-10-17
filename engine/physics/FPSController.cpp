#include "engine/physics/FPSController.hpp"
#include "engine/core/Config.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace classicfps::physics {

namespace {

[[nodiscard]] glm::vec3 clipVelocity(
    const glm::vec3& inVel,
    const glm::vec3& normal,
    float overbounce = 1.001f) noexcept {
    const float backoff = glm::dot(inVel, normal) * overbounce;
    glm::vec3 out = inVel - normal * backoff;
    // Clamp tiny residual velocity toward the plane
    const float adjust = glm::dot(out, normal);
    if (adjust < 0.0f) {
        out -= normal * adjust;
    }
    return out;
}

} // namespace

void FPSController::loadFromConfig(const core::Config& config) noexcept {
    config_.gravity = std::max(0.1f, config.getFloat("sv_gravity", 20.0f));
    config_.jumpSpeed = std::max(0.1f, config.getFloat("pm_jump_speed", 6.8f));
    config_.runSpeed = std::max(0.5f, config.getFloat("pm_run_speed", 6.5f));
    config_.walkSpeed = std::max(0.5f, config.getFloat("pm_walk_speed", 3.4f));
    config_.crouchSpeed = std::max(0.5f, config.getFloat("pm_crouch_speed", 2.4f));
    config_.accelerate = std::max(1.0f, config.getFloat("sv_accelerate", 10.0f));
    config_.airAccelerate = std::max(0.1f, config.getFloat("sv_airaccelerate", 2.5f));
    config_.friction = std::max(0.1f, config.getFloat("sv_friction", 6.5f));
    config_.stopSpeed = std::max(0.1f, config.getFloat("sv_stopspeed", 1.8f));
    config_.stepHeight = std::max(0.05f, config.getFloat("pm_step_height", 0.45f));
    config_.standingEyeHeight = std::max(0.5f, config.getFloat("pm_eye_height_stand", 1.62f));
    config_.crouchEyeHeight = std::max(0.3f, config.getFloat("pm_eye_height_crouch", 0.85f));
}

void FPSController::spawn(
    const glm::vec3& feetPosition,
    float yawDegrees,
    float pitchDegrees) noexcept {
    state_ = PlayerState{};
    state_.feetPosition = feetPosition;
    state_.prevFeetPosition = feetPosition;
    state_.yawDegrees = yawDegrees;
    state_.pitchDegrees = std::clamp(pitchDegrees, -89.0f, 89.0f);
    state_.eyeHeight = config_.standingEyeHeight;
    state_.prevEyeHeight = config_.standingEyeHeight;
}

void FPSController::simulateStep(
    const input::PlayerCommand& cmd,
    const CollisionWorld& world,
    float fixedDt) noexcept {
    const float dt = std::max(0.0001f, fixedDt);

    state_.prevFeetPosition = state_.feetPosition;
    state_.prevEyeHeight = state_.eyeHeight;
    state_.yawDegrees = cmd.viewYawDegrees;
    state_.pitchDegrees = std::clamp(cmd.viewPitchDegrees, -89.0f, 89.0f);
    state_.lastCommandSequence = cmd.sequence;

    updateCrouch(cmd.crouch, world, dt);
    categorizePosition(world, false);

    const bool wasOnGround = state_.onGround;

    // Jump check (must be on ground)
    if (cmd.jump && state_.onGround) {
        state_.onGround = false;
        state_.velocity.y = config_.jumpSpeed;
    }

    // Compute horizontal wish direction from yaw angle
    const float yawRad = glm::radians(state_.yawDegrees);
    const glm::vec3 forwardDir(std::cos(yawRad), 0.0f, std::sin(yawRad));
    const glm::vec3 rightDir(-forwardDir.z, 0.0f, forwardDir.x);

    glm::vec3 wishVel = forwardDir * cmd.forwardMove + rightDir * cmd.rightMove;
    wishVel.y = 0.0f;

    float maxMoveSpeed = config_.runSpeed;
    if (state_.crouching) {
        maxMoveSpeed = config_.crouchSpeed;
    } else if (cmd.walk) {
        maxMoveSpeed = config_.walkSpeed;
    }

    const float wishMag = glm::length(wishVel);
    glm::vec3 wishDir(0.0f);
    float wishSpeed = 0.0f;
    if (wishMag > 1e-4f) {
        wishDir = wishVel / wishMag;
        wishSpeed = std::min(wishMag, 1.0f) * maxMoveSpeed;
    }

    if (state_.onGround) {
        state_.velocity.y = 0.0f;
        applyFriction(dt);
        applyAcceleration(wishDir, wishSpeed, config_.accelerate, dt);
        stepSlideMove(world, dt);
        categorizePosition(world, wasOnGround && !cmd.jump);
    } else {
        // Half-gravity integration before & after air move for framerate-independent jump arcs
        state_.velocity.y -= 0.5f * config_.gravity * dt;
        applyAcceleration(wishDir, wishSpeed, config_.airAccelerate, dt);
        slideMove(world, dt);
        state_.velocity.y -= 0.5f * config_.gravity * dt;
        categorizePosition(world, false);
    }
}

void FPSController::syncCamera(
    renderer::FirstPersonCamera& camera,
    double interpolationAlpha) const noexcept {
    camera.setPosition(interpolatedEyePosition(interpolationAlpha));
    camera.setAngles(state_.yawDegrees, state_.pitchDegrees);
}

glm::vec3 FPSController::interpolatedEyePosition(double alpha) const noexcept {
    const float t = static_cast<float>(std::clamp(alpha, 0.0, 1.0));
    const glm::vec3 feet = glm::mix(state_.prevFeetPosition, state_.feetPosition, t);
    const float eyeH = std::lerp(state_.prevEyeHeight, state_.eyeHeight, t);
    return glm::vec3(feet.x, feet.y + eyeH, feet.z);
}

void FPSController::updateCrouch(
    bool wishCrouch,
    const CollisionWorld& world,
    float dt) noexcept {
    if (wishCrouch) {
        state_.crouching = true;
    } else if (state_.crouching) {
        // Verify there is enough vertical headroom to stand up safely
        if (!world.checkHullOverlap(
                state_.feetPosition, config_.halfWidth, config_.standingHeight)) {
            state_.crouching = false;
        }
    }

    const float targetEye = state_.crouching
        ? config_.crouchEyeHeight
        : config_.standingEyeHeight;
    const float blend = std::clamp(config_.eyeTransitionSpeed * dt, 0.0f, 1.0f);
    state_.eyeHeight = std::lerp(state_.eyeHeight, targetEye, blend);
}

void FPSController::categorizePosition(
    const CollisionWorld& world,
    bool stickToSlope) noexcept {
    // Only detach from ground if moving upward fast from a jump impulse
    if (!stickToSlope && state_.velocity.y > config_.jumpSpeed * 0.65f) {
        state_.onGround = false;
        return;
    }

    const float probeDist = stickToSlope ? (config_.stepHeight + 0.08f) : 0.08f;
    float surfaceY = 0.0f;
    glm::vec3 surfaceNormal(0.0f, 1.0f, 0.0f);
    std::uint32_t colliderId = 0;

    if (world.querySupportSurface(
            state_.feetPosition,
            config_.halfWidth,
            currentHullHeight(),
            probeDist,
            surfaceY,
            surfaceNormal,
            colliderId)) {
        if (surfaceNormal.y >= config_.maxSlopeCos) {
            state_.feetPosition.y = surfaceY;
            state_.onGround = true;
            state_.groundNormal = surfaceNormal;
            if (state_.velocity.y < 0.0f) {
                state_.velocity.y = 0.0f;
            }
            return;
        }
    }

    state_.onGround = false;
    state_.groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
}

void FPSController::applyFriction(float dt) noexcept {
    const glm::vec3 horiz(state_.velocity.x, 0.0f, state_.velocity.z);
    const float speed = glm::length(horiz);
    if (speed < 1e-4f) {
        state_.velocity.x = 0.0f;
        state_.velocity.z = 0.0f;
        return;
    }

    const float control = (speed < config_.stopSpeed) ? config_.stopSpeed : speed;
    const float drop = control * config_.friction * dt;
    const float newSpeed = std::max(0.0f, speed - drop) / speed;

    state_.velocity.x *= newSpeed;
    state_.velocity.z *= newSpeed;
}

void FPSController::applyAcceleration(
    const glm::vec3& wishDir,
    float wishSpeed,
    float accel,
    float dt) noexcept {
    if (wishSpeed <= 0.0f) {
        return;
    }
    const float currentSpeed = glm::dot(state_.velocity, wishDir);
    const float addSpeed = wishSpeed - currentSpeed;
    if (addSpeed <= 0.0f) {
        return;
    }
    const float accelSpeed = std::min(accel * dt * wishSpeed, addSpeed);
    state_.velocity += wishDir * accelSpeed;
}

void FPSController::slideMove(const CollisionWorld& world, float dt) noexcept {
    constexpr int kMaxBumps = 4;
    std::array<glm::vec3, kMaxBumps + 1> planes{};
    int numPlanes = 0;

    float timeLeft = dt;
    const float hullH = currentHullHeight();

    for (int bump = 0; bump < kMaxBumps && timeLeft > 1e-5f; ++bump) {
        if (glm::dot(state_.velocity, state_.velocity) < 1e-8f) {
            break;
        }

        const glm::vec3 targetPos = state_.feetPosition + state_.velocity * timeLeft;
        const SweepHit trace = world.sweepPlayerHull(
            state_.feetPosition, targetPos, config_.halfWidth, hullH);

        if (trace.fraction > 0.0f) {
            state_.feetPosition = trace.endFeetPos;
        }

        if (!trace.hit) {
            break;
        }

        timeLeft -= timeLeft * trace.fraction;

        // If hitting a low ceiling while moving upward, zero out upward velocity immediately
        if (trace.normal.y < -0.7f && state_.velocity.y > 0.0f) {
            state_.velocity.y = 0.0f;
        }

        if (numPlanes < static_cast<int>(planes.size())) {
            planes[static_cast<std::size_t>(numPlanes++)] = trace.normal;
        }

        // Clip velocity against all contacted planes
        glm::vec3 newVel = state_.velocity;
        for (int i = 0; i < numPlanes; ++i) {
            newVel = clipVelocity(
                state_.velocity, planes[static_cast<std::size_t>(i)], 1.001f);
            int j = 0;
            for (; j < numPlanes; ++j) {
                if (j != i && glm::dot(newVel, planes[static_cast<std::size_t>(j)]) < 0.0f) {
                    break;
                }
            }
            if (j == numPlanes) {
                break;
            }
            if (i == numPlanes - 1) {
                if (numPlanes == 2) {
                    const glm::vec3 dir = glm::cross(planes[0], planes[1]);
                    const float d = glm::dot(dir, state_.velocity);
                    newVel = dir * d;
                } else {
                    newVel = glm::vec3(0.0f);
                }
            }
        }

        state_.velocity = newVel;
    }
}

void FPSController::stepSlideMove(const CollisionWorld& world, float dt) noexcept {
    const glm::vec3 startPos = state_.feetPosition;
    const glm::vec3 startVel = state_.velocity;
    const float hullH = currentHullHeight();

    // 1. Try regular slide move first (handles flat ground and walkable ramps)
    slideMove(world, dt);
    categorizePosition(world, true);
    const glm::vec3 regularPos = state_.feetPosition;
    const glm::vec3 regularVel = state_.velocity;

    // 2. Try stepping up by stepHeight, moving across, then stepping back down (for stairs and curbs)
    state_.feetPosition = startPos;
    state_.velocity = startVel;

    const glm::vec3 upTarget = startPos + glm::vec3(0.0f, config_.stepHeight, 0.0f);
    const SweepHit upTrace = world.sweepPlayerHull(startPos, upTarget, config_.halfWidth, hullH);
    state_.feetPosition = upTrace.endFeetPos;

    slideMove(world, dt);

    const glm::vec3 downTarget = state_.feetPosition - glm::vec3(0.0f, config_.stepHeight + 0.05f, 0.0f);
    const SweepHit downTrace = world.sweepPlayerHull(
        state_.feetPosition, downTarget, config_.halfWidth, hullH);
    if (!downTrace.hit || downTrace.normal.y >= config_.maxSlopeCos) {
        state_.feetPosition = downTrace.endFeetPos;
    }
    categorizePosition(world, true);

    // Prefer regularPos unless stepping up made significantly more horizontal progress (> 5%)
    const glm::vec2 regDelta(regularPos.x - startPos.x, regularPos.z - startPos.z);
    const glm::vec2 stepDelta(state_.feetPosition.x - startPos.x, state_.feetPosition.z - startPos.z);

    if (glm::dot(regDelta, regDelta) * 1.15f >= glm::dot(stepDelta, stepDelta)) {
        state_.feetPosition = regularPos;
        state_.velocity = regularVel;
    }
}

} // namespace classicfps::physics
