#include "engine/world/EntityManager.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace classicfps::world {

std::string_view entityTypeName(EntityType type) noexcept {
    switch (type) {
        case EntityType::PlayerSpawn:  return "player_spawn";
        case EntityType::TargetDummy:  return "target_dummy";
        case EntityType::WeaponPickup: return "weapon_pickup";
        case EntityType::AmmoCrate:    return "ammo_crate";
        case EntityType::PointLight:   return "point_light";
        case EntityType::TriggerZone:  return "trigger_zone";
    }
    return "unknown";
}

void EntityManager::initializeResources(core::ResourceManager& resources) {
    pickupBoxMesh_ = renderer::Mesh::createBox(glm::vec3(0.45f, 0.35f, 0.45f), 1.0f);
    entityTexture_ = renderer::Texture2D::createHazardStripes(32);
    crateTexture_  = renderer::Texture2D::createCrateWood(32);

    resources.registerResource("entities/pickup_box", pickupBoxMesh_);
    resources.registerResource("entities/tex_hazard", entityTexture_);
    resources.registerResource("entities/tex_crate", crateTexture_);
}

void EntityManager::clear() noexcept {
    entities_.clear();
    nextId_ = 1;
    totalTargetsDestroyed_ = 0;
    pickupBoxMesh_.reset();
    entityTexture_.reset();
    crateTexture_.reset();
}

std::uint32_t EntityManager::spawnEntity(Entity entity) {
    entity.id = nextId_++;
    if (entity.model) {
        entity.animPlayer.setSkeleton(&entity.model->skeleton);
        (void)entity.animPlayer.play("idle", 0.0f);
    }
    entities_.push_back(std::move(entity));
    return entities_.back().id;
}

Entity* EntityManager::findById(std::uint32_t id) noexcept {
    for (auto& e : entities_) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

const Entity* EntityManager::findById(std::uint32_t id) const noexcept {
    for (const auto& e : entities_) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

Entity* EntityManager::findByName(std::string_view name) noexcept {
    for (auto& e : entities_) {
        if (e.name == name) {
            return &e;
        }
    }
    return nullptr;
}

const Entity* EntityManager::findByName(std::string_view name) const noexcept {
    for (const auto& e : entities_) {
        if (e.name == name) {
            return &e;
        }
    }
    return nullptr;
}

bool EntityManager::applyDamageByColliderTag(std::string_view colliderTag, float damage) {
    if (colliderTag.empty() || damage <= 0.0f) {
        return false;
    }

    for (auto& e : entities_) {
        if (!e.active || !e.destructible) {
            continue;
        }
        if (!e.targetTag.empty() && colliderTag.find(e.targetTag) != std::string_view::npos) {
            e.health = std::max(0.0f, e.health - damage);
            if (e.model) {
                (void)e.animPlayer.play("hit_flinch", 0.04f, true);
            }
            if (e.health <= 0.0f) {
                e.active = false;
                e.respawnTimer = 3.0f;
                ++totalTargetsDestroyed_;
            }
            return true;
        }
    }
    return false;
}

void EntityManager::update(float dt) noexcept {
    for (auto& e : entities_) {
        if (!e.active && e.respawnTimer > 0.0f) {
            e.respawnTimer -= dt;
            if (e.respawnTimer <= 0.0f) {
                e.respawnTimer = 0.0f;
                e.health = e.maxHealth;
                e.active = true;
                if (e.model) {
                    (void)e.animPlayer.play("idle", 0.1f);
                }
            }
        }
        if (e.active && e.model) {
            e.animPlayer.update(dt);
            if (e.animPlayer.currentClipName() == "hit_flinch" && e.animPlayer.isFinished()) {
                (void)e.animPlayer.play("idle", 0.1f);
            }
        }
    }
}

void EntityManager::checkPlayerPickups(
    const glm::vec3& playerFeetPos,
    weapons::WeaponSystem& weaponSystem) {
    for (auto& e : entities_) {
        if (!e.active) {
            continue;
        }
        if (e.type == EntityType::AmmoCrate) {
            const float dist = glm::distance(playerFeetPos, e.position);
            if (dist < 1.35f) {
                weaponSystem.refillAllAmmo();
                e.active = false;
                e.respawnTimer = 8.0f;
            }
        }
    }
}

void EntityManager::appendRenderCommands(
    float elapsedSeconds,
    std::vector<renderer::RenderCommand>& outCommands) const {
    for (const auto& e : entities_) {
        if (!e.active) {
            continue;
        }

        if (e.type == EntityType::TargetDummy && e.model && entityTexture_) {
            glm::mat4 entityRoot = glm::translate(glm::mat4(1.0f), e.position);
            entityRoot = glm::rotate(entityRoot, glm::radians(e.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));

            const auto& boneMats = e.animPlayer.globalBoneMatrices();
            for (const auto& sm : e.model->subMeshes) {
                if (!sm.mesh) {
                    continue;
                }
                glm::mat4 boneMat(1.0f);
                if (sm.attachedBoneIndex >= 0 &&
                    static_cast<std::size_t>(sm.attachedBoneIndex) < boneMats.size()) {
                    boneMat = boneMats[static_cast<std::size_t>(sm.attachedBoneIndex)];
                }
                outCommands.push_back(renderer::RenderCommand{
                    .mesh = sm.mesh,
                    .texture = entityTexture_,
                    .modelMatrix = entityRoot * boneMat,
                    .tintColor = sm.baseColorTint
                });
            }
        } else if ((e.type == EntityType::WeaponPickup || e.type == EntityType::AmmoCrate) &&
                   pickupBoxMesh_ && crateTexture_) {
            const float bob = std::sin(elapsedSeconds * 3.0f) * 0.06f;
            glm::mat4 mat = glm::translate(glm::mat4(1.0f), e.position + glm::vec3(0.0f, bob, 0.0f));
            mat = glm::rotate(mat, elapsedSeconds * 1.4f, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 tint = (e.type == EntityType::AmmoCrate)
                ? glm::vec3(0.45f, 0.85f, 0.45f)
                : glm::vec3(0.85f, 0.75f, 0.35f);
            outCommands.push_back(renderer::RenderCommand{
                .mesh = pickupBoxMesh_,
                .texture = crateTexture_,
                .modelMatrix = mat,
                .tintColor = tint
            });
        }
    }
}

std::size_t EntityManager::activeCount() const noexcept {
    std::size_t c = 0;
    for (const auto& e : entities_) {
        if (e.active) {
            ++c;
        }
    }
    return c;
}

} // namespace classicfps::world
