#pragma once

#include "engine/renderer/Mesh.hpp"
#include "engine/renderer/Texture.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace classicfps::renderer {

struct DirectionalLight {
    glm::vec3 direction{-0.35f, -0.85f, -0.40f};
    glm::vec3 color{1.00f, 0.96f, 0.90f};
    float intensity{0.70f};
    glm::vec3 ambientColor{0.32f, 0.35f, 0.40f};
    float ambientIntensity{1.0f};
};

struct PointLight {
    glm::vec3 position{0.0f, 2.5f, 0.0f};
    glm::vec3 color{1.0f, 0.85f, 0.55f};
    float intensity{1.2f};
    float radius{12.0f};
};

struct FogSettings {
    bool enabled{true};
    glm::vec3 color{0.08f, 0.11f, 0.15f};
    float startDistance{10.0f};
    float endDistance{42.0f};
};

struct RenderCommand {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Texture2D> texture;
    glm::mat4 modelMatrix{1.0f};
    glm::vec3 tintColor{1.0f, 1.0f, 1.0f};
};

struct RenderStats {
    std::uint32_t submittedObjects{0};
    std::uint32_t visibleObjects{0};
    std::uint32_t culledObjects{0};
    std::uint32_t drawCalls{0};
    std::uint32_t trianglesRendered{0};
};

} // namespace classicfps::renderer
