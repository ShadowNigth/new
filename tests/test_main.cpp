#include "engine/animation/Skeleton.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/core/Clock.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/Engine.hpp"
#include "engine/core/Error.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/ResourceManager.hpp"
#include "engine/core/Version.hpp"
#include "engine/debug/FrameStats.hpp"
#include "engine/debug/Profiler.hpp"
#include "engine/input/InputManager.hpp"
#include "engine/networking/NetSystem.hpp"
#include "engine/physics/CollisionWorld.hpp"
#include "engine/physics/FPSController.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/renderer/Mesh.hpp"
#include "engine/renderer/Shader.hpp"
#include "engine/renderer/SoftwareGraphicsDevice.hpp"
#include "engine/renderer/Texture.hpp"
#include "engine/resources/ModelLoader.hpp"
#include "engine/ui/HudAndConsole.hpp"
#include "engine/weapons/ViewModelRenderer.hpp"
#include "engine/weapons/WeaponDef.hpp"
#include "engine/weapons/WeaponSystem.hpp"
#include "engine/world/EntityManager.hpp"
#include "engine/world/MapFormat.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

#define EXPECT_TRUE(cond)                                                                 \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " Expected true: "   \
                      << #cond << "\n";                                                   \
            return false;                                                                 \
        }                                                                                 \
    } while (false)

#define EXPECT_NEAR(a, b, eps)                                                            \
    do {                                                                                  \
        if (std::fabs(static_cast<double>(a) - static_cast<double>(b)) > (eps)) {         \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << " Expected " << (a)  \
                      << " ~= " << (b) << "\n";                                           \
            return false;                                                                 \
        }                                                                                 \
    } while (false)

bool testVersion() {
    using namespace classicfps::core;
    constexpr VersionInfo v = Version::current();
    EXPECT_TRUE(v.major == 1);
    EXPECT_TRUE(v.minor == 0);
    EXPECT_TRUE(v.patch == 0);
    EXPECT_TRUE(v.isCompatibleWith(v));

    VersionInfo incompatible = v;
    incompatible.minor = 1;
    EXPECT_TRUE(!v.isCompatibleWith(incompatible));
    EXPECT_TRUE(!Version::toString().empty());
    EXPECT_TRUE(!Version::buildBanner().empty());
    return true;
}

bool testLogger() {
    using namespace classicfps::core;
    Logger& log = Logger::instance();
    log.setConsoleOutputEnabled(false);
    log.clearHistory();
    log.setMinLevel(LogLevel::Warn);

    log.info("Test", "This info message should be filtered out");
    log.warn("Test", "This warning should be recorded");
    log.error("Test", "This error should increment errorCount");

    const auto entries = log.recentEntries();
    EXPECT_TRUE(entries.size() == 2);
    EXPECT_TRUE(entries[0].level == LogLevel::Warn);
    EXPECT_TRUE(entries[1].level == LogLevel::Error);
    EXPECT_TRUE(log.errorCount() == 1);

    log.setMinLevel(LogLevel::Info);
    log.setConsoleOutputEnabled(true);
    log.clearHistory();
    return true;
}

bool testConfig() {
    using namespace classicfps::core;
    Config cfg;
    EXPECT_TRUE(cfg.getInt("vid_width") == 800);
    EXPECT_TRUE(cfg.getInt("vid_height") == 600);

    const std::string sampleCfg = R"(
        # Sample GoldSrc-style config
        window_title "ClassicFPS Test Window"
        vid_width 1024
        vid_height = 768
        gl_vsync 0 // disable vsync
        sys_ticrate 100.0
        r_clear_r 0.25
    )";

    EXPECT_TRUE(cfg.loadFromString(sampleCfg));
    EXPECT_TRUE(cfg.getString("window_title") == "ClassicFPS Test Window");
    EXPECT_TRUE(cfg.getInt("vid_width") == 1024);
    EXPECT_TRUE(cfg.getInt("vid_height") == 768);
    EXPECT_TRUE(cfg.getBool("gl_vsync") == false);
    EXPECT_NEAR(cfg.getFloat("sys_ticrate"), 100.0f, 1e-4f);
    EXPECT_NEAR(cfg.getFloat("r_clear_r"), 0.25f, 1e-4f);

    const std::filesystem::path tempFile =
        std::filesystem::temp_directory_path() / "classicfps_test_cfg.cfg";
    EXPECT_TRUE(cfg.saveToFile(tempFile.string()));

    Config reloaded;
    EXPECT_TRUE(reloaded.loadFromFile(tempFile.string()));
    EXPECT_TRUE(reloaded.getInt("vid_width") == 1024);
    EXPECT_TRUE(reloaded.getString("window_title") == "ClassicFPS Test Window");
    std::filesystem::remove(tempFile);
    return true;
}

bool testClock() {
    using namespace classicfps::core;
    EngineClock clock(100.0, 0.25);
    EXPECT_NEAR(clock.fixedDeltaTime(), 0.01, 1e-6);

    clock.advanceManual(0.035);
    int steps = 0;
    while (clock.shouldStepSimulation()) {
        clock.consumeSimulationStep();
        ++steps;
    }
    EXPECT_TRUE(steps == 3);
    EXPECT_TRUE(clock.tickCount() == 3);
    EXPECT_NEAR(clock.interpolationAlpha(), 0.5, 1e-4);

    clock.advanceManual(2.0);
    EXPECT_NEAR(clock.renderDeltaTime(), 0.25, 1e-6);
    return true;
}

class DummyMeshResource final : public classicfps::core::IResource {
public:
    explicit DummyMeshResource(std::size_t bytes, bool* destroyedFlag) noexcept
        : bytes_(bytes), destroyedFlag_(destroyedFlag) {}

    ~DummyMeshResource() override {
        if (destroyedFlag_ != nullptr) {
            *destroyedFlag_ = true;
        }
    }

