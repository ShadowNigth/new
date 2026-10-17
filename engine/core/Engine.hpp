#pragma once

#include "engine/audio/AudioEngine.hpp"
#include "engine/core/Clock.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/ResourceManager.hpp"
#include "engine/debug/FrameStats.hpp"
#include "engine/debug/Profiler.hpp"
#include "engine/input/InputManager.hpp"
#include "engine/networking/NetSystem.hpp"
#include "engine/physics/CollisionWorld.hpp"
#include "engine/physics/FPSController.hpp"
#include "engine/platform/Window.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/renderer/IGraphicsDevice.hpp"
#include "engine/ui/HudAndConsole.hpp"
#include "engine/weapons/ViewModelRenderer.hpp"
#include "engine/weapons/WeaponDef.hpp"
#include "engine/weapons/WeaponSystem.hpp"
#include "engine/world/EntityManager.hpp"
#include "engine/world/MapFormat.hpp"
#include "engine/world/TestMap.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace classicfps::core {

struct EngineOptions {
    std::string configPath;
    std::string mapPath;
    std::string screenshotPath;
    std::string connectHost;
    std::uint16_t connectPort{0};
    std::uint16_t serverPort{0};
    int overrideWidth{0};
    int overrideHeight{0};
    std::uint64_t maxFrames{0}; // 0 = run until window close / signal
    bool forceHeadless{false};
    bool requireOpenGL{false};
};

class Engine {
public:
    Engine() = default;
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    [[nodiscard]] bool initialize(const EngineOptions& options = {});
    int run();
    bool stepOneFrame();
    void requestExit() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] bool isRunning() const noexcept { return running_; }

    [[nodiscard]] Config& config() noexcept { return config_; }
    [[nodiscard]] const Config& config() const noexcept { return config_; }
    [[nodiscard]] ResourceManager& resources() noexcept { return resourceManager_; }
    [[nodiscard]] const EngineClock& clock() const noexcept { return clock_; }
    [[nodiscard]] const debug::FrameStats& frameStats() const noexcept { return frameStats_; }
    [[nodiscard]] const platform::Window& window() const noexcept { return window_; }
    [[nodiscard]] const renderer::IGraphicsDevice* graphicsDevice() const noexcept {
        return graphicsDevice_.get();
    }
    [[nodiscard]] renderer::FirstPersonCamera& camera() noexcept { return camera_; }
    [[nodiscard]] const renderer::FirstPersonCamera& camera() const noexcept { return camera_; }
    [[nodiscard]] const world::TestMap& testMap() const noexcept { return testMap_; }
    [[nodiscard]] const physics::CollisionWorld& collisionWorld() const noexcept {
        return collisionWorld_;
    }
    [[nodiscard]] physics::FPSController& playerController() noexcept { return playerController_; }
    [[nodiscard]] const physics::FPSController& playerController() const noexcept {
        return playerController_;
    }
    [[nodiscard]] weapons::WeaponSystem& weaponSystem() noexcept { return weaponSystem_; }
    [[nodiscard]] const weapons::WeaponSystem& weaponSystem() const noexcept {
        return weaponSystem_;
    }
    [[nodiscard]] const weapons::WeaponLibrary& weaponLibrary() const noexcept {
        return weaponLibrary_;
    }
    [[nodiscard]] const weapons::ViewModelRenderer& viewModelRenderer() const noexcept {
        return viewModelRenderer_;
    }
    [[nodiscard]] world::EntityManager& entityManager() noexcept { return entityManager_; }
    [[nodiscard]] const world::EntityManager& entityManager() const noexcept {
        return entityManager_;
    }
    [[nodiscard]] audio::AudioEngine& audioEngine() noexcept { return audioEngine_; }
    [[nodiscard]] const audio::AudioEngine& audioEngine() const noexcept { return audioEngine_; }
    [[nodiscard]] ui::DiagnosticConsole& console() noexcept { return console_; }
    [[nodiscard]] const ui::DiagnosticConsole& console() const noexcept { return console_; }
    [[nodiscard]] input::InputManager& inputManager() noexcept { return inputManager_; }
    [[nodiscard]] const renderer::RenderStats& lastRenderStats() const noexcept {
        return lastRenderStats_;
    }

private:
    void fixedUpdate(double fixedDt);
    void render(double alpha);

    bool initialized_{false};
    bool running_{false};
    std::uint64_t maxFrames_{0};
    std::string screenshotPath_;

    Config config_;
    ResourceManager resourceManager_;
    platform::Window window_;
    input::InputManager inputManager_;
    physics::CollisionWorld collisionWorld_;
    physics::FPSController playerController_;
    weapons::WeaponLibrary weaponLibrary_;
    weapons::WeaponSystem weaponSystem_;
    weapons::ViewModelRenderer viewModelRenderer_;
    world::EntityManager entityManager_;
    audio::AudioEngine audioEngine_;
    networking::NetServer netServer_;
    networking::NetClient netClient_;
    ui::DiagnosticConsole console_;
    ui::HudRenderer hudRenderer_;
    std::unique_ptr<renderer::IGraphicsDevice> graphicsDevice_;
    renderer::FirstPersonCamera camera_;
    world::TestMap testMap_;
    std::vector<renderer::RenderCommand> frameCommands_;
    renderer::RenderStats lastRenderStats_{};
    EngineClock clock_;
    debug::FrameStats frameStats_;
    renderer::ClearColor clearColor_{};
    int fpsMax_{0};
    bool demoAutoWalk_{true};
    std::uint32_t demoSeq_{1};
    std::size_t processedImpactCount_{0};
};

} // namespace classicfps::core
