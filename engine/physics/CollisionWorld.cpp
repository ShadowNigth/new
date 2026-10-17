#include "engine/physics/CollisionWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace classicfps::physics {

namespace {

constexpr float kCollisionEpsilon = 1e-4f;

// Evaluates the surface Y of a RampNegZ collider for a hull centered at (x, z) with given halfWidth
[[nodiscard]] float evaluateRampSurfaceY(
    const WorldCollider& ramp,
    float zCenter,
    float halfWidth) noexcept {
    // Because the ramp ascends toward -Z, the deepest/highest part of the ramp under the player hull
    // is at z_front = clamp(zCenter - halfWidth, ramp.min.z, ramp.max.z)
    const float zSample = std::clamp(zCenter - halfWidth, ramp.min.z, ramp.max.z);
    const float depth = std::max(ramp.max.z - ramp.min.z, 1e-5f);
    const float t = std::clamp((ramp.max.z - zSample) / depth, 0.0f, 1.0f);
    return ramp.min.y + t * (ramp.max.y - ramp.min.y);
}

[[nodiscard]] bool slabRayAABB(
    const glm::vec3& origin,
    const glm::vec3& delta,
    const glm::vec3& bMin,
    const glm::vec3& bMax,
    float& outFraction,
    glm::vec3& outNormal,
    bool& outStartSolid) noexcept {
    outStartSolid = false;

    // Check if origin is strictly inside expanded box
    if (origin.x > bMin.x + kCollisionEpsilon && origin.x < bMax.x - kCollisionEpsilon &&
        origin.y > bMin.y + kCollisionEpsilon && origin.y < bMax.y - kCollisionEpsilon &&
        origin.z > bMin.z + kCollisionEpsilon && origin.z < bMax.z - kCollisionEpsilon) {
        outStartSolid = true;
        outFraction = 0.0f;

        // Compute minimum penetration axis to push out cleanly
        const float dxMin = std::fabs(origin.x - bMin.x);
        const float dxMax = std::fabs(bMax.x - origin.x);
        const float dyMin = std::fabs(origin.y - bMin.y);
        const float dyMax = std::fabs(bMax.y - origin.y);
        const float dzMin = std::fabs(origin.z - bMin.z);
        const float dzMax = std::fabs(bMax.z - origin.z);

        float best = dxMin;
        outNormal = glm::vec3(-1.0f, 0.0f, 0.0f);
        if (dxMax < best) { best = dxMax; outNormal = glm::vec3(+1.0f, 0.0f, 0.0f); }
        if (dyMin < best) { best = dyMin; outNormal = glm::vec3(0.0f, -1.0f, 0.0f); }
        if (dyMax < best) { best = dyMax; outNormal = glm::vec3(0.0f, +1.0f, 0.0f); }
        if (dzMin < best) { best = dzMin; outNormal = glm::vec3(0.0f, 0.0f, -1.0f); }
        if (dzMax < best) { outNormal = glm::vec3(0.0f, 0.0f, +1.0f); }
        return true;
    }

    float tEnter = 0.0f;
    float tExit = 1.0f;
    glm::vec3 enterNormal(0.0f);

    for (int axis = 0; axis < 3; ++axis) {
        const float o = origin[axis];
        const float d = delta[axis];
        const float mn = bMin[axis];
        const float mx = bMax[axis];

        if (std::fabs(d) < 1e-7f) {
            if (o <= mn + kCollisionEpsilon || o >= mx - kCollisionEpsilon) {
                return false;
            }
        } else {
            const float invD = 1.0f / d;
            float t1 = (mn - o) * invD;
            float t2 = (mx - o) * invD;
            glm::vec3 n(0.0f);
            n[axis] = (d > 0.0f) ? -1.0f : +1.0f;

            if (t1 > t2) {
                std::swap(t1, t2);
            }
            if (t1 > tEnter) {
                tEnter = t1;
                enterNormal = n;
            }
            tExit = std::min(tExit, t2);
            if (tEnter > tExit) {
                return false;
            }
        }
    }

    if (tEnter < 0.0f || tEnter > 1.0f || glm::length(enterNormal) < 0.5f) {
        return false;
    }

    outFraction = tEnter;
    outNormal = enterNormal;
    return true;
}

} // namespace

void CollisionWorld::clear() noexcept {
    colliders_.clear();
    nextId_ = 1;
}

std::uint32_t CollisionWorld::addBoxCollider(
    std::string_view tag,
    const glm::vec3& center,
    const glm::vec3& size) {
    const glm::vec3 half = glm::max(size * 0.5f, glm::vec3(0.001f));
    return addAABBCollider(tag, center - half, center + half);
}

std::uint32_t CollisionWorld::addAABBCollider(
    std::string_view tag,
    const glm::vec3& minCorner,
    const glm::vec3& maxCorner) {
    const std::uint32_t id = nextId_++;
    colliders_.push_back(WorldCollider{
        .id = id,
        .kind = ColliderKind::SolidBox,
        .tag = std::string(tag),
        .min = glm::min(minCorner, maxCorner),
        .max = glm::max(minCorner, maxCorner),
        .slopeNormal = glm::vec3(0.0f, 1.0f, 0.0f)
    });
    return id;
}