    [[nodiscard]] std::string_view typeName() const noexcept override { return "DummyMesh"; }
    [[nodiscard]] std::size_t memoryBytes() const noexcept override { return bytes_; }

private:
    std::size_t bytes_{0};
    bool* destroyedFlag_{nullptr};
};

bool testResourceManager() {
    using namespace classicfps::core;
    bool destroyed = false;
    {
        ResourceManager mgr;
        auto res = std::make_shared<DummyMeshResource>(4096, &destroyed);
        const ResourceId id = mgr.registerResource("models/test_box", std::move(res));
        EXPECT_TRUE(id != kInvalidResourceId);
        EXPECT_TRUE(mgr.activeCount() == 1);
        EXPECT_TRUE(mgr.totalMemoryBytes() == 4096);
        EXPECT_TRUE(mgr.findById(id) != nullptr);
        EXPECT_TRUE(mgr.findByName("models/test_box") != nullptr);
        EXPECT_TRUE(!destroyed);

        EXPECT_TRUE(mgr.releaseByName("models/test_box"));
        EXPECT_TRUE(mgr.activeCount() == 0);
        EXPECT_TRUE(destroyed);
    }
    return true;
}

bool testFrameStats() {
    using namespace classicfps::debug;
    FrameStats stats;
    for (int i = 0; i < 60; ++i) {
        stats.recordFrame(0.010, 1);
    }
    EXPECT_TRUE(stats.totalFrames() == 60);
    EXPECT_TRUE(stats.totalSimSteps() == 60);
    EXPECT_NEAR(stats.averageFrameMs(), 10.0, 1e-3);
    EXPECT_NEAR(stats.currentFps(), 100.0, 1e-2);
    EXPECT_TRUE(!stats.summaryString().empty());
    return true;
}

bool testCameraAndFrustum() {
    using namespace classicfps::renderer;
    FirstPersonCamera cam(glm::vec3(0.0f, 1.7f, 0.0f), -90.0f, 0.0f, 75.0f, 4.0f / 3.0f, 0.1f, 50.0f);
    EXPECT_NEAR(cam.forward().z, -1.0f, 1e-4f);
    EXPECT_NEAR(cam.right().x, 1.0f, 1e-4f);

    cam.rotate(0.0f, 150.0f);
    EXPECT_NEAR(cam.pitchDegrees(), 89.0f, 1e-4f);
    cam.setAngles(-90.0f, 0.0f);

    const Frustum frustum = cam.extractFrustum();
    EXPECT_TRUE(frustum.intersectsAABB(glm::vec3(-1.0f, 0.0f, -6.0f), glm::vec3(1.0f, 2.0f, -4.0f)));
    EXPECT_TRUE(!frustum.intersectsAABB(glm::vec3(-1.0f, 0.0f, +4.0f), glm::vec3(1.0f, 2.0f, +6.0f)));
    EXPECT_TRUE(!frustum.intersectsAABB(glm::vec3(-1.0f, 0.0f, -85.0f), glm::vec3(1.0f, 2.0f, -75.0f)));
    return true;
}

bool testTexturesAndPPM() {
    using namespace classicfps::renderer;
    auto concrete = Texture2D::createConcreteWall(64);
    auto brick    = Texture2D::createBrickWall(64);
    auto hazard   = Texture2D::createHazardStripes(64);
    auto crate    = Texture2D::createCrateWood(64);

    EXPECT_TRUE(concrete && concrete->isValid());
    EXPECT_TRUE(brick && brick->isValid());
    EXPECT_TRUE(hazard && hazard->isValid());
    EXPECT_TRUE(crate && crate->isValid());

    const glm::vec4 s1 = brick->sampleNearest(0.25f, 0.25f);
    const glm::vec4 s2 = brick->sampleNearest(1.25f, -0.75f);
    EXPECT_NEAR(s1.r, s2.r, 1e-5f);
    EXPECT_NEAR(s1.g, s2.g, 1e-5f);
    EXPECT_NEAR(s1.b, s2.b, 1e-5f);

    const std::filesystem::path tmpPpm =
        std::filesystem::temp_directory_path() / "classicfps_tex_test.ppm";
    EXPECT_TRUE(hazard->saveToPPM(tmpPpm.string()));

    Texture2D loaded;
    EXPECT_TRUE(loaded.loadFromPPM(tmpPpm.string()));
    EXPECT_TRUE(loaded.width() == 64 && loaded.height() == 64);
    EXPECT_TRUE(loaded.pixels() == hazard->pixels());
    std::filesystem::remove(tmpPpm);
    return true;
}

bool testMeshAndBounds() {
    using namespace classicfps::renderer;
    auto box = Mesh::createBox(glm::vec3(2.0f, 4.0f, 6.0f), 0.5f);
    EXPECT_TRUE(box && box->isValid());
    EXPECT_TRUE(box->vertices().size() == 24);
    EXPECT_TRUE(box->indices().size() == 36);
    EXPECT_NEAR(box->localBounds().min.x, -1.0f, 1e-4f);
    EXPECT_NEAR(box->localBounds().max.y, +2.0f, 1e-4f);
    EXPECT_NEAR(box->localBounds().max.z, +3.0f, 1e-4f);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 5.0f, -3.0f));
    const AABB worldBox = box->localBounds().transform(model);
    EXPECT_NEAR(worldBox.min.x, 9.0f, 1e-4f);
    EXPECT_NEAR(worldBox.max.x, 11.0f, 1e-4f);
    EXPECT_NEAR(worldBox.max.y, 7.0f, 1e-4f);

    auto ramp = Mesh::createRamp(2.0f, 1.5f, 4.0f);
    EXPECT_TRUE(ramp && ramp->isValid());
    EXPECT_TRUE(ramp->indices().size() == 24);
    return true;
}

