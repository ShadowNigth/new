#pragma once

#include "engine/physics/CollisionWorld.hpp"
#include "engine/renderer/Mesh.hpp"

#include <glm/vec3.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::weapons {

enum class Hitgroup : std::uint8_t {
    Generic = 0,
    Head = 1,
    Chest = 2,
    Stomach = 3,
    LeftArm = 4,
    RightArm = 5,
    LeftLeg = 6,
    RightLeg = 7
};

struct HitboxZone {
    Hitgroup group{Hitgroup::Generic};
    renderer::AABB worldBounds{};
};

struct HitboxRayResult {
    bool hit{false};
    Hitgroup hitgroup{Hitgroup::Generic};
    float distance{0.0f};
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

struct ArmorDamageResult {
    int healthDamage{0};
    int armorConsumed{0};
    int remainingHealth{0};
    int remainingArmor{0};
    bool fatal{false};
};

enum class SurfaceMaterial : std::uint8_t {
    Concrete = 0,
    Metal = 1,
    Wood = 2,
    Plaster = 3,
    Grate = 4
};

struct WallbangSegment {
    glm::vec3 entryPoint{0.0f};
    glm::vec3 exitPoint{0.0f};
    SurfaceMaterial material{SurfaceMaterial::Concrete};
    float thicknessMeters{0.0f};
    float damageBefore{0.0f};
    float damageAfter{0.0f};
};

struct PenetratingShotResult {
    bool hitAnySurface{false};
    int surfacesPenetrated{0};
    float finalDamage{0.0f};
    glm::vec3 finalImpactPoint{0.0f};
    std::string finalColliderTag;
    std::vector<WallbangSegment> wallbangs;
};

enum class GrenadeType : std::uint8_t {
    HEGrenade = 0,
    Flashbang = 1,
    SmokeGrenade = 2
};

struct ActiveGrenade {
    std::uint32_t id{0};
    GrenadeType type{GrenadeType::HEGrenade};
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float fuseRemainingSeconds{1.6f};
    float smokeDurationRemaining{0.0f};
    int bounceCount{0};
    bool detonated{false};
    bool active{true};
};

struct GrenadeDetonationEvent {
    std::uint32_t id{0};
    GrenadeType type{GrenadeType::HEGrenade};
    glm::vec3 position{0.0f};
};

class BallisticsCalculator {
public:
    [[nodiscard]] static std::array<HitboxZone, 7> buildCharacterHitboxes(
        const glm::vec3& feetPosition,
        float yawDegrees,
        bool crouching = false) noexcept;

    [[nodiscard]] static HitboxRayResult raycastCharacterHitboxes(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirNormalized,
        float maxDistance,
        const glm::vec3& characterFeetPos,
        float characterYawDegrees,
        bool characterCrouching = false) noexcept;

    [[nodiscard]] static float hitgroupDamageMultiplier(Hitgroup group) noexcept;
    [[nodiscard]] static std::string_view hitgroupName(Hitgroup group) noexcept;

    [[nodiscard]] static ArmorDamageResult computeDamageWithArmor(
        float rawWeaponDamage,
        float weaponArmorRatio,
        Hitgroup hitgroup,
        int currentHealth,
        int currentArmor,
        bool hasHelmet) noexcept;

    [[nodiscard]] static SurfaceMaterial classifyMaterialFromTag(std::string_view tag) noexcept;

    [[nodiscard]] static PenetratingShotResult tracePenetratingShot(
        const physics::CollisionWorld& world,
        const glm::vec3& origin,
        const glm::vec3& directionNormalized,
        float maxRange,
        float initialDamage,
        float rangeModifier,
        int maxPenetrations = 2);
};

class GrenadeSystem {
public:
    std::uint32_t throwGrenade(
        GrenadeType type,
        const glm::vec3& origin,
        const glm::vec3& throwVelocity,
        float fuseSeconds = 1.6f);

    void update(const physics::CollisionWorld& world, float dt);
    void clear();

    [[nodiscard]] float computeHEBlastDamage(
        const glm::vec3& detonationPos,
        const glm::vec3& victimEyeOrChestPos,
        const physics::CollisionWorld& world) const;

    [[nodiscard]] static float computeFlashBlindnessSeconds(
        const glm::vec3& flashPos,
        const glm::vec3& viewerEyePos,
        const glm::vec3& viewerForwardNormalized,
        const physics::CollisionWorld& world);

    [[nodiscard]] bool isLineOccludedBySmoke(
        const glm::vec3& startPos,
        const glm::vec3& endPos) const noexcept;

    [[nodiscard]] const std::vector<ActiveGrenade>& activeGrenades() const noexcept {
        return grenades_;
    }
    [[nodiscard]] const std::vector<GrenadeDetonationEvent>& recentDetonations() const noexcept {
        return detonations_;
    }

private:
    std::uint32_t nextId_{1};
    std::vector<ActiveGrenade> grenades_;
    std::vector<GrenadeDetonationEvent> detonations_;
};

} // namespace classicfps::weapons
