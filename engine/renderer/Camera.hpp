#pragma once

#include <array>
#include <glm/glm.hpp>

namespace classicfps::renderer {

struct FrustumPlane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance{0.0f};

    [[nodiscard]] float signedDistanceTo(const glm::vec3& point) const noexcept {
        return glm::dot(normal, point) + distance;
    }
};

struct Frustum {
    // Left, Right, Bottom, Top, Near, Far
    std::array<FrustumPlane, 6> planes{};

    [[nodiscard]] bool intersectsAABB(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const noexcept;
};

class FirstPersonCamera {
public:
    FirstPersonCamera(
        const glm::vec3& position = glm::vec3(0.0f, 1.7f, 5.0f),
        float yawDegrees = -90.0f,
        float pitchDegrees = 0.0f,
        float fovDegrees = 75.0f,
        float aspectRatio = 4.0f / 3.0f,
        float nearPlane = 0.1f,
        float farPlane = 120.0f) noexcept;

    void setPosition(const glm::vec3& pos) noexcept { position_ = pos; }
    void setAngles(float yawDegrees, float pitchDegrees) noexcept;
    void setPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) noexcept;
    void setAspectRatio(float aspectRatio) noexcept;

    void rotate(float deltaYawDegrees, float deltaPitchDegrees) noexcept;
    void moveForward(float distance) noexcept;
    void moveRight(float distance) noexcept;
    void moveUp(float distance) noexcept;

    [[nodiscard]] const glm::vec3& position() const noexcept { return position_; }
    [[nodiscard]] float yawDegrees() const noexcept { return yawDegrees_; }
    [[nodiscard]] float pitchDegrees() const noexcept { return pitchDegrees_; }
    [[nodiscard]] float fovDegrees() const noexcept { return fovDegrees_; }
    [[nodiscard]] float aspectRatio() const noexcept { return aspectRatio_; }
    [[nodiscard]] float nearPlane() const noexcept { return nearPlane_; }
    [[nodiscard]] float farPlane() const noexcept { return farPlane_; }

    [[nodiscard]] const glm::vec3& forward() const noexcept { return forward_; }
    [[nodiscard]] const glm::vec3& right() const noexcept { return right_; }
    [[nodiscard]] const glm::vec3& up() const noexcept { return up_; }
    [[nodiscard]] glm::vec3 flatForward() const noexcept;

    [[nodiscard]] glm::mat4 viewMatrix() const noexcept;
    [[nodiscard]] glm::mat4 projectionMatrix() const noexcept;
    [[nodiscard]] glm::mat4 viewProjectionMatrix() const noexcept;
    [[nodiscard]] Frustum extractFrustum() const noexcept;

private:
    void updateVectors() noexcept;

    glm::vec3 position_{0.0f, 1.7f, 5.0f};
    glm::vec3 forward_{0.0f, 0.0f, -1.0f};
    glm::vec3 right_{1.0f, 0.0f, 0.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};
    glm::vec3 worldUp_{0.0f, 1.0f, 0.0f};

    float yawDegrees_{-90.0f};
    float pitchDegrees_{0.0f};
    float fovDegrees_{75.0f};
    float aspectRatio_{4.0f / 3.0f};
    float nearPlane_{0.1f};
    float farPlane_{120.0f};
};

} // namespace classicfps::renderer