bool testShaderValidation() {
    using namespace classicfps::renderer;
    std::string err;
    EXPECT_TRUE(ShaderProgram::validateGlsl330Source(
        ShaderProgram::defaultVertexShaderSource(), err));
    EXPECT_TRUE(ShaderProgram::validateGlsl330Source(
        ShaderProgram::defaultFragmentShaderSource(), err));

    EXPECT_TRUE(!ShaderProgram::validateGlsl330Source("void main() {}", err));
    return true;
}

bool testCollisionAndRaycast() {
    using namespace classicfps::physics;
    CollisionWorld world;
    world.addBoxCollider("floor", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(20.0f, 1.0f, 20.0f));
    world.addBoxCollider("wall_north", glm::vec3(0.0f, 2.0f, -5.0f), glm::vec3(10.0f, 4.0f, 1.0f));

    // Raycast forward into north wall (front face at z = -4.5)
    const RaycastHit hit = world.raycast(glm::vec3(0.0f, 1.5f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), 20.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_TRUE(hit.tag == "wall_north");
    EXPECT_NEAR(hit.distance, 4.5f, 1e-3f);
    EXPECT_NEAR(hit.normal.z, +1.0f, 1e-3f);

    // Swept player hull towards wall (halfWidth = 0.35 -> stops near z = -4.5 + 0.35 = -4.15)
    const SweepHit sweep = world.sweepPlayerHull(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -10.0f),
        0.35f,
        1.80f);
    EXPECT_TRUE(sweep.hit);
    EXPECT_TRUE(sweep.endFeetPos.z > -4.16f && sweep.endFeetPos.z < -4.13f);
    EXPECT_NEAR(sweep.normal.z, +1.0f, 1e-3f);
    return true;
}

bool testFPSMovementAndJump() {
    using namespace classicfps::physics;
    using namespace classicfps::input;

    CollisionWorld world;
    world.addBoxCollider("floor", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f));
    world.addBoxCollider("wall", glm::vec3(0.0f, 2.0f, -3.0f), glm::vec3(10.0f, 4.0f, 1.0f));

    FPSController controller;
    // Spawn slightly above floor (y = 0.5) and let gravity land the player
    controller.spawn(glm::vec3(0.0f, 0.5f, 0.0f), -90.0f, 0.0f);

    PlayerCommand idleCmd{};
    for (int i = 0; i < 60; ++i) {
        idleCmd.sequence = static_cast<std::uint32_t>(i + 1);
        controller.simulateStep(idleCmd, world, 0.01f);
    }
    EXPECT_TRUE(controller.state().onGround);
    EXPECT_NEAR(controller.state().feetPosition.y, 0.0f, 1e-3f);

    // Jump upward and verify apex > 0.8m and eventual landing back at y = 0.0
    PlayerCommand jumpCmd = idleCmd;
    jumpCmd.jump = true;
    controller.simulateStep(jumpCmd, world, 0.01f);
    EXPECT_TRUE(!controller.state().onGround);
    EXPECT_TRUE(controller.state().velocity.y > 5.0f);

    float maxHeight = 0.0f;
    jumpCmd.jump = false;
    for (int i = 0; i < 120; ++i) {
        controller.simulateStep(jumpCmd, world, 0.01f);
        maxHeight = std::max(maxHeight, controller.state().feetPosition.y);
    }
    EXPECT_TRUE(maxHeight > 0.9f);
    EXPECT_TRUE(controller.state().onGround);
    EXPECT_NEAR(controller.state().feetPosition.y, 0.0f, 1e-3f);

    // Run forward toward the north wall (wall front face at z = -2.5, hull stops around z = -2.15)
    PlayerCommand runCmd = idleCmd;
    runCmd.forwardMove = 1.0f;
    for (int i = 0; i < 200; ++i) {
        controller.simulateStep(runCmd, world, 0.01f);
    }
    // Must not penetrate the wall at z = -2.5!
    EXPECT_TRUE(controller.state().feetPosition.z >= -2.16f);
    EXPECT_NEAR(controller.state().feetPosition.y, 0.0f, 1e-3f);
    return true;
}

bool testCrouchAndSlopes() {
    using namespace classicfps::physics;
    using namespace classicfps::input;

    CollisionWorld world;
    world.addBoxCollider("floor", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f));
    // Low overhead beam at z in [-2.0, -1.0], bottom at y = 1.25m (standing 1.80m blocked, crouch 1.00m fits)
    world.addBoxCollider("low_beam", glm::vec3(0.0f, 1.75f, -1.5f), glm::vec3(4.0f, 1.0f, 1.0f));
    // Ramp ascending from z = -4.0 (y = 0) to z = -8.0 (y = 1.5)
    world.addRampNegZCollider("ramp", glm::vec3(0.0f, 0.75f, -6.0f), 4.0f, 1.5f, 4.0f);

    FPSController controller;
    controller.spawn(glm::vec3(0.0f, 0.0f, 0.5f), -90.0f, 0.0f);

    // 1. Try walking under low_beam while standing -> must be blocked before z = -1.0
    PlayerCommand walkStand{};
    walkStand.forwardMove = 1.0f;
    walkStand.viewYawDegrees = -90.0f;
    for (int i = 0; i < 100; ++i) {
        controller.simulateStep(walkStand, world, 0.01f);
    }
    EXPECT_TRUE(controller.state().feetPosition.z > -0.70f);

    // 2. Crouch and move under the beam -> must pass underneath and refuse to uncrouch while under beam
    PlayerCommand crouchMove = walkStand;
    crouchMove.crouch = true;
    for (int i = 0; i < 40; ++i) {
        controller.simulateStep(crouchMove, world, 0.01f);
    }
    // Now underneath the beam around z ~ -1.5
    EXPECT_TRUE(controller.state().crouching);
    EXPECT_TRUE(controller.state().feetPosition.z < -1.1f && controller.state().feetPosition.z > -1.9f);

    // Release crouch button while still under beam -> controller must stay crouched!
    PlayerCommand tryStandUnderBeam = walkStand;
    tryStandUnderBeam.forwardMove = 0.0f;
    tryStandUnderBeam.crouch = false;
    controller.simulateStep(tryStandUnderBeam, world, 0.01f);
    EXPECT_TRUE(controller.state().crouching);

    // 3. Continue forward out from under the beam and onto the ramp -> should stand up and climb ramp!
    for (int i = 0; i < 100; ++i) {
        controller.simulateStep(walkStand, world, 0.01f);
    }
    EXPECT_TRUE(!controller.state().crouching);
    EXPECT_TRUE(controller.state().feetPosition.z < -5.0f && controller.state().feetPosition.z > -7.8f);
    EXPECT_TRUE(controller.state().feetPosition.y > 0.4f);
    return true;
}

