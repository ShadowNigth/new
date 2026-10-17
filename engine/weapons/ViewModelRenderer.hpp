#pragma once

#include "engine/core/ResourceManager.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/renderer/SceneTypes.hpp"
#include "engine/weapons/WeaponSystem.hpp"

#include <memory>
#include <vector>

namespace classicfps::weapons {

class ViewModelRenderer {
public:
    ViewModelRenderer() = default;

    void initialize(core::ResourceManager& resources);
    void clear() noexcept;

    // Appends the animated first-person weapon, arms, muzzle flash, active projectiles,
    // and world impact markers to the frame's render command list.
    void appendRenderCommands(
        const renderer::FirstPersonCamera& camera,
        const WeaponSystem& weaponSystem,
        float walkCycleTimeSeconds,
        float playerHorizontalSpeed,
        std::vector<renderer::RenderCommand>& outCommands) const;

    [[nodiscard]] std::size_t weaponPartCount() const noexcept { return partMeshes_.size(); }

private:
    struct PartMesh {
        std::shared_ptr<renderer::Mesh> mesh;
        std::shared_ptr<renderer::Texture2D> texture;
        glm::vec3 localOffset{0.0f};
        glm::vec3 localRotationEulerDeg{0.0f};
        glm::vec3 tintColor{1.0f};
        bool isMagazine{false};
        bool isLeftSupportArm{false};
    };

    std::vector<PartMesh> partMeshes_;
    std::shared_ptr<renderer::Mesh> muzzleFlashMesh_;
    std::shared_ptr<renderer::Mesh> impactMarkerMesh_;
    std::shared_ptr<renderer::Mesh> projectileMesh_;
    std::shared_ptr<renderer::Texture2D> metalTexture_;
    std::shared_ptr<renderer::Texture2D> sleeveTexture_;
    std::shared_ptr<renderer::Texture2D> effectTexture_;
};

} // namespace classicfps::weapons
