#include "engine/world/TestMap.hpp"
#include "engine/core/Logger.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>

namespace classicfps::world {

void TestMap::build(core::ResourceManager& resources, physics::CollisionWorld& collisionWorld) {
    commands_.clear();
    pointLights_.clear();
    collisionWorld.clear();

    // 1. Create and register procedural classic 64x64 textures
    auto texConcrete = renderer::Texture2D::createConcreteWall(64);
    auto texBrick    = renderer::Texture2D::createBrickWall(64);
    auto texFloor    = renderer::Texture2D::createFloorTile(64);
    auto texCeiling  = renderer::Texture2D::createCeilingPanel(64);
    auto texHazard   = renderer::Texture2D::createHazardStripes(64);
    auto texCrate    = renderer::Texture2D::createCrateWood(64);

    resources.registerResource("textures/concrete_wall", texConcrete);
    resources.registerResource("textures/brick_wall", texBrick);
    resources.registerResource("textures/floor_tile", texFloor);
    resources.registerResource("textures/ceiling_panel", texCeiling);
    resources.registerResource("textures/hazard_stripes", texHazard);
    resources.registerResource("textures/crate_wood", texCrate);

    auto addSolidBrush = [&](const char* name,
                             const glm::vec3& center,
                             const glm::vec3& size,
                             const std::shared_ptr<renderer::Texture2D>& tex,
                             float uvScale = 0.5f,
                             const glm::vec3& tint = glm::vec3(1.0f)) {
        auto mesh = renderer::Mesh::createBox(size, uvScale);
        resources.registerResource(name, mesh);

        glm::mat4 model(1.0f);
        model = glm::translate(model, center);
        commands_.push_back(renderer::RenderCommand{
            .mesh = std::move(mesh),
            .texture = tex,
            .modelMatrix = model,
            .tintColor = tint
        });

        collisionWorld.addBoxCollider(name, center, size);
    };

    // 2. Main arena floor & ceiling (20m x 20m, height 5.5m)
    addSolidBrush("map/floor_main",   {0.0f, -0.25f, 0.0f}, {20.0f, 0.5f, 20.0f}, texFloor, 0.5f);
    addSolidBrush("map/ceiling_main", {0.0f,  5.75f, 0.0f}, {20.0f, 0.5f, 20.0f}, texCeiling, 0.4f);

    // 3. Perimeter walls
    addSolidBrush("map/wall_north", { 0.0f, 2.75f, -10.25f}, {20.0f, 5.5f, 0.5f}, texConcrete, 0.4f);
    addSolidBrush("map/wall_south", { 0.0f, 2.75f, +10.25f}, {20.0f, 5.5f, 0.5f}, texBrick, 0.4f);
    addSolidBrush("map/wall_west",  {-10.25f, 2.75f, 0.0f},  {0.5f, 5.5f, 20.0f}, texBrick, 0.4f);
    addSolidBrush("map/wall_east",  {+10.25f, 2.75f, 0.0f},  {0.5f, 5.5f, 20.0f}, texConcrete, 0.4f);

    // 4. Elevated north platform / catwalk + hazard ramp
    addSolidBrush("map/platform_north", {2.0f, 0.75f, -7.25f}, {10.0f, 1.5f, 4.5f}, texConcrete, 0.5f);
    addSolidBrush("map/platform_trim",  {2.0f, 1.45f, -4.95f}, {10.0f, 0.2f, 0.2f}, texHazard, 1.0f);

    {
        constexpr float rampW = 2.8f;
        constexpr float rampH = 1.5f;
        constexpr float rampD = 4.0f;
        const glm::vec3 rampCenter(-4.4f, 0.75f, -6.5f);

        auto rampMesh = renderer::Mesh::createRamp(rampW, rampH, rampD, 0.6f);
        resources.registerResource("map/ramp_north", rampMesh);
        glm::mat4 rampModel(1.0f);
        rampModel = glm::translate(rampModel, rampCenter);
        commands_.push_back(renderer::RenderCommand{
            .mesh = std::move(rampMesh),
            .texture = texHazard,
            .modelMatrix = rampModel,
            .tintColor = glm::vec3(1.0f)
        });

        collisionWorld.addRampNegZCollider("map/ramp_north", rampCenter, rampW, rampH, rampD);
    }

    // 5. Low overhead beam / duct (bottom at y = 1.25m: standing 1.80m is blocked, crouching 1.00m passes under!)
    addSolidBrush("map/crouch_beam", {-6.8f, 1.75f, 0.0f}, {3.6f, 1.0f, 1.2f}, texHazard, 0.5f);

    // 6. Low step curb (height 0.35m <= stepHeight 0.45m) to test automatic step-up
    addSolidBrush("map/step_curb", {6.5f, 0.175f, 2.0f}, {2.0f, 0.35f, 2.0f}, texConcrete, 0.5f);

    // 7. Structural pillars
    addSolidBrush("map/pillar_west", {-4.2f, 2.75f, -0.5f}, {1.2f, 5.5f, 1.2f}, texConcrete, 0.5f);
    addSolidBrush("map/pillar_east", {+4.2f, 2.75f, -0.5f}, {1.2f, 5.5f, 1.2f}, texConcrete, 0.5f);

    // 8. Classic stacked wooden crates in central combat zone
    addSolidBrush("map/crate_center_a",   { 0.2f, 0.70f, -2.2f}, {1.4f, 1.4f, 1.4f}, texCrate, 0.714f);
    addSolidBrush("map/crate_center_b",   { 1.7f, 0.70f, -2.4f}, {1.4f, 1.4f, 1.4f}, texCrate, 0.714f);
    addSolidBrush("map/crate_center_top", { 0.9f, 2.00f, -2.3f}, {1.2f, 1.2f, 1.2f}, texCrate, 0.833f);
    addSolidBrush("map/crate_platform",   { 4.2f, 2.15f, -7.0f}, {1.3f, 1.3f, 1.3f}, texCrate, 0.769f);

    // 8b. Combat target dummy (torso + head hitbox volumes for Phase 4 weapon testing)
    addSolidBrush("target_torso_alpha", {-2.2f, 0.85f, -3.8f}, {0.65f, 1.10f, 0.40f}, texHazard, 1.0f, {0.95f, 0.55f, 0.25f});
    addSolidBrush("target_head_alpha",  {-2.2f, 1.58f, -3.8f}, {0.34f, 0.34f, 0.34f}, texHazard, 1.0f, {1.00f, 0.25f, 0.20f});

    // 9. Rear/side objects behind initial camera to test frustum culling
    addSolidBrush("map/crate_south_left",  {-6.0f, 0.75f, +7.5f}, {1.5f, 1.5f, 1.5f}, texCrate, 0.66f);
    addSolidBrush("map/crate_south_right", {+6.0f, 0.75f, +7.5f}, {1.5f, 1.5f, 1.5f}, texCrate, 0.66f);
    addSolidBrush("map/rear_divider",      { 0.0f, 1.50f, +8.2f}, {5.0f, 3.0f, 0.6f}, texBrick, 0.5f);

    // 10. Lighting & classic fog
    dirLight_ = renderer::DirectionalLight{
        .direction = glm::normalize(glm::vec3(-0.35f, -0.82f, -0.45f)),
        .color = glm::vec3(1.00f, 0.96f, 0.88f),
        .intensity = 0.65f,
        .ambientColor = glm::vec3(0.32f, 0.35f, 0.42f),
        .ambientIntensity = 1.0f
    };

    pointLights_.push_back(renderer::PointLight{
        .position = glm::vec3(0.5f, 3.6f, -1.8f),
        .color = glm::vec3(1.00f, 0.84f, 0.55f),
        .intensity = 1.25f,
        .radius = 13.0f
    });
    pointLights_.push_back(renderer::PointLight{
        .position = glm::vec3(2.0f, 4.0f, -7.0f),
        .color = glm::vec3(0.60f, 0.82f, 1.00f),
        .intensity = 1.10f,
        .radius = 12.0f
    });
    pointLights_.push_back(renderer::PointLight{
        .position = glm::vec3(0.0f, 3.5f, +6.0f),
        .color = glm::vec3(1.00f, 0.72f, 0.45f),
        .intensity = 0.95f,
        .radius = 11.0f
    });

    fog_ = renderer::FogSettings{
        .enabled = true,
        .color = glm::vec3(0.08f, 0.11f, 0.15f),
        .startDistance = 10.0f,
        .endDistance = 30.0f
    };

    std::ostringstream oss;
    oss << "Built 3D TestMap with " << commands_.size() << " geometry brushes, "
        << collisionWorld.colliderCount() << " collision volumes, and "
        << pointLights_.size() << " point lights";
    core::Logger::instance().info("TestMap", oss.str());
}

void TestMap::clear() noexcept {
    commands_.clear();
    pointLights_.clear();
}

input::PlayerCommand TestMap::generateDemoPlayerCommand(
    double totalSimTimeSeconds,
    std::uint32_t sequence) const noexcept {
    const float t = static_cast<float>(totalSimTimeSeconds);
    input::PlayerCommand cmd{};
    cmd.sequence = sequence;
    cmd.forwardMove = 1.0f;
    cmd.rightMove = 0.15f * std::sin(t * 0.8f);
    cmd.viewYawDegrees = -90.0f + std::sin(t * 0.55f) * 28.0f;
    cmd.viewPitchDegrees = -4.0f + std::cos(t * 0.45f) * 4.0f;
    cmd.jump = (std::fmod(t, 2.5f) > 1.20f && std::fmod(t, 2.5f) < 1.28f);
    cmd.crouch = (std::fmod(t, 4.0f) > 2.8f && std::fmod(t, 4.0f) < 3.6f);
    cmd.walk = false;

    const float combatCycle = std::fmod(t, 1.2f);
    cmd.fireHeld = (combatCycle < 0.35f);
    cmd.fireJustPressed = (combatCycle < 0.02f);
    cmd.reload = (combatCycle > 0.85f && combatCycle < 0.90f);
    return cmd;
}

} // namespace classicfps::world