bool testWeaponConfig() {
    using namespace classicfps::weapons;

    WeaponLibrary lib;
    EXPECT_TRUE(lib.count() == 3);

    // Load from game/configs/weapons.cfg if present in working tree
    if (std::filesystem::exists("game/configs/weapons.cfg")) {
        EXPECT_TRUE(lib.loadFromFile("game/configs/weapons.cfg"));
    }

    const WeaponDefinition* m4 = lib.findById("carbine_m4");
    EXPECT_TRUE(m4 != nullptr);
    EXPECT_NEAR(m4->damage, 32.0f, 1e-4f);
    EXPECT_NEAR(m4->roundsPerMinute, 650.0f, 1e-3f);
    EXPECT_NEAR(m4->fireIntervalSeconds(), 60.0f / 650.0f, 1e-4f);
    EXPECT_TRUE(m4->magazineCapacity == 30);
    EXPECT_TRUE(m4->initialReserveAmmo == 90);
    EXPECT_NEAR(m4->reloadTimeSeconds, 2.10f, 1e-4f);
    EXPECT_NEAR(m4->switchTimeSeconds, 0.65f, 1e-4f);
    EXPECT_NEAR(m4->baseSpreadDegrees, 0.45f, 1e-4f);
    EXPECT_NEAR(m4->moveSpreadMultiplier, 2.60f, 1e-4f);
    EXPECT_NEAR(m4->crouchSpreadMultiplier, 0.60f, 1e-4f);
    EXPECT_NEAR(m4->airSpreadMultiplier, 4.20f, 1e-4f);
    EXPECT_NEAR(m4->recoilPitchDegrees, 1.35f, 1e-4f);
    EXPECT_NEAR(m4->recoilYawDegrees, 0.45f, 1e-4f);
    EXPECT_NEAR(m4->recoilRecoverySpeed, 9.00f, 1e-4f);
    EXPECT_NEAR(m4->maxRangeMeters, 80.0f, 1e-4f);
    EXPECT_TRUE(m4->fireType == FireType::HitscanRaycast);
    EXPECT_TRUE(m4->fireMode == FireMode::Automatic);
    EXPECT_TRUE(!m4->soundFire.empty());
    EXPECT_TRUE(!m4->soundReload.empty());
    EXPECT_TRUE(!m4->animFire.empty());
    EXPECT_TRUE(!m4->animReload.empty());

    const WeaponDefinition* usp = lib.findById("tactical_usp");
    EXPECT_TRUE(usp != nullptr);
    EXPECT_TRUE(usp->fireType == FireType::HitscanRaycast);
    EXPECT_TRUE(usp->fireMode == FireMode::SemiAutomatic);
    EXPECT_TRUE(usp->magazineCapacity == 12);

    const WeaponDefinition* gl = lib.findById("launcher_gl");
    EXPECT_TRUE(gl != nullptr);
    EXPECT_TRUE(gl->fireType == FireType::PhysicalProjectile);
    EXPECT_TRUE(gl->fireMode == FireMode::SemiAutomatic);
    EXPECT_NEAR(gl->projectileSpeed, 28.0f, 1e-3f);

    // Test parsing custom weapon config from string
    const std::string customCfg = R"(
        [ak47_classic]
        display_name = "CV-47 Assault Rifle"
        damage = 36.0
        rpm = 600.0
        magazine_capacity = 30
        reserve_ammo = 90
        reload_time = 2.45
        switch_time = 0.50
        base_spread_deg = 0.42
        move_spread_mult = 2.65
        crouch_spread_mult = 0.55
        air_spread_mult = 4.50
        recoil_pitch_deg = 1.85
        recoil_yaw_deg = 0.65
        recoil_recovery_speed = 8.50
        max_range = 130.0
        fire_type = "hitscan"
        fire_mode = "auto"
        sound_fire = "weapons/ak47_fire.wav"
        anim_fire = "vm_ak47_fire"
    )";
    WeaponLibrary customLib;
    EXPECT_TRUE(customLib.loadFromString(customCfg));
    const WeaponDefinition* ak = customLib.findById("ak47_classic");
    EXPECT_TRUE(ak != nullptr);
    EXPECT_NEAR(ak->damage, 36.0f, 1e-4f);
    EXPECT_NEAR(ak->roundsPerMinute, 600.0f, 1e-4f);
    EXPECT_NEAR(ak->fireIntervalSeconds(), 0.10f, 1e-4f);
    return true;
}

