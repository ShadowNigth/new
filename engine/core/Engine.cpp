#include "engine/core/Engine.hpp"
#include "engine/core/Error.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/Version.hpp"
#include "engine/renderer/OpenGLGraphicsDevice.hpp"
#include "engine/renderer/SoftwareGraphicsDevice.hpp"
#include "engine/resources/ModelLoader.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

namespace classicfps::core {

Engine::~Engine() {
    shutdown();
}

bool Engine::initialize(const EngineOptions& options) {
    if (initialized_) {
        shutdown();
    }

    ErrorHandler::installSignalHandlers();
    ErrorHandler::clearLastError();
    debug::Profiler::instance().reset();

    config_.loadDefaults();
    if (!options.configPath.empty()) {
        (void)config_.loadFromFile(options.configPath);
    }

    if (options.overrideWidth > 0) {
        config_.setInt("vid_width", options.overrideWidth);
    }
    if (options.overrideHeight > 0) {
        config_.setInt("vid_height", options.overrideHeight);
    }
    maxFrames_ = options.maxFrames;
    screenshotPath_ = options.screenshotPath;

    Logger& logger = Logger::instance();
    logger.setMinLevel(Logger::parseLevel(config_.getString("log_level", "info")));

    const std::string logFile = config_.getString("log_file", "");
    if (!logFile.empty()) {
        if (!logger.openLogFile(logFile)) {
            logger.warn("Engine", "Could not open log file: " + logFile);
        }
    }

    logger.info("Engine", Version::buildBanner());

    if (options.forceHeadless) {
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
        logger.info("Engine", "Headless mode requested (--headless); SDL_VIDEODRIVER set to 'dummy'");
    }

    const bool allowFallback =
        !options.requireOpenGL && config_.getBool("r_allow_software_fallback", true);

    platform::WindowConfig winCfg{
        .title = config_.getString("window_title", "ClassicFPS Engine"),
        .width = config_.getInt("vid_width", 800),
        .height = config_.getInt("vid_height", 600),
        .fullscreen = config_.getBool("vid_fullscreen", false),
        .resizable = config_.getBool("vid_resizable", true),
        .requestOpenGL = !options.forceHeadless,
        .allowSoftwareFallback = allowFallback
    };

    if (!window_.initialize(winCfg)) {
        logger.fatal("Engine", "Window initialization failed; aborting engine startup");
        shutdown();
        return false;
    }

    const bool vsync = config_.getBool("gl_vsync", true);
    bool graphicsReady = false;

    if (window_.hasOpenGLFlag()) {
        auto glDevice = std::make_unique<renderer::OpenGLGraphicsDevice>();
        if (glDevice->initialize(window_, vsync)) {
            graphicsDevice_ = std::move(glDevice);
            graphicsReady = true;
        }
    }

    if (!graphicsReady) {
        if (!allowFallback) {
            ErrorHandler::reportCritical(
                ErrorCategory::Graphics,
                2001,
                "Engine",
                "Failed to initialize OpenGL 3.3 Core context and software fallback is disabled.");
            shutdown();
            return false;
        }

        logger.warn(
            "Engine",
            "OpenGL 3.3 Core context unavailable in current environment; activating SoftwareGraphicsDevice.");
        auto swDevice = std::make_unique<renderer::SoftwareGraphicsDevice>();
        if (!swDevice->initialize(window_, vsync)) {
            ErrorHandler::reportCritical(
                ErrorCategory::Graphics,
                2002,
                "Engine",
                "Failed to initialize fallback SoftwareGraphicsDevice.");
            shutdown();
            return false;
        }
        graphicsDevice_ = std::move(swDevice);
    }

    clearColor_ = renderer::ClearColor{
        .r = std::clamp(config_.getFloat("r_clear_r", 0.08f), 0.0f, 1.0f),
        .g = std::clamp(config_.getFloat("r_clear_g", 0.11f), 0.0f, 1.0f),
        .b = std::clamp(config_.getFloat("r_clear_b", 0.15f), 0.0f, 1.0f),
        .a = std::clamp(config_.getFloat("r_clear_a", 1.00f), 0.0f, 1.0f)
    };

    testMap_.build(resourceManager_, collisionWorld_);
    testMap_.fogSettings().enabled = config_.getBool("r_fog_enable", true);
    testMap_.fogSettings().color = glm::vec3(clearColor_.r, clearColor_.g, clearColor_.b);
    testMap_.fogSettings().startDistance = config_.getFloat("r_fog_start", 10.0f);
    testMap_.fogSettings().endDistance = config_.getFloat("r_fog_end", 30.0f);

    inputManager_.loadFromConfig(config_);
    inputManager_.setViewAngles(testMap_.playerSpawnYawDegrees(), 0.0f);

    playerController_.loadFromConfig(config_);
    playerController_.spawn(testMap_.playerSpawnFeetPosition(), testMap_.playerSpawnYawDegrees(), 0.0f);

    weaponLibrary_.loadDefaults();
    const std::string weaponsCfgPath = config_.getString("weapons_config", "game/configs/weapons.cfg");
    if (!weaponsCfgPath.empty() && std::filesystem::exists(weaponsCfgPath)) {
        (void)weaponLibrary_.loadFromFile(weaponsCfgPath);
    }
    weaponSystem_.initialize(weaponLibrary_);
    viewModelRenderer_.initialize(resourceManager_);

    // Populate EntityManager from arena map definition (or external .cbsp/.cfmap if present)
    entityManager_.clear();
    entityManager_.initializeResources(resourceManager_);
    world::MapData arenaData{};
    bool loadedExternalMap = false;
    if (!options.mapPath.empty() && std::filesystem::exists(options.mapPath)) {
        if (options.mapPath.ends_with(".cbsp")) {
            loadedExternalMap = world::MapFormat::loadBinaryMap(options.mapPath, arenaData);
        } else {
            loadedExternalMap = world::MapFormat::loadTextMap(options.mapPath, arenaData);
        }
    } else if (std::filesystem::exists("assets/maps/de_arena.cbsp")) {
        loadedExternalMap = world::MapFormat::loadBinaryMap("assets/maps/de_arena.cbsp", arenaData);
    }
    if (!loadedExternalMap) {
        arenaData = world::MapFormat::createDefaultArenaMapData();
    }

    auto dummyModel = std::filesystem::exists("assets/models/target_dummy.gltf")
        ? resources::ModelLoader::loadGltf2FromFile("assets/models/target_dummy.gltf")
        : nullptr;
    if (!dummyModel) {
        dummyModel = resources::ModelLoader::createTargetDummyModel();
    }
    resourceManager_.registerResource("models/target_dummy.gltf", dummyModel);
    for (const auto& ed : arenaData.entities) {
        world::Entity ent{};
        ent.type = ed.type;
        ent.name = ed.name;
        ent.position = ed.position;
        ent.yawDegrees = ed.yawDegrees;
        ent.health = ed.maxHealth;
        ent.maxHealth = ed.maxHealth;
        ent.destructible = (ed.type == world::EntityType::TargetDummy);
        ent.targetTag = ed.targetTag;
        ent.customPayload = ed.customPayload;
        if (ed.type == world::EntityType::TargetDummy) {
            ent.model = dummyModel;
        }
        (void)entityManager_.spawnEntity(std::move(ent));
    }

    if (options.serverPort > 0) {
        (void)netServer_.start(options.serverPort, &collisionWorld_);
    }
    if (!options.connectHost.empty() && options.connectPort > 0) {
        (void)netClient_.connectToServer(
            options.connectHost,
            options.connectPort,
            &collisionWorld_,
            testMap_.playerSpawnFeetPosition());
    }

    (void)audioEngine_.initialize(resourceManager_, options.forceHeadless);

    const float aspect = static_cast<float>(window_.width()) /
                         static_cast<float>(std::max(1, window_.height()));
    const float fov = config_.getFloat("fov_degrees", 75.0f);
    camera_ = renderer::FirstPersonCamera(
        playerController_.interpolatedEyePosition(1.0),
        testMap_.playerSpawnYawDegrees(),
        0.0f,
        fov,
        aspect,
        0.1f,
        80.0f);

    demoAutoWalk_ = config_.getBool("cam_flythrough", true);
    demoSeq_ = 1;
    processedImpactCount_ = 0;

    const double tickRate = static_cast<double>(config_.getFloat("sys_ticrate", 100.0f));
    clock_.setTickRate(tickRate);
    clock_.reset();
    frameStats_.reset();
    fpsMax_ = config_.getInt("fps_max", 0);

    initialized_ = true;
    running_ = true;

    std::ostringstream oss;
    oss << "Engine initialized successfully (backend="
        << graphicsDevice_->deviceInfo().backendName
        << ", sys_ticrate=" << tickRate << " Hz, colliders="
        << collisionWorld_.colliderCount()
        << ", weapons=" << weaponLibrary_.count()
        << ", entities=" << entityManager_.activeCount() << ")";
    logger.info("Engine", oss.str());
    return true;
}

int Engine::run() {
    if (!initialized_) {
        Logger::instance().error("Engine", "Engine::run() called before initialize()");
        return 1;
    }

    Logger::instance().info("Engine", "Entering main engine loop");

    while (running_) {
        if (!stepOneFrame()) {
            break;
        }
        if (maxFrames_ > 0 && clock_.frameCount() >= maxFrames_) {
            Logger::instance().info(
                "Engine",
                "Reached configured maxFrames (" + std::to_string(maxFrames_) + "); exiting loop");
            running_ = false;
            break;
        }
    }

    if (!screenshotPath_.empty() && graphicsDevice_) {
        if (graphicsDevice_->saveScreenshotPPM(screenshotPath_)) {
            Logger::instance().info("Engine", "Saved rendered 3D frame to " + screenshotPath_);
        } else {
            Logger::instance().warn("Engine", "Failed to save screenshot to " + screenshotPath_);
        }
    }

    const auto& pState = playerController_.state();
    const auto* wSlot = weaponSystem_.activeWeapon();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << "Exiting main engine loop | " << frameStats_.summaryString()
        << " | PlayerFeet=(" << pState.feetPosition.x << ", "
        << pState.feetPosition.y << ", " << pState.feetPosition.z << ")"
        << " Grounded=" << (pState.onGround ? "true" : "false")
        << " Crouched=" << (pState.crouching ? "true" : "false");
    if (wSlot != nullptr) {
        oss << " | Weapon=" << wSlot->def.id
            << "(" << weaponSystem_.stateName()
            << " " << wSlot->magazineAmmo << "/" << wSlot->reserveAmmo
            << " Shots=" << weaponSystem_.totalShotsFired() << ")";
    }
    oss << " | LastFrame DrawCalls=" << lastRenderStats_.drawCalls
        << " Visible=" << lastRenderStats_.visibleObjects
        << " Culled=" << lastRenderStats_.culledObjects
        << " Triangles=" << lastRenderStats_.trianglesRendered
        << " | Profiler=[" << debug::Profiler::instance().summaryReport() << "]";
    Logger::instance().info("Engine", oss.str());
    return ErrorHandler::lastCriticalError().isOk() ? 0 : 2;
}

bool Engine::stepOneFrame() {
    if (!initialized_ || !running_) {
        return false;
    }

    const auto frameStart = std::chrono::steady_clock::now();

    window_.pollEvents();
    if (window_.shouldClose() || ErrorHandler::terminationRequested()) {
        running_ = false;
        return false;
    }

    if (window_.wasResizedThisFrame() && graphicsDevice_) {
        graphicsDevice_->resizeViewport(window_.width(), window_.height());
        camera_.setAspectRatio(
            static_cast<float>(window_.width()) / static_cast<float>(std::max(1, window_.height())));
    }

    if (window_.cameraInput().anyManualInput) {
        demoAutoWalk_ = false;
    }

    const double frameDelta = clock_.beginFrame();
    std::uint32_t simSteps = 0;

    while (clock_.shouldStepSimulation()) {
        fixedUpdate(clock_.fixedDeltaTime());
        clock_.consumeSimulationStep();
        ++simSteps;
    }

    render(clock_.interpolationAlpha());
    frameStats_.recordFrame(frameDelta, simSteps);

    if (fpsMax_ > 0) {
        const double targetSeconds = 1.0 / static_cast<double>(fpsMax_);
        const auto elapsed = std::chrono::steady_clock::now() - frameStart;
        const std::chrono::duration<double> remaining =
            std::chrono::duration<double>(targetSeconds) - elapsed;
        if (remaining.count() > 0.0005) {
            std::this_thread::sleep_for(remaining);
        }
    }

    return running_;
}

void Engine::requestExit() noexcept {
    running_ = false;
    window_.requestClose();
}

void Engine::shutdown() noexcept {
    running_ = false;
    if (!initialized_ && !window_.isInitialized() && !graphicsDevice_) {
        return;
    }

    Logger::instance().info("Engine", "Shutting down ClassicFPS Engine subsystems...");
    netClient_.disconnect();
    netServer_.stop();
    audioEngine_.shutdown();
    entityManager_.clear();
    frameCommands_.clear();
    viewModelRenderer_.clear();
    testMap_.clear();
    collisionWorld_.clear();
    resourceManager_.releaseAll();

    if (graphicsDevice_) {
        graphicsDevice_->shutdown();
        graphicsDevice_.reset();
    }

    window_.shutdown();
    initialized_ = false;
    Logger::instance().info("Engine", "Shutdown complete.");
    Logger::instance().closeLogFile();
}

void Engine::fixedUpdate(double fixedDt) {
    const auto dt = static_cast<float>(fixedDt);

    input::PlayerCommand cmd{};
    if (!demoAutoWalk_) {
        cmd = inputManager_.buildCommand(window_, dt);
    } else {
        cmd = testMap_.generateDemoPlayerCommand(clock_.totalSimulationTime(), demoSeq_++);
    }

    {
        debug::ScopedProfileTimer timer("Physics");
        playerController_.simulateStep(cmd, collisionWorld_, dt);
    }

    const auto& pState = playerController_.state();
    const glm::vec3 eyePos = playerController_.interpolatedEyePosition(1.0);
    camera_.setPosition(eyePos);
    camera_.setAngles(
        pState.yawDegrees + weaponSystem_.recoilYawDegrees(),
        pState.pitchDegrees + weaponSystem_.recoilPitchDegrees());

    const float horizSpeed = glm::length(glm::vec2(pState.velocity.x, pState.velocity.z));

    {
        debug::ScopedProfileTimer timer("Weapons");
        weapons::WeaponSimulationInput weaponIn{
            .fireHeld = cmd.fireHeld,
            .fireJustPressed = cmd.fireJustPressed,
            .reloadPressed = cmd.reload,
            .requestedWeaponSlot = cmd.weaponSlotSelect,
            .aimOrigin = eyePos,
            .aimForward = camera_.forward(),
            .aimRight = camera_.right(),
            .aimUp = camera_.up(),
            .playerHorizontalSpeed = horizSpeed,
            .playerOnGround = pState.onGround,
            .playerCrouching = pState.crouching
        };
        weaponSystem_.update(weaponIn, collisionWorld_, dt);
    }

    {
        debug::ScopedProfileTimer timer("EntitiesAndAudio");
        audioEngine_.setListenerTransform(eyePos, camera_.forward(), camera_.right(), camera_.up());
        for (const std::string& snd : weaponSystem_.pendingSoundEvents()) {
            (void)audioEngine_.playSound2D(snd, 0.9f);
        }
        weaponSystem_.clearPendingSoundEvents();

        const auto& impacts = weaponSystem_.recentImpacts();
        while (processedImpactCount_ < impacts.size()) {
            const auto& imp = impacts[processedImpactCount_++];
            if (imp.hitWorld) {
                (void)entityManager_.applyDamageByColliderTag(imp.colliderTag, imp.damageDealt);
                (void)audioEngine_.playSound3D("weapons/impact.wav", imp.impactPoint, 0.8f);
            }
        }
        entityManager_.checkPlayerPickups(pState.feetPosition, weaponSystem_);
        entityManager_.update(dt);
        if (netServer_.isRunning()) {
            netServer_.pollAndSimulate(dt);
        }
        if (netClient_.isConnected()) {
            netClient_.sendAndPredictCommand(cmd, dt);
            netClient_.pollNetwork();
        }
    }
}

void Engine::render(double alpha) {
    if (!graphicsDevice_) {
        return;
    }
    playerController_.syncCamera(camera_, alpha);
    const auto& pState = playerController_.state();
    camera_.setAngles(
        pState.yawDegrees + weaponSystem_.recoilYawDegrees(),
        pState.pitchDegrees + weaponSystem_.recoilPitchDegrees());

    const float horizSpeed = glm::length(glm::vec2(pState.velocity.x, pState.velocity.z));

    {
        debug::ScopedProfileTimer timer("Render3D");
        frameCommands_ = testMap_.renderCommands();
        entityManager_.appendRenderCommands(
            static_cast<float>(clock_.totalSimulationTime()),
            frameCommands_);
        viewModelRenderer_.appendRenderCommands(
            camera_,
            weaponSystem_,
            static_cast<float>(clock_.totalSimulationTime()),
            horizSpeed,
            frameCommands_);

        graphicsDevice_->beginFrame();
        graphicsDevice_->clear(clearColor_, true);
        lastRenderStats_ = graphicsDevice_->renderScene(
            camera_,
            frameCommands_,
            testMap_.directionalLight(),
            testMap_.pointLights(),
            testMap_.fogSettings());
    }

    if (graphicsDevice_->backendType() == renderer::GraphicsBackendType::SoftwareFallback) {
        debug::ScopedProfileTimer timer("HudOverlay");
        auto* sw = static_cast<renderer::SoftwareGraphicsDevice*>(graphicsDevice_.get());
        ui::HudState hud{};
        hud.playerHealth = 100;
        hud.playerArmor = 100;
        hud.currentSpreadDegrees =
            weaponSystem_.computeSpreadDegrees(horizSpeed, pState.onGround, pState.crouching);
        hud.weaponName = weaponSystem_.activeWeapon() ? weaponSystem_.activeWeapon()->def.id : "M4";
        hud.weaponState = std::string(weaponSystem_.stateName());
        hud.magazineAmmo = weaponSystem_.currentMagazineAmmo();
        hud.reserveAmmo = weaponSystem_.currentReserveAmmo();
        hud.currentFps = frameStats_.currentFps();
        hud.avgFrameMs = frameStats_.averageFrameMs();
        hud.targetsDestroyed = entityManager_.totalTargetsDestroyed();
        hudRenderer_.renderToPixelBuffer(
            sw->mutableColorBuffer(),
            window_.width(),
            window_.height(),
            hud,
            console_);
    }

    graphicsDevice_->endFrame(window_);
}

} // namespace classicfps::core
