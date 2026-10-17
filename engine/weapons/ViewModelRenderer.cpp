#include "engine/weapons/ViewModelRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace classicfps::weapons {

void ViewModelRenderer::initialize(core::ResourceManager& resources) {
    partMeshes_.clear();

    metalTexture_  = renderer::Texture2D::createConcreteWall(32);
    sleeveTexture_ = renderer::Texture2D::createFloorTile(32);
    effectTexture_ = renderer::Texture2D::createHazardStripes(32);

    resources.registerResource("viewmodel/tex_gunmetal", metalTexture_);
    resources.registerResource("viewmodel/tex_sleeve", sleeveTexture_);
    resources.registerResource("viewmodel/tex_effect", effectTexture_);

    auto addPart = [&](const char* name,
                       const glm::vec3& size,
                       const glm::vec3& localOffset,
                       const glm::vec3& rotDeg,
                       const glm::vec3& tint,
                       const std::shared_ptr<renderer::Texture2D>& tex,
                       bool isMag = false,
                       bool isLeftArm = false) {
        auto mesh = renderer::Mesh::createBox(size, 2.0f);
        resources.registerResource(name, mesh);
        partMeshes_.push_back(PartMesh{
            .mesh = std::move(mesh),
            .texture = tex,
            .localOffset = localOffset,
            .localRotationEulerDeg = rotDeg,
            .tintColor = tint,
            .isMagazine = isMag,
            .isLeftSupportArm = isLeftArm
        });
    };

    // --- Recognizable Low-Poly First-Person Carbine Rifle (7 distinct parts) ---
    // Camera-local coordinate system: +X = right, +Y = up, -Z = forward
    const glm::vec3 gunSteel(0.28f, 0.31f, 0.35f);
    const glm::vec3 darkPolymer(0.18f, 0.20f, 0.23f);
    const glm::vec3 brassAccent(0.75f, 0.65f, 0.42f);

    // 1. Upper & lower receiver body
    addPart("viewmodel/rifle_receiver",  {0.055f, 0.075f, 0.24f}, {0.18f, -0.15f, -0.42f}, {0.0f, 0.0f, 0.0f}, gunSteel, metalTexture_);
    // 2. Ribbed front handguard / shroud
    addPart("viewmodel/rifle_handguard", {0.048f, 0.052f, 0.20f}, {0.18f, -0.145f, -0.62f}, {0.0f, 0.0f, 0.0f}, darkPolymer, metalTexture_);
    // 3. Extended steel barrel + flash hider
    addPart("viewmodel/rifle_barrel",    {0.022f, 0.022f, 0.22f}, {0.18f, -0.140f, -0.80f}, {0.0f, 0.0f, 0.0f}, gunSteel, metalTexture_);
    // 4. Carry handle / rear & front iron sights
    addPart("viewmodel/rifle_sights",    {0.026f, 0.036f, 0.16f}, {0.18f, -0.095f, -0.43f}, {0.0f, 0.0f, 0.0f}, darkPolymer, metalTexture_);
    // 5. Rear tactical buttstock
    addPart("viewmodel/rifle_stock",     {0.045f, 0.085f, 0.18f}, {0.18f, -0.165f, -0.22f}, {0.0f, 0.0f, 0.0f}, darkPolymer, metalTexture_);
    // 6. Angled pistol grip
    addPart("viewmodel/rifle_grip",      {0.038f, 0.095f, 0.045f}, {0.18f, -0.220f, -0.37f}, {-18.0f, 0.0f, 0.0f}, darkPolymer, metalTexture_);
    // 7. Detachable magazine (animated during Reloading!)
    addPart("viewmodel/rifle_magazine",  {0.034f, 0.135f, 0.058f}, {0.18f, -0.245f, -0.47f}, {10.0f, 0.0f, 0.0f}, brassAccent, metalTexture_, true, false);

    // --- First-Person Arms & Gloved Hands (4 articulated parts) ---
    const glm::vec3 sleeveOlive(0.32f, 0.38f, 0.28f);
    const glm::vec3 skinGlove(0.24f, 0.22f, 0.20f);

    // 8. Right forearm sleeve (trigger arm)
    addPart("viewmodel/right_forearm",   {0.068f, 0.068f, 0.24f}, {0.26f, -0.26f, -0.24f}, {12.0f, -16.0f, 0.0f}, sleeveOlive, sleeveTexture_);
    // 9. Right gloved hand on pistol grip
    addPart("viewmodel/right_hand",      {0.055f, 0.058f, 0.075f}, {0.19f, -0.215f, -0.36f}, {0.0f, 0.0f, 0.0f}, skinGlove, sleeveTexture_);
    // 10. Left support forearm sleeve
    addPart("viewmodel/left_forearm",    {0.065f, 0.065f, 0.28f}, {0.02f, -0.25f, -0.44f}, {16.0f, 28.0f, -10.0f}, sleeveOlive, sleeveTexture_, false, true);
    // 11. Left gloved support hand under handguard/magazine
    addPart("viewmodel/left_hand",       {0.058f, 0.052f, 0.085f}, {0.16f, -0.185f, -0.58f}, {0.0f, 10.0f, 0.0f}, skinGlove, sleeveTexture_, false, true);

    muzzleFlashMesh_  = renderer::Mesh::createBox({0.11f, 0.11f, 0.04f}, 1.0f);
    impactMarkerMesh_ = renderer::Mesh::createBox({0.10f, 0.10f, 0.10f}, 1.0f);
    projectileMesh_   = renderer::Mesh::createBox({0.16f, 0.16f, 0.24f}, 1.0f);

    resources.registerResource("viewmodel/muzzle_flash", muzzleFlashMesh_);
    resources.registerResource("viewmodel/impact_marker", impactMarkerMesh_);
    resources.registerResource("viewmodel/projectile_mesh", projectileMesh_);
}