bool testWeaponStateMachine() {
    using namespace classicfps::weapons;
    using namespace classicfps::physics;

    CollisionWorld world;
    world.addBoxCollider("floor", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f));
    world.addBoxCollider("target_torso_alpha", glm::vec3(0.0f, 1.0f, -6.0f), glm::vec3(1.0f, 1.2f, 0.6f));
    world.addBoxCollider("target_head_alpha", glm::vec3(0.0f, 1.9f, -6.0f), glm::vec3(0.6f, 0.6f, 0.6f));

    WeaponLibrary lib;
    WeaponSystem ws;
    ws.initialize(lib);

    EXPECT_TRUE(ws.activeWeapon() != nullptr);
    EXPECT_TRUE(ws.activeWeapon()->def.id == "carbine_m4");
    EXPECT_TRUE(ws.state() == WeaponState::Idle);
    EXPECT_TRUE(ws.currentMagazineAmmo() == 30);

    // 1. Fire 1 shot aimed directly at the torso target (y = 1.0, z = -6.0) while crouching
    WeaponSimulationInput in{};
    in.fireHeld = true;
    in.fireJustPressed = true;
    in.playerOnGround = true;
    in.playerCrouching = true;
    in.playerHorizontalSpeed = 0.0f;
    in.aimOrigin = glm::vec3(0.0f, 1.0f, 0.0f);
    in.aimForward = glm::vec3(0.0f, 0.0f, -1.0f);
    in.aimRight = glm::vec3(1.0f, 0.0f, 0.0f);
    in.aimUp = glm::vec3(0.0f, 1.0f, 0.0f);

    ws.update(in, world, 0.01f);
    EXPECT_TRUE(ws.state() == WeaponState::Firing);
    EXPECT_TRUE(ws.currentMagazineAmmo() == 29);
    EXPECT_TRUE(ws.totalShotsFired() == 1);
    EXPECT_TRUE(!ws.recentImpacts().empty());
    EXPECT_TRUE(ws.recentImpacts().back().hitWorld);
    EXPECT_TRUE(ws.recentImpacts().back().colliderTag == "target_torso_alpha");
    EXPECT_TRUE(ws.recentImpacts().back().hitZone == "torso");
    // Torso damage is 1x (32.0)
    EXPECT_NEAR(ws.recentImpacts().back().damageDealt, 32.0f, 1e-3f);
    EXPECT_TRUE(ws.recoilPitchDegrees() > 1.0f);

    // 2. Wait for firing cycle to return to Idle (fire interval ~0.092s)
    in.fireHeld = false;
    in.fireJustPressed = false;
    for (int i = 0; i < 15; ++i) {
        ws.update(in, world, 0.01f);
    }
    EXPECT_TRUE(ws.state() == WeaponState::Idle);

    // 3. Aim at the head target (y = 1.9, z = -6.0) and verify 4x Headshot multiplier (128.0)
    in.aimOrigin = glm::vec3(0.0f, 1.9f, 0.0f);
    in.fireHeld = true;
    in.fireJustPressed = true;
    ws.update(in, world, 0.01f);
    EXPECT_TRUE(ws.totalShotsFired() == 2);
    EXPECT_TRUE(ws.recentImpacts().back().hitWorld);
    EXPECT_TRUE(ws.recentImpacts().back().colliderTag == "target_head_alpha");
    EXPECT_TRUE(ws.recentImpacts().back().hitZone == "head");
    EXPECT_NEAR(ws.recentImpacts().back().damageDealt, 128.0f, 1e-3f);

    // Wait for firing cycle to return to Idle
    in.fireHeld = false;
    in.fireJustPressed = false;
    for (int i = 0; i < 15; ++i) {
        ws.update(in, world, 0.01f);
    }
    EXPECT_TRUE(ws.state() == WeaponState::Idle);

    // 4. Reload (state transitions to Reloading -> Idle after reloadTimeSeconds = 2.10s)
    in.reloadPressed = true;
    ws.update(in, world, 0.01f);
    EXPECT_TRUE(ws.state() == WeaponState::Reloading);

    in.reloadPressed = false;
    for (int i = 0; i < 220; ++i) {
        ws.update(in, world, 0.01f);
    }
    EXPECT_TRUE(ws.state() == WeaponState::Idle);
    EXPECT_TRUE(ws.currentMagazineAmmo() == 30);
    EXPECT_TRUE(ws.currentReserveAmmo() == 88);

    // 5. Switch to slot 2 (launcher_gl - physical projectile weapon)
    in.requestedWeaponSlot = 2;
    ws.update(in, world, 0.01f);
    EXPECT_TRUE(ws.activeSlotIndex() == 2);
    EXPECT_TRUE(ws.activeWeapon()->def.id == "launcher_gl");
    EXPECT_TRUE(ws.state() == WeaponState::Switching);

    in.requestedWeaponSlot = -1;
    for (int i = 0; i < 90; ++i) {
        ws.update(in, world, 0.01f);
    }
    EXPECT_TRUE(ws.state() == WeaponState::Idle);

    // 6. Fire projectile from launcher_gl and simulate flight until impact
    in.fireHeld = true;
    in.fireJustPressed = true;
    ws.update(in, world, 0.01f);
    EXPECT_TRUE(ws.currentMagazineAmmo() == 5);
    EXPECT_TRUE(!ws.activeProjectiles().empty());

    in.fireHeld = false;
    in.fireJustPressed = false;
    for (int i = 0; i < 60; ++i) {
        ws.update(in, world, 0.01f);
    }
    // Projectile should have impacted target at z = -6.0 and recorded an impact event
    EXPECT_TRUE(ws.recentImpacts().size() >= 3);
    EXPECT_TRUE(ws.recentImpacts().back().fromProjectile);
    return true;
}

