#pragma once

#include "engine/animation/Skeleton.hpp"
#include "engine/core/ResourceManager.hpp"
#include "engine/renderer/SceneTypes.hpp"
#include "engine/resources/ModelLoader.hpp"
#include "engine/weapons/WeaponSystem.hpp"

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace classicfps::world {

enum class EntityType : std::uint8_t {
    PlayerSpawn = 0,
    TargetDummy = 1,
    WeaponPickup = 2,
    AmmoCrate = 3,
    PointLight = 4,
    TriggerZone = 5
};

struct Entity {
    std::uint32_t id{0};
    EntityType type{EntityType::TargetDummy};
    std::string name;
    glm::vec3 position{0.0f};
    glm::vec3 halfExtents{0.4f, 0.9f, 0.4f};
    float yawDegrees{0.0f};

    float health{100.0f};
    float maxHealth{100.0f};
    float respawnTimer{0.0f};
    bool active{true};
    bool destructible{false};

    std::string targetTag;      // Matches collision tag (e.g., "target_torso_alpha")
    std::string customPayload;  // e.g. weapon id "carbine_m4" or ammo count "30"

    std::shared_ptr<resources::ArticulatedModel> model;
    animation::AnimationPlayer animPlayer;
};

class EntityManager {
public:
    EntityManager() = default;

    void initializeResources(core::ResourceManager& resources);
    void clear() noexcept;

    std::uint32_t spawnEntity(Entity entity);
    [[nodiscard]] Entity* findById(std::uint32_t id) noexcept;
    [[nodiscard]] const Entity* findById(std::uint32_t id) const noexcept;
    [[nodiscard]] Entity* findByName(std::string_view name) noexcept;
    [[nodiscard]] const Entity* findByName(std::string_view name) const noexcept;

    // Applies damage to any destructible entity matching colliderTag; returns true if entity was hit
    bool applyDamageByColliderTag(std::string_view colliderTag, float damage);

    void update(float dt) noexcept;
    void checkPlayerPickups(const glm::vec3& playerFeetPos, weapons::WeaponSystem& weaponSystem);

    // Appends 3D render commands for active articulated TargetDummies and pickups
    void appendRenderCommands(
        float elapsedSeconds,
        std::vector<renderer::RenderCommand>& outCommands) const;

    [[nodiscard]] const std::vector<Entity>& entities() const noexcept { return entities_; }
    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] std::uint32_t totalTargetsDestroyed() const noexcept {
        return totalTargetsDestroyed_;
    }

private:
    std::vector<Entity> entities_;
    std::uint32_t nextId_{1};
    std::uint32_t totalTargetsDestroyed_{0};

    std::shared_ptr<renderer::Mesh> pickupBoxMesh_;
    std::shared_ptr<renderer::Texture2D> entityTexture_;
    std::shared_ptr<renderer::Texture2D> crateTexture_;
};

[[nodiscard]] std::string_view entityTypeName(EntityType type) noexcept;

} // namespace classicfps::world
