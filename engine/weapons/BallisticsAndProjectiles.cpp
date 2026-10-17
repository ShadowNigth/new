#include "engine/weapons/BallisticsAndProjectiles.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace classicfps::weapons {

namespace {

bool intersectRayAABB(
    const glm::vec3& origin,
    const glm::vec3& dir,
    float maxDistance,
    const renderer::AABB& box,
    float& outT,
    glm::vec3& outNormal) noexcept {
    float tMin = 0.0f;
    float tMax = maxDistance;
    glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};

    const auto axisIndex = [](int a) noexcept -> glm::length_t {
        return static_cast<glm::length_t>(a);
    };

    for (int axis = 0; axis < 3; ++axis) {
        const glm::length_t idx = axisIndex(axis);
        const float o = origin[idx];
        const float d = dir[idx];
        const float bMin = box.min[idx];
        const float bMax = box.max[idx];

        if (std::abs(d) < 1e-6f) {
            if (o < bMin || o > bMax) {
                return false;
            }
        } else {
            const float invD = 1.0f / d;
            float t1 = (bMin - o) * invD;
            float t2 = (bMax - o) * invD;
            glm::vec3 n{0.0f};
            n[idx] = (invD < 0.0f) ? 1.0f : -1.0f;

            if (t1 > t2) {
                std::swap(t1, t2);
            }
            if (t1 > tMin) {
                tMin = t1;
                hitNormal = n;
            }
            tMax = std::min(tMax, t2);
            if (tMin > tMax) {
                return false;
            }
        }
    }

    outT = tMin;
    outNormal = hitNormal;
    return true;
}

} // namespace

std::array<HitboxZone, 7> BallisticsCalculator::buildCharacterHitboxes(
    const glm::vec3& feetPosition,
    float yawDegrees,
    bool crouching) noexcept {
    const float heightScale = crouching ? 0.68f : 1.0f;
    const float yawRad = glm::radians(yawDegrees);
    const glm::vec3 right{std::cos(yawRad), 0.0f, -std::sin(yawRad)};

    auto makeBox = [&](const glm::vec3& centerOffset, const glm::vec3& halfExtents) {
        const glm::vec3 worldCenter = feetPosition +
            right * centerOffset.x +
            glm::vec3(0.0f, centerOffset.y * heightScale, 0.0f);
        const glm::vec3 scaledHalf{halfExtents.x, halfExtents.y * heightScale, halfExtents.z};
        return renderer::AABB{worldCenter - scaledHalf, worldCenter + scaledHalf};
    };

    return {{
        HitboxZone{Hitgroup::Head, makeBox({0.0f, 1.62f, 0.0f}, {0.13f, 0.14f, 0.13f})},
        HitboxZone{Hitgroup::Chest, makeBox({0.0f, 1.28f, 0.0f}, {0.20f, 0.20f, 0.16f})},
        HitboxZone{Hitgroup::Stomach, makeBox({0.0f, 0.94f, 0.0f}, {0.18f, 0.14f, 0.15f})},
        HitboxZone{Hitgroup::LeftArm, makeBox({-0.29f, 1.22f, 0.0f}, {0.09f, 0.26f, 0.10f})},
        HitboxZone{Hitgroup::RightArm, makeBox({0.29f, 1.22f, 0.0f}, {0.09f, 0.26f, 0.10f})},
        HitboxZone{Hitgroup::LeftLeg, makeBox({-0.11f, 0.40f, 0.0f}, {0.10f, 0.40f, 0.11f})},
        HitboxZone{Hitgroup::RightLeg, makeBox({0.11f, 0.40f, 0.0f}, {0.10f, 0.40f, 0.11f})}
    }};
}

HitboxRayResult BallisticsCalculator::raycastCharacterHitboxes(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDirNormalized,
    float maxDistance,
    const glm::vec3& characterFeetPos,
    float characterYawDegrees,
    bool characterCrouching) noexcept {
    HitboxRayResult best{};
    best.distance = maxDistance;

    const auto zones = buildCharacterHitboxes(
        characterFeetPos,
        characterYawDegrees,
        characterCrouching);

    for (const auto& zone : zones) {
        float t = 0.0f;
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        if (intersectRayAABB(rayOrigin, rayDirNormalized, best.distance, zone.worldBounds, t, normal)) {
            best.hit = true;
            best.hitgroup = zone.group;
            best.distance = t;
            best.point = rayOrigin + rayDirNormalized * t;
            best.normal = normal;
        }
    }
    return best;
}