bool testWeaponViewModel() {
    using namespace classicfps::core;
    using namespace classicfps::renderer;
    using namespace classicfps::weapons;
    using namespace classicfps::physics;

    ResourceManager resources;
    ViewModelRenderer vm;
    vm.initialize(resources);
    EXPECT_TRUE(vm.weaponPartCount() == 11);

    WeaponLibrary lib;
    WeaponSystem ws;
    ws.initialize(lib);

    CollisionWorld world;
    world.addBoxCollider("wall", glm::vec3(0.0f, 1.5f, -5.0f), glm::vec3(6.0f, 3.0f, 1.0f));

    FirstPersonCamera cam(glm::vec3(0.0f, 1.62f, 0.0f), -90.0f, 0.0f, 75.0f, 4.0f / 3.0f, 0.1f, 50.0f);

    // Fire 1 shot so we have both muzzle flash and a 3D world impact marker
    WeaponSimulationInput in{};
    in.fireHeld = true;
    in.fireJustPressed = true;
    in.playerOnGround = true;
    in.aimOrigin = cam.position();
    in.aimForward = cam.forward();
    in.aimRight = cam.right();
    in.aimUp = cam.up();
    ws.update(in, world, 0.01f);

    std::vector<RenderCommand> cmds;
    vm.appendRenderCommands(cam, ws, 0.25f, 3.5f, cmds);

    // 1 impact marker + 11 weapon/arm parts + 1 muzzle flash = 13 render commands
    EXPECT_TRUE(cmds.size() == 13);

    for (const auto& cmd : cmds) {
        EXPECT_TRUE(cmd.mesh != nullptr && cmd.mesh->isValid());
        EXPECT_TRUE(cmd.texture != nullptr && cmd.texture->isValid());
    }

    // Verify Reloading animation displaces magazine & left support arm compared to Firing
    ws.startReload();
    ws.update(WeaponSimulationInput{}, world, 1.0f); // mid-reload
    std::vector<RenderCommand> reloadCmds;
    vm.appendRenderCommands(cam, ws, 0.25f, 0.0f, reloadCmds);
    // 1 impact marker + 11 weapon/arm parts (no muzzle flash during reload) = 12 commands
    EXPECT_TRUE(reloadCmds.size() == 12);

    vm.clear();
    resources.releaseAll();
    return true;
}

bool testGltfAndAnimation() {
    using namespace classicfps::resources;
    using namespace classicfps::animation;

    auto carbine = ModelLoader::createTacticalCarbineModel();
    EXPECT_TRUE(carbine != nullptr);
    EXPECT_TRUE(carbine->subMeshes.size() == 7);
    EXPECT_TRUE(carbine->skeleton.boneCount() == 5);
    EXPECT_TRUE(carbine->skeleton.clips().size() == 4);

    // Save to glTF 2.0 (.gltf) and reload from disk
    const std::filesystem::path tmpGltf =
        std::filesystem::temp_directory_path() / "classicfps_carbine_test.gltf";
    EXPECT_TRUE(ModelLoader::saveGltf2ToFile(*carbine, tmpGltf.string()));

    auto reloaded = ModelLoader::loadGltf2FromFile(tmpGltf.string());
    std::filesystem::remove(tmpGltf);
    EXPECT_TRUE(reloaded != nullptr);
    EXPECT_TRUE(reloaded->subMeshes.size() == 7);
    EXPECT_TRUE(reloaded->skeleton.boneCount() == 5);
    EXPECT_TRUE(reloaded->skeleton.findBoneIndex("magazine") == 2);
    EXPECT_TRUE(reloaded->skeleton.findClip("reload") != nullptr);

    // Test AnimationPlayer pose evaluation and cross-fade blending
    AnimationPlayer player(&reloaded->skeleton);
    EXPECT_TRUE(player.play("idle", 0.0f));
    player.update(0.25f);
    EXPECT_TRUE(player.globalBoneMatrices().size() == 5);
    const glm::vec3 magRestPos = glm::vec3(player.globalBoneMatrices()[2][3]);

    // Switch to reload animation with cross-fade blend and advance to mid-reload (t = 0.85s)
    EXPECT_TRUE(player.play("reload", 0.10f));
    player.update(0.85f);
    const glm::vec3 magReloadPos = glm::vec3(player.globalBoneMatrices()[2][3]);
    // Magazine bone should drop downward significantly during reload!
    EXPECT_TRUE(magReloadPos.y < magRestPos.y - 0.10f);

    // Test Wavefront .obj parser
    const std::string sampleObj = R"(
        v -1.0 0.0 0.0
        v  1.0 0.0 0.0
        v  0.0 2.0 0.0
        vn 0.0 0.0 1.0
        vt 0.0 0.0
        vt 1.0 0.0
        vt 0.5 1.0
        f 1/1/1 2/2/1 3/3/1
    )";
    auto objModel = ModelLoader::loadObjFromString(sampleObj, "tri_obj");
    EXPECT_TRUE(objModel != nullptr);
    EXPECT_TRUE(objModel->subMeshes.size() == 1);
    EXPECT_TRUE(objModel->subMeshes[0].mesh->indices().size() == 3);
    return true;
}