void ViewModelRenderer::clear() noexcept {
    partMeshes_.clear();
    muzzleFlashMesh_.reset();
    impactMarkerMesh_.reset();
    projectileMesh_.reset();
    metalTexture_.reset();
    sleeveTexture_.reset();
    effectTexture_.reset();
}

void ViewModelRenderer::appendRenderCommands(
    const renderer::FirstPersonCamera& camera,
    const WeaponSystem& weaponSystem,
    float walkCycleTimeSeconds,
    float playerHorizontalSpeed,
    std::vector<renderer::RenderCommand>& outCommands) const {
    if (partMeshes_.empty()) {
        return;
    }

    // 1. Append 3D impact markers in world space for recent weapon hits
    if (impactMarkerMesh_) {
        for (const ShotImpactEvent& imp : weaponSystem.recentImpacts()) {
            if (!imp.hitWorld) {
                continue;
            }
            glm::mat4 model(1.0f);
            model = glm::translate(model, imp.impactPoint + imp.impactNormal * 0.03f);
            const glm::vec3 color = (imp.hitZone == "head")
                ? glm::vec3(1.0f, 0.20f, 0.15f)
                : ((imp.hitZone == "torso")
                    ? glm::vec3(1.0f, 0.75f, 0.15f)
                    : glm::vec3(0.95f, 0.85f, 0.35f));

            outCommands.push_back(renderer::RenderCommand{
                .mesh = impactMarkerMesh_,
                .texture = effectTexture_,
                .modelMatrix = model,
                .tintColor = color
            });
        }
    }

    // 2. Append active physical projectiles in world space
    if (projectileMesh_) {
        for (const ActiveProjectile& proj : weaponSystem.activeProjectiles()) {
            glm::mat4 model(1.0f);
            model = glm::translate(model, proj.position);
            outCommands.push_back(renderer::RenderCommand{
                .mesh = projectileMesh_,
                .texture = effectTexture_,
                .modelMatrix = model,
                .tintColor = glm::vec3(1.0f, 0.55f, 0.15f)
            });
        }
    }

    // 3. Compute camera-aligned basis matrix for first-person viewmodel
    glm::mat4 cameraBasis(1.0f);
    cameraBasis[0] = glm::vec4(camera.right(), 0.0f);
    cameraBasis[1] = glm::vec4(camera.up(), 0.0f);
    cameraBasis[2] = glm::vec4(-camera.forward(), 0.0f);
    cameraBasis[3] = glm::vec4(camera.position(), 1.0f);

    // 4. Compute weapon state animation offsets (Idle bob, Firing kickback, Reloading cycle, Switching raise)
    const float speedRatio = std::clamp(playerHorizontalSpeed / 6.5f, 0.0f, 1.2f);
    const float bobX = std::cos(walkCycleTimeSeconds * 7.5f) * 0.012f * speedRatio +
                       std::sin(walkCycleTimeSeconds * 1.8f) * 0.003f;
    const float bobY = std::fabs(std::sin(walkCycleTimeSeconds * 7.5f)) * (-0.014f) * speedRatio +
                       std::cos(walkCycleTimeSeconds * 1.8f) * 0.003f;

    glm::vec3 animOffset(bobX, bobY, 0.0f);
    glm::vec3 animRotDeg(0.0f);
    glm::vec3 magExtraOffset(0.0f);
    glm::vec3 leftArmExtraOffset(0.0f);

    const WeaponState st = weaponSystem.state();
    const float prog = weaponSystem.stateProgress();

    if (st == WeaponState::Firing) {
        const float kick = std::sin((1.0f - prog) * 3.14159265f);
        animOffset.z += 0.045f * kick;
        animOffset.y += 0.010f * kick;
        animRotDeg.x += 6.5f * kick;
    } else if (st == WeaponState::Reloading) {
        // Tilt rifle inward and animate magazine extraction + insertion
        const float tilt = std::sin(prog * 3.14159265f);
        animOffset.y -= 0.04f * tilt;
        animRotDeg.z += 22.0f * tilt;
        animRotDeg.x += 12.0f * tilt;

        const float magDrop = std::sin(prog * 3.14159265f);
        magExtraOffset = glm::vec3(-0.03f * magDrop, -0.18f * magDrop, 0.04f * magDrop);
        leftArmExtraOffset = glm::vec3(0.02f * magDrop, -0.09f * magDrop, 0.08f * magDrop);
    } else if (st == WeaponState::Switching) {
        const float lower = 1.0f - prog;
        animOffset.y -= 0.22f * lower;
        animRotDeg.x -= 25.0f * lower;
    }

    // Apply base viewmodel transform
    glm::mat4 weaponRoot = glm::translate(cameraBasis, animOffset);
    weaponRoot = glm::rotate(weaponRoot, glm::radians(animRotDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    weaponRoot = glm::rotate(weaponRoot, glm::radians(animRotDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    weaponRoot = glm::rotate(weaponRoot, glm::radians(animRotDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));

    for (const PartMesh& part : partMeshes_) {
        glm::vec3 offset = part.localOffset;
        if (part.isMagazine) {
            offset += magExtraOffset;
        } else if (part.isLeftSupportArm) {
            offset += leftArmExtraOffset;
        }

        glm::mat4 partMat = glm::translate(weaponRoot, offset);
        partMat = glm::rotate(partMat, glm::radians(part.localRotationEulerDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
        partMat = glm::rotate(partMat, glm::radians(part.localRotationEulerDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
        partMat = glm::rotate(partMat, glm::radians(part.localRotationEulerDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));

        outCommands.push_back(renderer::RenderCommand{
            .mesh = part.mesh,
            .texture = part.texture,
            .modelMatrix = partMat,
            .tintColor = part.tintColor
        });
    }

    // 5. Append muzzle flash at the tip of the barrel when firing
    if (weaponSystem.muzzleFlashTimer() > 0.0f && muzzleFlashMesh_) {
        glm::mat4 flashMat = glm::translate(weaponRoot, glm::vec3(0.18f, -0.140f, -0.94f));
        flashMat = glm::rotate(flashMat, walkCycleTimeSeconds * 25.0f, glm::vec3(0.0f, 0.0f, 1.0f));
        outCommands.push_back(renderer::RenderCommand{
            .mesh = muzzleFlashMesh_,
            .texture = effectTexture_,
            .modelMatrix = flashMat,
            .tintColor = glm::vec3(1.0f, 0.88f, 0.35f)
        });
    }
}

} // namespace classicfps::weapons