float BallisticsCalculator::hitgroupDamageMultiplier(Hitgroup group) noexcept {
    switch (group) {
        case Hitgroup::Head:
            return 4.0f;
        case Hitgroup::Chest:
        case Hitgroup::LeftArm:
        case Hitgroup::RightArm:
        case Hitgroup::Generic:
            return 1.0f;
        case Hitgroup::Stomach:
            return 1.25f;
        case Hitgroup::LeftLeg:
        case Hitgroup::RightLeg:
            return 0.75f;
    }
    return 1.0f;
}

std::string_view BallisticsCalculator::hitgroupName(Hitgroup group) noexcept {
    switch (group) {
        case Hitgroup::Head:
            return "HEAD";
        case Hitgroup::Chest:
            return "CHEST";
        case Hitgroup::Stomach:
            return "STOMACH";
        case Hitgroup::LeftArm:
            return "LEFT_ARM";
        case Hitgroup::RightArm:
            return "RIGHT_ARM";
        case Hitgroup::LeftLeg:
            return "LEFT_LEG";
        case Hitgroup::RightLeg:
            return "RIGHT_LEG";
        case Hitgroup::Generic:
            return "GENERIC";
    }
    return "GENERIC";
}

ArmorDamageResult BallisticsCalculator::computeDamageWithArmor(
    float rawWeaponDamage,
    float weaponArmorRatio,
    Hitgroup hitgroup,
    int currentHealth,
    int currentArmor,
    bool hasHelmet) noexcept {
    float scaledDamage = rawWeaponDamage * hitgroupDamageMultiplier(hitgroup);

    const bool protectedByArmor = [&]() {
        if (currentArmor <= 0) {
            return false;
        }
        if (hitgroup == Hitgroup::LeftLeg || hitgroup == Hitgroup::RightLeg) {
            return false; // Kevlar never protects legs in CS 1.6
        }
        if (hitgroup == Hitgroup::Head) {
            return hasHelmet;
        }
        return true;
    }();

    int armorUsed = 0;
    if (protectedByArmor) {
        const float ratio = std::clamp(weaponArmorRatio, 0.1f, 2.0f) * 0.5f;
        float healthDmg = scaledDamage * ratio;
        constexpr float kArmorBonus = 0.5f;
        float armorDmg = (scaledDamage - healthDmg) * kArmorBonus;

        if (armorDmg > static_cast<float>(currentArmor)) {
            armorUsed = currentArmor;
            healthDmg = scaledDamage - (static_cast<float>(currentArmor) / kArmorBonus);
        } else {
            armorUsed = std::max(1, static_cast<int>(std::round(armorDmg)));
        }
        scaledDamage = healthDmg;
    }

    const int finalHealthDmg = std::max(1, static_cast<int>(std::floor(scaledDamage)));
    ArmorDamageResult res{};
    res.healthDamage = finalHealthDmg;
    res.armorConsumed = armorUsed;
    res.remainingHealth = std::max(0, currentHealth - finalHealthDmg);
    res.remainingArmor = std::max(0, currentArmor - armorUsed);
    res.fatal = (res.remainingHealth == 0);
    return res;
}

SurfaceMaterial BallisticsCalculator::classifyMaterialFromTag(std::string_view tag) noexcept {
    if (tag.find("crate") != std::string_view::npos ||
        tag.find("wood") != std::string_view::npos ||
        tag.find("door") != std::string_view::npos) {
        return SurfaceMaterial::Wood;
    }
    if (tag.find("metal") != std::string_view::npos ||
        tag.find("beam") != std::string_view::npos) {
        return SurfaceMaterial::Metal;
    }
    if (tag.find("plaster") != std::string_view::npos ||
        tag.find("partition") != std::string_view::npos) {
        return SurfaceMaterial::Plaster;
    }
    return SurfaceMaterial::Concrete;
}

