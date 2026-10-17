#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::physics {

enum class ColliderKind : std::uint8_t {
    SolidBox = 0,
    RampNegZ = 1 // Ascends from front (+Z, min.y) to back (-Z, max.y)
};

struct WorldCollider {
    std::uint32_t id{0};
    ColliderKind kind{ColliderKind::SolidBox};
    std::string tag;
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    glm::vec3 slopeNormal{0.0f, 1.0f, 0.0f};
};

struct RaycastHit {
    bool hit{false};
    float distance{0.0f};
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    std::uint32_t colliderId{0};
    std::string tag;
};

struct SweepHit {
    bool hit{false};
    bool startSolid{false};
    float fraction{1.0f};
    glm::vec3 endFeetPos{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    std::uint32_t colliderId{0};
};

class CollisionWorld {
public:
    CollisionWorld() = default;

    void clear() noexcept;

    std::uint32_t addBoxCollider(
        std::string_view tag,
        const glm::vec3& center,
        const glm::vec3& size);

    std::uint32_t addAABBCollider(
        std::string_view tag,
        const glm::vec3& minCorner,
        const glm::vec3& maxCorner);

    // Ramp ascending along -Z (front at center.z + depth/2 has y = center.y - height/2,
    // back at center.z - depth/2 has y = center.y + height/2)
    std::uint32_t addRampNegZCollider(
        std::string_view tag,
        const glm::vec3& center,
        float width,
        float height,
        float depth);

    [[nodiscard]] std::size_t colliderCount() const noexcept { return colliders_.size(); }
    [[nodiscard]] const std::vector<WorldCollider>& colliders() const noexcept { return colliders_; }

    // Checks if a player hull with bottom-center at feetPos overlaps any world collider
    [[nodiscard]] bool checkHullOverlap(
        const glm::vec3& feetPos,
        float halfWidth,
        float height) const noexcept;

    // Sweeps a player hull from startFeetPos to endFeetPos
    [[nodiscard]] SweepHit sweepPlayerHull(
        const glm::vec3& startFeetPos,
        const glm::vec3& endFeetPos,
        float halfWidth,
        float height) const noexcept;

    // Queries the highest supporting surface height directly beneath (x, z) within feetY
    [[nodiscard]] bool querySupportSurface(
        const glm::vec3& feetPos,
        float halfWidth,
        float height,
        float probeDistance,
        float& outSurfaceY,
        glm::vec3& outNormal,
        std::uint32_t& outColliderId) const noexcept;

    // Raycast against all world colliders
    [[nodiscard]] RaycastHit raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance) const noexcept;

private:
    std::uint32_t nextId_{1};
    std::vector<WorldCollider> colliders_;
};

} // namespace classicfps::physics