bool testAudioSpatial() {
    using namespace classicfps::audio;
    using namespace classicfps::core;

    auto shotClip = SoundClip::createGunshotSound(22050);
    EXPECT_TRUE(shotClip != nullptr);
    EXPECT_TRUE(!shotClip->samples().empty());
    EXPECT_TRUE(shotClip->durationSeconds() > 0.15f);

    // Test WAV round-trip serialization
    const std::filesystem::path tmpWav =
        std::filesystem::temp_directory_path() / "classicfps_shot_test.wav";
    EXPECT_TRUE(shotClip->saveToWavFile(tmpWav.string()));

    SoundClip loadedWav;
    EXPECT_TRUE(loadedWav.loadFromWavFile(tmpWav.string()));
    std::filesystem::remove(tmpWav);
    EXPECT_TRUE(loadedWav.sampleRate() == 22050);
    EXPECT_TRUE(loadedWav.samples() == shotClip->samples());

    // Test 3D Spatial Audio stereo panning and distance attenuation
    ResourceManager resources;
    AudioEngine audio;
    EXPECT_TRUE(audio.initialize(resources, true));
    audio.setListenerTransform(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    // Play a 3D sound at (+6, 0, 0) -> strictly to the RIGHT of the listener
    EXPECT_TRUE(audio.playSound3D("weapons/m4_fire.wav", glm::vec3(6.0f, 0.0f, 0.0f), 1.0f, 1.5f, 30.0f));
    EXPECT_TRUE(audio.activeVoiceCount() == 1);

    std::vector<std::int16_t> stereoBuf(512 * 2, 0);
    audio.mixStereoFrames(stereoBuf.data(), 512);

    double energyLeft = 0.0;
    double energyRight = 0.0;
    for (std::size_t f = 0; f < 512; ++f) {
        energyLeft  += std::fabs(static_cast<double>(stereoBuf[f * 2 + 0]));
        energyRight += std::fabs(static_cast<double>(stereoBuf[f * 2 + 1]));
    }
    EXPECT_TRUE(energyRight > 1000.0);
    EXPECT_TRUE(energyRight > energyLeft * 4.0);

    audio.shutdown();
    resources.releaseAll();
    return true;
}

bool testEntitiesAndMaps() {
    using namespace classicfps::world;
    using namespace classicfps::resources;

    // 1. Test EntityManager lifecycle, damage, destruction, and timed respawn
    EntityManager em;
    Entity dummy{};
    dummy.type = EntityType::TargetDummy;
    dummy.name = "dummy_alpha";
    dummy.health = 100.0f;
    dummy.maxHealth = 100.0f;
    dummy.destructible = true;
    dummy.targetTag = "target_";
    dummy.model = ModelLoader::createTargetDummyModel();
    const std::uint32_t id = em.spawnEntity(std::move(dummy));
    EXPECT_TRUE(id == 1);
    EXPECT_TRUE(em.activeCount() == 1);

    // Hit for 40 damage -> still alive at 60 HP, playing hit_flinch animation
    EXPECT_TRUE(em.applyDamageByColliderTag("target_torso_alpha", 40.0f));
    EXPECT_NEAR(em.findById(id)->health, 60.0f, 1e-3f);
    EXPECT_TRUE(em.findById(id)->animPlayer.currentClipName() == "hit_flinch");

    // Hit for 80 damage -> destroyed!
    EXPECT_TRUE(em.applyDamageByColliderTag("target_head_alpha", 80.0f));
    EXPECT_TRUE(!em.findById(id)->active);
    EXPECT_TRUE(em.totalTargetsDestroyed() == 1);
    EXPECT_TRUE(em.activeCount() == 0);

    // Advance 3.5 seconds -> automatically respawns at full 100 HP!
    em.update(3.5f);
    EXPECT_TRUE(em.findById(id)->active);
    EXPECT_NEAR(em.findById(id)->health, 100.0f, 1e-3f);

    // 2. Test Text (.cfmap) and Binary (.cbsp) MapFormat round-trip
    const MapData origMap = MapFormat::createDefaultArenaMapData();
    const std::string textMap = MapFormat::serializeToText(origMap);
    MapData parsedTextMap{};
    EXPECT_TRUE(MapFormat::loadTextMapFromString(textMap, parsedTextMap));
    EXPECT_TRUE(parsedTextMap.name == origMap.name);
    EXPECT_TRUE(parsedTextMap.brushes.size() == origMap.brushes.size());
    EXPECT_TRUE(parsedTextMap.entities.size() == origMap.entities.size());

    const std::vector<std::uint8_t> binBytes = MapFormat::serializeToBinary(origMap);
    MapData parsedBinMap{};
    EXPECT_TRUE(MapFormat::loadBinaryMapFromMemory(binBytes, parsedBinMap));
    EXPECT_TRUE(parsedBinMap.name == origMap.name);
    EXPECT_TRUE(parsedBinMap.brushes.size() == origMap.brushes.size());
    EXPECT_TRUE(parsedBinMap.pointLights.size() == origMap.pointLights.size());
    EXPECT_TRUE(parsedBinMap.entities.size() == origMap.entities.size());
    return true;
}

bool testHudAndConsole() {
    using namespace classicfps::ui;
    using namespace classicfps::core;
    using namespace classicfps::weapons;
    using namespace classicfps::world;

    Config cfg;
    WeaponLibrary lib;
    WeaponSystem ws;
    ws.initialize(lib);
    EntityManager em;

    DiagnosticConsole console;
    EXPECT_TRUE(!console.isOpen());
    console.toggle();
    EXPECT_TRUE(console.isOpen());

    const std::string r1 = console.executeCommand("set fps_max 144", cfg, ws, em);
    EXPECT_TRUE(cfg.getInt("fps_max") == 144);
    EXPECT_TRUE(!r1.empty());

    const std::string r2 = console.executeCommand("slot 1", cfg, ws, em);
    EXPECT_TRUE(ws.activeSlotIndex() == 1);
    EXPECT_TRUE(!r2.empty());

    // Render HUD + Console onto a 320x240 pixel buffer
    std::vector<std::uint32_t> fb(320 * 240, 0xFF000000U);
    HudRenderer hudRen;
    HudState hud{};
    hud.playerHealth = 100;
    hud.playerArmor = 100;
    hud.currentSpreadDegrees = 1.2f;
    hud.weaponName = "USP";
    hud.weaponState = "IDLE";
    hud.magazineAmmo = 12;
    hud.reserveAmmo = 48;
    hud.currentFps = 120.0;
    hudRen.renderToPixelBuffer(fb, 320, 240, hud, console);

    // Verify amber HUD text and green crosshair pixels were written
    int amberPixels = 0;
    int greenCrosshairPixels = 0;
    for (std::uint32_t px : fb) {
        if (px == 0xFFFFA000U) {
            ++amberPixels;
        } else if (px == 0xFF30FF40U) {
            ++greenCrosshairPixels;
        }
    }
    EXPECT_TRUE(amberPixels > 50);
    EXPECT_TRUE(greenCrosshairPixels >= 20);
    return true;
}

bool testNetworkingUdp() {
    using namespace classicfps::networking;
    using namespace classicfps::physics;
    using namespace classicfps::input;

    CollisionWorld world;
    world.addBoxCollider("floor", glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(40.0f, 1.0f, 40.0f));

    NetServer server;
    // Bind to ephemeral localhost UDP port (0)
    EXPECT_TRUE(server.start(0, &world));
    EXPECT_TRUE(server.port() > 0);

    NetClient client;
    EXPECT_TRUE(client.connectToServer("127.0.0.1", server.port(), &world, glm::vec3(0.0f, 0.0f, 0.0f)));

    // Process handshake
    server.pollAndSimulate(0.01f);
    client.pollNetwork();
    EXPECT_TRUE(client.isConnected());
    EXPECT_TRUE(client.clientId() == 1);
    EXPECT_TRUE(server.connectedClientCount() == 1);

    // Send 15 forward movement commands with client-side prediction
    for (std::uint32_t seq = 1; seq <= 15; ++seq) {
        PlayerCommand cmd{};
        cmd.sequence = seq;
        cmd.forwardMove = 1.0f;
        cmd.viewYawDegrees = -90.0f;
        client.sendAndPredictCommand(cmd, 0.01f);
        server.pollAndSimulate(0.01f);
        client.pollNetwork();
    }

    EXPECT_TRUE(client.snapshotsReceived() >= 15);
    EXPECT_TRUE(client.predictedController().state().feetPosition.z < -0.15f);
    // Because client and server run identical deterministic physics, 0 reconciliations were needed so far!
    EXPECT_TRUE(client.reconciliationCount() == 0);

    // Now inject an authoritative server position override (e.g. server knockback/teleport to z = -3.0)
    server.forceClientPosition(client.clientId(), glm::vec3(0.0f, 0.0f, -3.0f));

    PlayerCommand cmdAfter{};
    cmdAfter.sequence = 16;
    cmdAfter.forwardMove = 1.0f;
    cmdAfter.viewYawDegrees = -90.0f;
    client.sendAndPredictCommand(cmdAfter, 0.01f);
    server.pollAndSimulate(0.01f);
    client.pollNetwork();

    // Client must detect the discrepancy and reconcile to the server's authoritative state near z = -3.0!
    EXPECT_TRUE(client.reconciliationCount() >= 1);
    EXPECT_TRUE(client.predictedController().state().feetPosition.z < -2.9f);

    client.disconnect();
    server.stop();
    return true;
}

bool testEngineLifecycle() {
    using namespace classicfps::core;
    using namespace classicfps::debug;
    const std::filesystem::path shotPath =
        std::filesystem::temp_directory_path() / "classicfps_phase8_scene.ppm";

    Engine engine;
    EngineOptions opts{};
    opts.overrideWidth = 320;
    opts.overrideHeight = 240;
    opts.maxFrames = 5;
    opts.forceHeadless = true;
    opts.screenshotPath = shotPath.string();

    EXPECT_TRUE(engine.initialize(opts));
    EXPECT_TRUE(engine.isInitialized());
    EXPECT_TRUE(engine.testMap().renderCommands().size() >= 15);
    EXPECT_TRUE(engine.collisionWorld().colliderCount() >= 15);
    EXPECT_TRUE(engine.weaponLibrary().count() == 3);
    EXPECT_TRUE(engine.viewModelRenderer().weaponPartCount() == 11);
    EXPECT_TRUE(engine.entityManager().activeCount() == 4);

    const int exitCode = engine.run();
    EXPECT_TRUE(exitCode == 0);
    EXPECT_TRUE(engine.clock().frameCount() == 5);
    EXPECT_TRUE(engine.lastRenderStats().visibleObjects > 15);
    EXPECT_TRUE(engine.lastRenderStats().trianglesRendered > 50);
    EXPECT_TRUE(Profiler::instance().sectionStats("Render3D").callCount == 5);
    EXPECT_TRUE(std::filesystem::exists(shotPath));

    if (engine.graphicsDevice()->backendType() ==
        classicfps::renderer::GraphicsBackendType::SoftwareFallback) {
        const auto* sw = static_cast<const classicfps::renderer::SoftwareGraphicsDevice*>(
            engine.graphicsDevice());
        std::unordered_set<std::uint32_t> uniqueColors(
            sw->colorBuffer().begin(), sw->colorBuffer().end());
        EXPECT_TRUE(uniqueColors.size() > 50);
    }

    engine.shutdown();
    std::filesystem::remove(shotPath);
    EXPECT_TRUE(!engine.isInitialized());
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    struct TestCase {
        std::string_view name;
        std::function<bool()> fn;
    };

    const std::vector<TestCase> tests = {
        {"version", testVersion},
        {"logger", testLogger},
        {"config", testConfig},
        {"clock", testClock},
        {"resource_manager", testResourceManager},
        {"frame_stats", testFrameStats},
        {"camera_and_frustum", testCameraAndFrustum},
        {"textures_and_ppm", testTexturesAndPPM},
        {"mesh_and_bounds", testMeshAndBounds},
        {"shader_validation", testShaderValidation},
        {"collision_and_raycast", testCollisionAndRaycast},
        {"fps_movement_and_jump", testFPSMovementAndJump},
        {"crouch_and_slopes", testCrouchAndSlopes},
        {"weapon_config", testWeaponConfig},
        {"weapon_state_machine", testWeaponStateMachine},
        {"weapon_viewmodel", testWeaponViewModel},
        {"gltf_and_animation", testGltfAndAnimation},
        {"audio_spatial", testAudioSpatial},
        {"entities_and_maps", testEntitiesAndMaps},
        {"hud_and_console", testHudAndConsole},
        {"networking_udp", testNetworkingUdp},
        {"engine_lifecycle", testEngineLifecycle},
    };

    std::string_view filter;
    if (argc > 1) {
        filter = argv[1];
    }

    int passed = 0;
    int failed = 0;

    for (const auto& tc : tests) {
        if (!filter.empty() && tc.name != filter) {
            continue;
        }
        std::cout << "[RUN ] " << tc.name << "\n";
        if (tc.fn()) {
            std::cout << "[ OK ] " << tc.name << "\n";
            ++passed;
        } else {
            std::cerr << "[FAIL] " << tc.name << "\n";
            ++failed;
        }
    }

    if (passed == 0 && failed == 0) {
        std::cerr << "No test matched filter: " << filter << "\n";
        return 1;
    }

    std::cout << "\nSummary: " << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}