PenetratingShotResult BallisticsCalculator::tracePenetratingShot(
    const physics::CollisionWorld& world,
    const glm::vec3& origin,
    const glm::vec3& directionNormalized,
    float maxRange,
    float initialDamage,
    float rangeModifier,
    int maxPenetrations) {
    PenetratingShotResult result{};

    glm::vec3 currentOrigin = origin;
    float remainingRange = maxRange;
    float currentDamage = initialDamage;
    int penetrationsRemaining = std::max(0, maxPenetrations);

    while (remainingRange > 0.05f && currentDamage >= 1.0f) {
        const auto hit = world.raycast(currentOrigin, directionNormalized, remainingRange);
        if (!hit.hit) {
            break;
        }

        // Apply range falloff over the air distance travelled
        const float distFactor = std::pow(
            std::clamp(rangeModifier, 0.5f, 1.0f),
            hit.distance / 12.7f);
        currentDamage *= distFactor;

        result.hitAnySurface = true;
        result.finalDamage = currentDamage;
        result.finalImpactPoint = hit.point;
        result.finalColliderTag = hit.tag;

        if (penetrationsRemaining <= 0) {
            break;
        }

        // Determine thickness by probing forward in small steps until exiting the solid collider
        const SurfaceMaterial mat = classifyMaterialFromTag(hit.tag);
        float maxPenetrableThickness = 0.65f;
        float damageRetention = 0.50f;
        switch (mat) {
            case SurfaceMaterial::Wood:
                maxPenetrableThickness = 1.60f;
                damageRetention = 0.72f;
                break;
            case SurfaceMaterial::Plaster:
                maxPenetrableThickness = 1.20f;
                damageRetention = 0.65f;
                break;
            case SurfaceMaterial::Concrete:
                maxPenetrableThickness = 0.75f;
                damageRetention = 0.45f;
                break;
            case SurfaceMaterial::Metal:
                maxPenetrableThickness = 0.45f;
                damageRetention = 0.35f;
                break;
            case SurfaceMaterial::Grate:
                maxPenetrableThickness = 1.80f;
                damageRetention = 0.85f;
                break;
        }

        // Cast from slightly beyond maxPenetrableThickness backwards toward hit.point to find exit face
        const glm::vec3 probeStart = hit.point + directionNormalized * (maxPenetrableThickness + 0.05f);
        const auto backHit = world.raycast(
            probeStart,
            -directionNormalized,
            maxPenetrableThickness + 0.04f);

        float thickness = 0.0f;
        glm::vec3 exitPoint = hit.point;
        bool exited = false;

        if (backHit.hit && backHit.colliderId == hit.colliderId) {
            exitPoint = backHit.point;
            thickness = glm::length(exitPoint - hit.point);
            exited = (thickness > 0.001f && thickness <= maxPenetrableThickness);
        }

        if (!exited) {
            break;
        }

        const float thicknessPenalty = 1.0f - 0.35f * (thickness / maxPenetrableThickness);
        const float damageAfter = currentDamage * damageRetention * std::clamp(thicknessPenalty, 0.25f, 1.0f);

        result.wallbangs.push_back(WallbangSegment{
            .entryPoint = hit.point,
            .exitPoint = exitPoint,
            .material = mat,
            .thicknessMeters = thickness,
            .damageBefore = currentDamage,
            .damageAfter = damageAfter
        });

        ++result.surfacesPenetrated;
        --penetrationsRemaining;
        currentDamage = damageAfter;
        const float stepAdvance = hit.distance + thickness + 0.02f;
        remainingRange -= stepAdvance;
        currentOrigin = exitPoint + directionNormalized * 0.02f;
    }

    return result;
}

std::uint32_t GrenadeSystem::throwGrenade(
    GrenadeType type,
    const glm::vec3& origin,
    const glm::vec3& throwVelocity,
    float fuseSeconds) {
    ActiveGrenade g{};
    g.id = nextId_++;
    g.type = type;
    g.position = origin;
    g.velocity = throwVelocity;
    g.fuseRemainingSeconds = std::max(0.1f, fuseSeconds);
    g.smokeDurationRemaining = (type == GrenadeType::SmokeGrenade) ? 15.0f : 0.0f;
    g.detonated = false;
    g.active = true;
    grenades_.push_back(g);
    return g.id;
}