std::uint32_t CollisionWorld::addRampNegZCollider(
    std::string_view tag,
    const glm::vec3& center,
    float width,
    float height,
    float depth) {
    const glm::vec3 half(
        std::max(0.01f, width * 0.5f),
        std::max(0.01f, height * 0.5f),
        std::max(0.01f, depth * 0.5f));
    const std::uint32_t id = nextId_++;
    colliders_.push_back(WorldCollider{
        .id = id,
        .kind = ColliderKind::RampNegZ,
        .tag = std::string(tag),
        .min = center - half,
        .max = center + half,
        .slopeNormal = glm::normalize(glm::vec3(0.0f, depth, height))
    });
    return id;
}

bool CollisionWorld::checkHullOverlap(
    const glm::vec3& feetPos,
    float halfWidth,
    float height) const noexcept {
    const glm::vec3 pMin(feetPos.x - halfWidth + kCollisionEpsilon,
                         feetPos.y + kCollisionEpsilon,
                         feetPos.z - halfWidth + kCollisionEpsilon);
    const glm::vec3 pMax(feetPos.x + halfWidth - kCollisionEpsilon,
                         feetPos.y + height - kCollisionEpsilon,
                         feetPos.z + halfWidth - kCollisionEpsilon);

    for (const WorldCollider& col : colliders_) {
        if (pMax.x <= col.min.x || pMin.x >= col.max.x ||
            pMax.y <= col.min.y || pMin.y >= col.max.y ||
            pMax.z <= col.min.z || pMin.z >= col.max.z) {
            continue;
        }

        if (col.kind == ColliderKind::SolidBox) {
            return true;
        }

        if (col.kind == ColliderKind::RampNegZ) {
            const float rampY = evaluateRampSurfaceY(col, feetPos.z, halfWidth);
            if (pMin.y < rampY - kCollisionEpsilon && pMax.y > col.min.y + kCollisionEpsilon) {
                return true;
            }
        }
    }
    return false;
}

SweepHit CollisionWorld::sweepPlayerHull(
    const glm::vec3& startFeetPos,
    const glm::vec3& endFeetPos,
    float halfWidth,
    float height) const noexcept {
    SweepHit best{};
    best.fraction = 1.0f;
    best.endFeetPos = endFeetPos;

    const glm::vec3 delta = endFeetPos - startFeetPos;
    if (glm::dot(delta, delta) < 1e-12f) {
        return best;
    }

    for (const WorldCollider& col : colliders_) {
        if (col.kind == ColliderKind::SolidBox) {
            // Minkowski sum of box collider and player hull (whose reference point is bottom-center feetPos)
            const glm::vec3 expMin(
                col.min.x - halfWidth,
                col.min.y - height,
                col.min.z - halfWidth);
            const glm::vec3 expMax(
                col.max.x + halfWidth,
                col.max.y,
                col.max.z + halfWidth);

            float frac = 1.0f;
            glm::vec3 norm(0.0f);
            bool startSolid = false;
            if (slabRayAABB(startFeetPos, delta, expMin, expMax, frac, norm, startSolid)) {
                if (startSolid) {
                    // Only block if moving further into the surface
                    if (glm::dot(delta, norm) < 0.0f && frac < best.fraction) {
                        best.hit = true;
                        best.startSolid = true;
                        best.fraction = 0.0f;
                        best.normal = norm;
                        best.colliderId = col.id;
                    }
                } else if (frac < best.fraction) {
                    best.hit = true;
                    best.fraction = std::max(0.0f, frac - 1e-3f);
                    best.normal = norm;
                    best.colliderId = col.id;
                }
            }
        } else if (col.kind == ColliderKind::RampNegZ) {
            // Only collide against the sloped top face if moving into the slope plane
            if (glm::dot(delta, col.slopeNormal) >= -1e-5f) {
                continue;
            }

            const glm::vec3 expMin(col.min.x - halfWidth, col.min.y - height, col.min.z - halfWidth);
            const glm::vec3 expMax(col.max.x + halfWidth, col.max.y, col.max.z + halfWidth);

            const glm::vec3 segMin = glm::min(startFeetPos, endFeetPos);
            const glm::vec3 segMax = glm::max(startFeetPos, endFeetPos);
            if (segMax.x < expMin.x || segMin.x > expMax.x ||
                segMax.y < expMin.y || segMin.y > expMax.y ||
                segMax.z < expMin.z || segMin.z > expMax.z) {
                continue;
            }

            constexpr int kSubSteps = 32;
            for (int s = 1; s <= kSubSteps; ++s) {
                const float t = static_cast<float>(s) / static_cast<float>(kSubSteps);
                if (t >= best.fraction) {
                    break;
                }
                const glm::vec3 p = startFeetPos + delta * t;
                if (p.x <= expMin.x || p.x >= expMax.x ||
                    p.z <= expMin.z || p.z >= expMax.z) {
                    continue;
                }
                const float rampTopY = evaluateRampSurfaceY(col, p.z, halfWidth);
                if (p.y < rampTopY - kCollisionEpsilon && (p.y + height) > col.min.y + kCollisionEpsilon) {
                    const float prevT = static_cast<float>(s - 1) / static_cast<float>(kSubSteps);
                    best.hit = true;
                    best.fraction = prevT;
                    best.normal = col.slopeNormal;
                    best.colliderId = col.id;
                    break;
                }
            }
        }
    }

    best.endFeetPos = startFeetPos + delta * best.fraction;
    return best;
}

