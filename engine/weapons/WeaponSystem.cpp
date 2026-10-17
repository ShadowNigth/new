#include "engine/weapons/WeaponSystem.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace classicfps::weapons {

namespace {

// Deterministic xorshift32 PRNG for reproducible weapon spread (important for future netcode)
[[nodiscard]] float nextRandomSignedUnit(std::uint32_t& state) noexcept {
    if (state == 0U) {
        state = 0x9E3779B9U;
    }
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    const float u01 = static_cast<float>(state & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
    return u01 * 2.0f - 1.0f;
}

} // namespace

void WeaponSystem::initialize(const WeaponLibrary& library) {
    slots_.clear();
    for (const WeaponDefinition& def : library.definitions()) {
        slots_.push_back(WeaponSlotState{
            .def = def,
            .magazineAmmo = def.magazineCapacity,
            .reserveAmmo = def.initialReserveAmmo
        });
    }

    if (slots_.empty()) {
        WeaponLibrary fallback;
        for (const WeaponDefinition& def : fallback.definitions()) {
            slots_.push_back(WeaponSlotState{
                .def = def,
                .magazineAmmo = def.magazineCapacity,
                .reserveAmmo = def.initialReserveAmmo
            });
        }
    }

    activeSlot_ = 0;
    state_ = WeaponState::Idle;
    stateTimer_ = 0.0f;
    stateDuration_ = 0.0f;
    recoilPitch_ = 0.0f;
    recoilYaw_ = 0.0f;
    muzzleFlashTimer_ = 0.0f;
    triggerReleasedSinceLastShot_ = true;
    totalShotsFired_ = 0;
    recentImpacts_.clear();
    projectiles_.clear();
    soundEvents_.clear();
}

void WeaponSystem::update(
    const WeaponSimulationInput& input,
    const physics::CollisionWorld& collisionWorld,
    float fixedDt) {
    const float dt = std::max(0.0001f, fixedDt);

    if (slots_.empty()) {
        return;
    }

    if (!input.fireHeld) {
        triggerReleasedSinceLastShot_ = true;
    }

    // 1. Update active physical projectiles
    updateProjectiles(collisionWorld, dt);

    // 2. Recover recoil smoothly toward 0
    const WeaponSlotState& currentSlot = slots_[activeSlot_];
    const float recovery = currentSlot.def.recoilRecoverySpeed * dt;
    recoilPitch_ = std::lerp(recoilPitch_, 0.0f, std::clamp(recovery, 0.0f, 1.0f));
    recoilYaw_ = std::lerp(recoilYaw_, 0.0f, std::clamp(recovery, 0.0f, 1.0f));
    muzzleFlashTimer_ = std::max(0.0f, muzzleFlashTimer_ - dt);

    // 3. Check weapon switch request
    if (input.requestedWeaponSlot >= 0 &&
        static_cast<std::size_t>(input.requestedWeaponSlot) < slots_.size() &&
        static_cast<std::size_t>(input.requestedWeaponSlot) != activeSlot_) {
        (void)selectWeaponSlot(static_cast<std::size_t>(input.requestedWeaponSlot));
    }

    // 4. Advance current weapon state machine
    if (state_ == WeaponState::Switching) {
        stateTimer_ -= dt;
        if (stateTimer_ <= 0.0f) {
            stateTimer_ = 0.0f;
            state_ = (slots_[activeSlot_].magazineAmmo > 0)
                ? WeaponState::Idle
                : WeaponState::Empty;
        }
        return;
    }

    if (state_ == WeaponState::Reloading) {
        stateTimer_ -= dt;
        if (stateTimer_ <= 0.0f) {
            stateTimer_ = 0.0f;
            WeaponSlotState& slot = slots_[activeSlot_];
            const int needed = std::max(0, slot.def.magazineCapacity - slot.magazineAmmo);
            const int transferred = std::min(needed, slot.reserveAmmo);
            slot.magazineAmmo += transferred;
            slot.reserveAmmo -= transferred;
            state_ = (slot.magazineAmmo > 0) ? WeaponState::Idle : WeaponState::Empty;
        }
        return;
    }

    if (state_ == WeaponState::Firing) {
        stateTimer_ -= dt;
        if (stateTimer_ <= 0.0f) {
            stateTimer_ = 0.0f;
            state_ = (slots_[activeSlot_].magazineAmmo > 0)
                ? WeaponState::Idle
                : WeaponState::Empty;
        }
    }

    // 5. Handle manual reload request when Idle or Empty
    if (input.reloadPressed && (state_ == WeaponState::Idle || state_ == WeaponState::Empty)) {
        if (startReload()) {
            return;
        }
    }

    // 6. Handle firing request when Idle or Empty
    const WeaponDefinition& def = slots_[activeSlot_].def;
    const bool wantsToFire = (def.fireMode == FireMode::Automatic)
        ? input.fireHeld
        : (input.fireJustPressed || (input.fireHeld && triggerReleasedSinceLastShot_));

    if (wantsToFire && state_ == WeaponState::Empty) {
        triggerReleasedSinceLastShot_ = false;
        soundEvents_.push_back(def.soundEmpty);
        // Automatically start reload if reserve ammo is available
        if (slots_[activeSlot_].reserveAmmo > 0) {
            (void)startReload();
        }
        return;
    }

    if (wantsToFire && state_ == WeaponState::Idle) {
        if (slots_[activeSlot_].magazineAmmo <= 0) {
            state_ = WeaponState::Empty;
            soundEvents_.push_back(def.soundEmpty);
            return;
        }

        triggerReleasedSinceLastShot_ = false;
        executeShot(input, collisionWorld);
    }
}

bool WeaponSystem::selectWeaponSlot(std::size_t slotIndex) {
    if (slotIndex >= slots_.size() || slotIndex == activeSlot_) {
        return false;
    }
    activeSlot_ = slotIndex;
    state_ = WeaponState::Switching;
    stateDuration_ = slots_[activeSlot_].def.switchTimeSeconds;
    stateTimer_ = stateDuration_;
    recoilPitch_ = 0.0f;
    recoilYaw_ = 0.0f;
    return true;
}

bool WeaponSystem::startReload() {
    if (slots_.empty()) {
        return false;
    }
    WeaponSlotState& slot = slots_[activeSlot_];
    if (state_ == WeaponState::Reloading || state_ == WeaponState::Switching) {
        return false;
    }
    if (slot.magazineAmmo >= slot.def.magazineCapacity || slot.reserveAmmo <= 0) {
        return false;
    }

    state_ = WeaponState::Reloading;
    stateDuration_ = slot.def.reloadTimeSeconds;
    stateTimer_ = stateDuration_;
    soundEvents_.push_back(slot.def.soundReload);
    return true;
}

void WeaponSystem::refillAllAmmo() {
    for (WeaponSlotState& slot : slots_) {
        slot.magazineAmmo = slot.def.magazineCapacity;
        slot.reserveAmmo = slot.def.initialReserveAmmo;
    }
    if (state_ == WeaponState::Empty) {
        state_ = WeaponState::Idle;
    }
}

std::string_view WeaponSystem::stateName() const noexcept {
    switch (state_) {
        case WeaponState::Idle:      return "Idle";
        case WeaponState::Firing:    return "Firing";
        case WeaponState::Reloading: return "Reloading";
        case WeaponState::Switching: return "Switching";
        case WeaponState::Empty:     return "Empty";
    }
    return "Idle";
}

float WeaponSystem::stateProgress() const noexcept {
    if (stateDuration_ <= 1e-5f) {
        return 0.0f;
    }
    return std::clamp(1.0f - (stateTimer_ / stateDuration_), 0.0f, 1.0f);
}

const WeaponSlotState* WeaponSystem::activeWeapon() const noexcept {
    if (activeSlot_ >= slots_.size()) {
        return nullptr;
    }
    return &slots_[activeSlot_];
}

int WeaponSystem::currentMagazineAmmo() const noexcept {
    const auto* w = activeWeapon();
    return (w != nullptr) ? w->magazineAmmo : 0;
}

int WeaponSystem::currentReserveAmmo() const noexcept {
    const auto* w = activeWeapon();
    return (w != nullptr) ? w->reserveAmmo : 0;
}

float WeaponSystem::computeSpreadDegrees(
    float horizontalSpeed,
    bool onGround,
    bool crouching) const noexcept {
    const auto* w = activeWeapon();
    if (w == nullptr) {
        return 0.0f;
    }
    float spread = w->def.baseSpreadDegrees;
    if (!onGround) {
        spread *= w->def.airSpreadMultiplier;
    } else if (crouching) {
        spread *= w->def.crouchSpreadMultiplier;
    }

    if (horizontalSpeed > 0.5f) {
        const float speedFactor = std::clamp(horizontalSpeed / 6.5f, 0.0f, 1.5f);
        spread *= (1.0f + (w->def.moveSpreadMultiplier - 1.0f) * speedFactor);
    }

    // Sustained recoil slightly widens spread like CS 1.6
    spread += recoilPitch_ * 0.25f;
    return spread;
}

void WeaponSystem::executeShot(
    const WeaponSimulationInput& input,
    const physics::CollisionWorld& collisionWorld) {
    WeaponSlotState& slot = slots_[activeSlot_];
    const WeaponDefinition& def = slot.def;

    --slot.magazineAmmo;
    ++totalShotsFired_;

    state_ = WeaponState::Firing;
    stateDuration_ = def.fireIntervalSeconds();
    stateTimer_ = stateDuration_;
    muzzleFlashTimer_ = 0.06f;
    soundEvents_.push_back(def.soundFire);

    // Compute spread perturbation in radians
    const float spreadDeg = computeSpreadDegrees(
        input.playerHorizontalSpeed, input.playerOnGround, input.playerCrouching);
    const float spreadRad = glm::radians(spreadDeg);
    const float randX = nextRandomSignedUnit(rngState_) * spreadRad;
    const float randY = nextRandomSignedUnit(rngState_) * spreadRad;

    const glm::vec3 shotDir = glm::normalize(
        input.aimForward + input.aimRight * randX + input.aimUp * randY);

    if (def.fireType == FireType::HitscanRaycast) {
        const physics::RaycastHit hit = collisionWorld.raycast(
            input.aimOrigin, shotDir, def.maxRangeMeters);
        recordImpact(input.aimOrigin, hit, def.damage, false);
    } else {
        projectiles_.push_back(ActiveProjectile{
            .position = input.aimOrigin + shotDir * 0.4f,
            .velocity = shotDir * def.projectileSpeed,
            .gravity = def.projectileGravity,
            .damage = def.damage,
            .remainingLifetime = 4.0f
        });
    }

    // Apply recoil kick after shot
    recoilPitch_ = std::min(12.0f, recoilPitch_ + def.recoilPitchDegrees);
    recoilYaw_ += nextRandomSignedUnit(rngState_) * def.recoilYawDegrees;
}

void WeaponSystem::updateProjectiles(const physics::CollisionWorld& collisionWorld, float dt) {
    for (auto it = projectiles_.begin(); it != projectiles_.end();) {
        it->remainingLifetime -= dt;
        if (it->remainingLifetime <= 0.0f) {
            it = projectiles_.erase(it);
            continue;
        }

        it->velocity.y -= it->gravity * dt;
        const glm::vec3 stepDelta = it->velocity * dt;
        const float stepLen = glm::length(stepDelta);

        if (stepLen > 1e-5f) {
            const physics::RaycastHit hit = collisionWorld.raycast(
                it->position, stepDelta / stepLen, stepLen);
            if (hit.hit) {
                recordImpact(it->position, hit, it->damage, true);
                it = projectiles_.erase(it);
                continue;
            }
        }

        it->position += stepDelta;
        ++it;
    }
}

void WeaponSystem::recordImpact(
    const glm::vec3& origin,
    const physics::RaycastHit& hit,
    float baseDamage,
    bool fromProjectile) {
    ShotImpactEvent ev{};
    ev.origin = origin;
    ev.fromProjectile = fromProjectile;
    ev.hitWorld = hit.hit;

    if (hit.hit) {
        ev.impactPoint = hit.point;
        ev.impactNormal = hit.normal;
        ev.distance = hit.distance;
        ev.colliderId = hit.colliderId;
        ev.colliderTag = hit.tag;

        if (hit.tag.find("head") != std::string::npos) {
            ev.hitZone = "head";
            ev.damageDealt = baseDamage * 4.0f; // Classic 4x headshot multiplier
        } else if (hit.tag.find("torso") != std::string::npos ||
                   hit.tag.find("target") != std::string::npos) {
            ev.hitZone = "torso";
            ev.damageDealt = baseDamage;
        } else {
            ev.hitZone = "world";
            ev.damageDealt = baseDamage;
        }
    }

    constexpr std::size_t kMaxRecentImpacts = 32;
    if (recentImpacts_.size() >= kMaxRecentImpacts) {
        recentImpacts_.erase(recentImpacts_.begin());
    }
    recentImpacts_.push_back(std::move(ev));
}

} // namespace classicfps::weapons