void GrenadeSystem::update(const physics::CollisionWorld& world, float dt) {
    detonations_.clear();
    if (dt <= 0.0f) {
        return;
    }

    constexpr float kGravity = 16.0f;
    constexpr float kBounceRestitution = 0.45f;
    constexpr float kSurfaceFriction = 0.80f;

    for (auto& g : grenades_) {
        if (!g.active) {
            continue;
        }

        if (!g.detonated) {
            g.velocity.y -= kGravity * dt;
            const glm::vec3 stepDelta = g.velocity * dt;
            const float stepLen = glm::length(stepDelta);

            if (stepLen > 1e-5f) {
                const glm::vec3 dir = stepDelta / stepLen;
                const auto hit = world.raycast(g.position, dir, stepLen + 0.08f);
                if (hit.hit) {
                    g.position = hit.point + hit.normal * 0.06f;
                    const float vn = glm::dot(g.velocity, hit.normal);
                    if (vn < 0.0f) {
                        const glm::vec3 normalComp = vn * hit.normal;
                        const glm::vec3 tangentComp = g.velocity - normalComp;
                        g.velocity = tangentComp * kSurfaceFriction - normalComp * kBounceRestitution;
                        ++g.bounceCount;
                    }
                } else {
                    g.position += stepDelta;
                }
            }

            g.fuseRemainingSeconds -= dt;
            if (g.fuseRemainingSeconds <= 0.0f) {
                g.fuseRemainingSeconds = 0.0f;
                g.detonated = true;
                g.velocity = glm::vec3(0.0f);
                detonations_.push_back(GrenadeDetonationEvent{
                    .id = g.id,
                    .type = g.type,
                    .position = g.position
                });
                if (g.type != GrenadeType::SmokeGrenade) {
                    g.active = false;
                }
            }
        } else if (g.type == GrenadeType::SmokeGrenade) {
            g.smokeDurationRemaining -= dt;
            if (g.smokeDurationRemaining <= 0.0f) {
                g.smokeDurationRemaining = 0.0f;
                g.active = false;
            }
        }
    }
}

void GrenadeSystem::clear() {
    grenades_.clear();
    detonations_.clear();
}

float GrenadeSystem::computeHEBlastDamage(
    const glm::vec3& detonationPos,
    const glm::vec3& victimEyeOrChestPos,
    const physics::CollisionWorld& world) const {
    constexpr float kMaxRadius = 8.5f;
    constexpr float kMaxDamage = 98.0f;

    const glm::vec3 toVictim = victimEyeOrChestPos - detonationPos;
    const float dist = glm::length(toVictim);
    if (dist >= kMaxRadius) {
        return 0.0f;
    }

    if (dist > 0.05f) {
        const glm::vec3 dir = toVictim / dist;
        const auto occl = world.raycast(detonationPos + dir * 0.05f, dir, dist - 0.10f);
        if (occl.hit) {
            return 0.0f; // Solid cover blocks HE shrapnel/blast wave
        }
    }

    const float ratio = 1.0f - (dist / kMaxRadius);
    return kMaxDamage * ratio;
}

float GrenadeSystem::computeFlashBlindnessSeconds(
    const glm::vec3& flashPos,
    const glm::vec3& viewerEyePos,
    const glm::vec3& viewerForwardNormalized,
    const physics::CollisionWorld& world) {
    constexpr float kMaxFlashDistance = 30.0f;
    const glm::vec3 toFlash = flashPos - viewerEyePos;
    const float dist = glm::length(toFlash);
    if (dist >= kMaxFlashDistance || dist < 1e-4f) {
        return 0.0f;
    }

    const glm::vec3 dirToFlash = toFlash / dist;
    const auto occl = world.raycast(viewerEyePos, dirToFlash, dist - 0.05f);
    if (occl.hit) {
        return 0.0f; // Line of sight blocked by wall
    }

    const float viewDot = glm::dot(glm::normalize(viewerForwardNormalized), dirToFlash);
    const float distFactor = std::clamp(1.0f - (dist / kMaxFlashDistance), 0.15f, 1.0f);

    if (viewDot > 0.5f) {
        // Looking directly at flashbang -> full blindness duration
        return 4.5f * distFactor;
    }
    if (viewDot > -0.2f) {
        // Peripheral vision -> partial flash
        return 2.0f * distFactor;
    }
    // Turned away (> ~100 deg) -> minimal residual flash if close
    return (dist < 8.0f) ? (0.65f * distFactor) : 0.0f;
}

bool GrenadeSystem::isLineOccludedBySmoke(
    const glm::vec3& startPos,
    const glm::vec3& endPos) const noexcept {
    constexpr float kSmokeRadius = 3.8f;
    const glm::vec3 seg = endPos - startPos;
    const float segLenSq = glm::dot(seg, seg);
    if (segLenSq < 1e-6f) {
        return false;
    }

    for (const auto& g : grenades_) {
        if (!g.active || !g.detonated || g.type != GrenadeType::SmokeGrenade) {
            continue;
        }
        const float t = std::clamp(glm::dot(g.position - startPos, seg) / segLenSq, 0.0f, 1.0f);
        const glm::vec3 closest = startPos + seg * t;
        const glm::vec3 diff = g.position - closest;
        if (glm::dot(diff, diff) <= (kSmokeRadius * kSmokeRadius)) {
            return true;
        }
    }
    return false;
}

} // namespace classicfps::weapons
