#pragma once

#include "engine/core/ResourceManager.hpp"
#include "engine/input/InputManager.hpp"
#include "engine/physics/CollisionWorld.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/renderer/SceneTypes.hpp"

#include <cstdint>
#include <vector>

namespace classicfps::world {

class TestMap {
public:
    TestMap() = default;

    void build(core::ResourceManager& resources, physics::CollisionWorld& collisionWorld);
    void clear() noexcept;

    [[nodiscard]] input::PlayerCommand generateDemoPlayerCommand(
        double totalSimTimeSeconds,
        std::uint32_t sequence) const noexcept;

    [[nodiscard]] glm::vec3 playerSpawnFeetPosition() const noexcept {
        return glm::vec3(-4.4f, 0.0f, 3.0f);
    }
    [[nodiscard]] float playerSpawnYawDegrees() const noexcept { return -90.0f; }

    [[nodiscard]] const std::vector<renderer::RenderCommand>& renderCommands() const noexcept {
        return commands_;
    }
    [[nodiscard]] const renderer::DirectionalLight& directionalLight() const noexcept {
        return dirLight_;
    }
    [[nodiscard]] const std::vector<renderer::PointLight>& pointLights() const noexcept {
        return pointLights_;
    }
    [[nodiscard]] const renderer::FogSettings& fogSettings() const noexcept {
        return fog_;
    }
    [[nodiscard]] renderer::FogSettings& fogSettings() noexcept {
        return fog_;
    }

private:
    std::vector<renderer::RenderCommand> commands_;
    renderer::DirectionalLight dirLight_{};
    std::vector<renderer::PointLight> pointLights_;
    renderer::FogSettings fog_{};
};

} // namespace classicfps::world