bool CollisionWorld::querySupportSurface(
    const glm::vec3& feetPos,
    float halfWidth,
    float height,
    float probeDistance,
    float& outSurfaceY,
    glm::vec3& outNormal,
    std::uint32_t& outColliderId) const noexcept {
    bool found = false;
    float highestY = std::numeric_limits<float>::lowest();
    glm::vec3 bestNormal(0.0f, 1.0f, 0.0f);
    std::uint32_t bestId = 0;

    const float minFootY = feetPos.y - probeDistance;
    const float maxAcceptY = feetPos.y + 0.08f;

    for (const WorldCollider& col : colliders_) {
        if (feetPos.x + halfWidth <= col.min.x + kCollisionEpsilon ||
            feetPos.x - halfWidth >= col.max.x - kCollisionEpsilon ||
            feetPos.z + halfWidth <= col.min.z + kCollisionEpsilon ||
            feetPos.z - halfWidth >= col.max.z - kCollisionEpsilon) {
            continue;
        }

        if (col.kind == ColliderKind::SolidBox) {
            const float topY = col.max.y;
            if (topY >= minFootY && topY <= maxAcceptY && (feetPos.y + height) > col.max.y) {
                if (topY > highestY) {
                    highestY = topY;
                    bestNormal = glm::vec3(0.0f, 1.0f, 0.0f);
                    bestId = col.id;
                    found = true;
                }
            }
        } else if (col.kind == ColliderKind::RampNegZ) {
            const float topY = evaluateRampSurfaceY(col, feetPos.z, halfWidth);
            if (topY >= minFootY && topY <= maxAcceptY + 0.15f) {
                if (topY > highestY) {
                    highestY = topY;
                    bestNormal = col.slopeNormal;
                    bestId = col.id;
                    found = true;
                }
            }
        }
    }

    if (found) {
        outSurfaceY = highestY;
        outNormal = bestNormal;
        outColliderId = bestId;
    }
    return found;
}

RaycastHit CollisionWorld::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance) const noexcept {
    RaycastHit best{};
    best.distance = std::max(0.0f, maxDistance);

    const float dirLen = glm::length(direction);
    if (dirLen <= 1e-6f || maxDistance <= 0.0f) {
        return best;
    }
    const glm::vec3 unitDir = direction / dirLen;
    const glm::vec3 delta = unitDir * maxDistance;

    for (const WorldCollider& col : colliders_) {
        if (col.kind == ColliderKind::SolidBox) {
            float frac = 1.0f;
            glm::vec3 norm(0.0f);
            bool startSolid = false;
            if (slabRayAABB(origin, delta, col.min, col.max, frac, norm, startSolid)) {
                const float dist = frac * maxDistance;
                if (dist < best.distance) {
                    best.hit = true;
                    best.distance = dist;
                    best.point = origin + unitDir * dist;
                    best.normal = norm;
                    best.colliderId = col.id;
                    best.tag = col.tag;
                }
            }
        } else if (col.kind == ColliderKind::RampNegZ) {
            // Check ray against ramp AABB first, then intersect with sloped plane
            float frac = 1.0f;
            glm::vec3 norm(0.0f);
            bool startSolid = false;
            if (slabRayAABB(origin, delta, col.min, col.max, frac, norm, startSolid)) {
                constexpr int kSteps = 32;
                for (int i = 0; i <= kSteps; ++i) {
                    const float t = frac + (1.0f - frac) * (static_cast<float>(i) / static_cast<float>(kSteps));
                    const glm::vec3 p = origin + delta * t;
                    if (p.x < col.min.x || p.x > col.max.x || p.z < col.min.z || p.z > col.max.z) {
                        continue;
                    }
                    const float rampY = evaluateRampSurfaceY(col, p.z, 0.0f);
                    if (p.y <= rampY && p.y >= col.min.y) {
                        const float dist = t * maxDistance;
                        if (dist < best.distance) {
                            best.hit = true;
                            best.distance = dist;
                            best.point = p;
                            best.normal = col.slopeNormal;
                            best.colliderId = col.id;
                            best.tag = col.tag;
                        }
                        break;
                    }
                }
            }
        }
    }

    return best;
}

} // namespace classicfps::physics
