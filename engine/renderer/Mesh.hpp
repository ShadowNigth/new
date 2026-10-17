#pragma once

#include "engine/core/ResourceManager.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string_view>
#include <vector>

namespace classicfps::renderer {

struct Vertex3D {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 color{1.0f};
};

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    [[nodiscard]] AABB transform(const glm::mat4& matrix) const noexcept;
};

class Mesh final : public core::IResource {
public:
    Mesh() = default;
    Mesh(std::vector<Vertex3D> vertices, std::vector<std::uint32_t> indices);
    ~Mesh() override = default;

    [[nodiscard]] std::string_view typeName() const noexcept override { return "Mesh"; }
    [[nodiscard]] std::size_t memoryBytes() const noexcept override {
        return vertices_.size() * sizeof(Vertex3D) + indices_.size() * sizeof(std::uint32_t);
    }

    [[nodiscard]] const std::vector<Vertex3D>& vertices() const noexcept { return vertices_; }
    [[nodiscard]] const std::vector<std::uint32_t>& indices() const noexcept { return indices_; }
    [[nodiscard]] const AABB& localBounds() const noexcept { return localBounds_; }
    [[nodiscard]] bool isValid() const noexcept {
        return !vertices_.empty() && !indices_.empty() && (indices_.size() % 3U == 0U);
    }

    [[nodiscard]] static std::shared_ptr<Mesh> createBox(
        const glm::vec3& size,
        float uvScalePerMeter = 0.5f,
        const glm::vec3& vertexColor = glm::vec3(1.0f));

    // Sloped wedge/ramp ascending along -Z (front at +depth/2 is y=0, back at -depth/2 is y=height)
    [[nodiscard]] static std::shared_ptr<Mesh> createRamp(
        float width,
        float height,
        float depth,
        float uvScalePerMeter = 0.5f,
        const glm::vec3& vertexColor = glm::vec3(1.0f));

private:
    void computeBounds() noexcept;

    std::vector<Vertex3D> vertices_;
    std::vector<std::uint32_t> indices_;
    AABB localBounds_{};
};

} // namespace classicfps::renderer
