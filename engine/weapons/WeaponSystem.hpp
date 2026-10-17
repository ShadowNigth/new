#pragma once

#include "engine/physics/CollisionWorld.hpp"
#include "engine/weapons/WeaponDef.hpp"

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::weapons {

enum class WeaponState : std::uint8_t {
    Idle = 0,
    Firing = 1,
    Reloading = 2,
    Switching = 3,
    Empty = 4
};

struct WeaponSimulationInput {
    bool fireHeld{false};
    bool fireJustPressed{false};
    bool reloadPressed{false};
    int requestedWeaponSlot{-1}; // -1 = no switch requested

    glm::vec3 aimOrigin{0.0f, 1.6f, 0.0f};
    glm::vec3 aimForward{0.0f, 0.0f, -1.0f};
    glm::vec3 aimRight{1.0f, 0.0f, 0.0f};
    glm::vec3 aimUp{0.0f, 1.0f, 0.0f};

    float playerHorizontalSpeed{0.0f};
    bool playerOnGround{true};
    bool playerCrouching{false};
};

struct ShotImpactEvent {
    bool hitWorld{false};
    bool fromProjectile{false};
    glm::vec3 origin{0.0f};
    glm::vec3 impactPoint{0.0f};
    glm::vec3 impactNormal{0.0f, 1.0f, 0.0f};
    float distance{0.0f};
    float damageDealt{0.0f};
    std::uint32_t colliderId{0};
    std::string colliderTag;
    std::string hitZone; // "head", "torso", or "world"
};

struct ActiveProjectile {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float gravity{9.8f};
    float damage{85.0f};
    float remainingLifetime{4.0f};
};

struct WeaponSlotState {
    WeaponDefinition def{};
    int magazineAmmo{30};
    int reserveAmmo{90};
};

class WeaponSystem {
public:
    WeaponSystem() = default;

    void initialize(const WeaponLibrary& library);

    void update(
        const WeaponSimulationInput& input,
        const physics::CollisionWorld& collisionWorld,
        float fixedDt);

    bool selectWeaponSlot(std::size_t slotIndex);
    bool startReload();
    void refillAllAmmo();

    [[nodiscard]] WeaponState state() const noexcept { return state_; }
    [[nodiscard]] std::string_view stateName() const noexcept;
    [[nodiscard]] float stateProgress() const noexcept;

    [[nodiscard]] std::size_t activeSlotIndex() const noexcept { return activeSlot_; }
    [[nodiscard]] const WeaponSlotState* activeWeapon() const noexcept;
    [[nodiscard]] int currentMagazineAmmo() const noexcept;
    [[nodiscard]] int currentReserveAmmo() const noexcept;

    [[nodiscard]] float recoilPitchDegrees() const noexcept { return recoilPitch_; }
    [[nodiscard]] float recoilYawDegrees() const noexcept { return recoilYaw_; }
    [[nodiscard]] float muzzleFlashTimer() const noexcept { return muzzleFlashTimer_; }
    [[nodiscard]] float computeSpreadDegrees(
        float horizontalSpeed,
        bool onGround,
        bool crouching) const noexcept;

    [[nodiscard]] std::uint32_t totalShotsFired() const noexcept { return totalShotsFired_; }
    [[nodiscard]] const std::vector<ShotImpactEvent>& recentImpacts() const noexcept {
        return recentImpacts_;
    }
    [[nodiscard]] const std::vector<ActiveProjectile>& activeProjectiles() const noexcept {
        return projectiles_;
    }
    [[nodiscard]] const std::vector<std::string>& pendingSoundEvents() const noexcept {
        return soundEvents_;
    }
    void clearPendingSoundEvents() noexcept { soundEvents_.clear(); }

private:
    void executeShot(
        const WeaponSimulationInput& input,
        const physics::CollisionWorld& collisionWorld);
    void updateProjectiles(const physics::CollisionWorld& collisionWorld, float dt);
    void recordImpact(
        const glm::vec3& origin,
        const physics::RaycastHit& hit,
        float baseDamage,
        bool fromProjectile);

    std::vector<WeaponSlotState> slots_;
    std::size_t activeSlot_{0};
    WeaponState state_{WeaponState::Idle};
    float stateTimer_{0.0f};
    float stateDuration_{0.0f};
    float recoilPitch_{0.0f};
    float recoilYaw_{0.0f};
    float muzzleFlashTimer_{0.0f};
    bool triggerReleasedSinceLastShot_{true};
    std::uint32_t totalShotsFired_{0};
    std::uint32_t rngState_{0xC0FFEE12U};

    std::vector<ShotImpactEvent> recentImpacts_;
    std::vector<ActiveProjectile> projectiles_;
    std::vector<std::string> soundEvents_;
};

} // namespace classicfps::weapons
