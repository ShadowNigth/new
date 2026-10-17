#include "engine/renderer/Camera.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace classicfps::renderer {

namespace {

[[nodiscard]] FrustumPlane normalizePlane(const glm::vec4& eq) noexcept {
    const glm::vec3 n(eq.x, eq.y, eq.z);
    const float len = glm::length(n);
    if (len <= 1e-6f) {
        return FrustumPlane{};
    }
    return FrustumPlane{
        .normal = n / len,
        .distance = eq.w / len
    };
}

} // namespace

bool Frustum::intersectsAABB(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const noexcept {
    for (const FrustumPlane& plane : planes) {
        // Compute positive vertex (p-vertex) furthest along plane normal
        const glm::vec3 pVertex(
            (plane.normal.x >= 0.0f) ? aabbMax.x : aabbMin.x,
            (plane.normal.y >= 0.0f) ? aabbMax.y : aabbMin.y,
            (plane.normal.z >= 0.0f) ? aabbMax.z : aabbMin.z);

        if (plane.signedDistanceTo(pVertex) < 0.0f) {
            return false;
        }
    }
    return true;
}

FirstPersonCamera::FirstPersonCamera(
    const glm::vec3& position,
    float yawDegrees,
    float pitchDegrees,
    float fovDegrees,
    float aspectRatio,
    float nearPlane,
    float farPlane) noexcept
    : position_(position),
      yawDegrees_(yawDegrees),
      pitchDegrees_(std::clamp(pitchDegrees, -89.0f, 89.0f)),
      fovDegrees_(std::clamp(fovDegrees, 30.0f, 120.0f)),
      aspectRatio_(std::max(0.1f, aspectRatio)),
      nearPlane_(std::max(0.01f, nearPlane)),
      farPlane_(std::max(nearPlane_ + 1.0f, farPlane)) {
    updateVectors();
}

void FirstPersonCamera::setAngles(float yawDegrees, float pitchDegrees) noexcept {
    yawDegrees_ = yawDegrees;
    pitchDegrees_ = std::clamp(pitchDegrees, -89.0f, 89.0f);
    updateVectors();
}

void FirstPersonCamera::setPerspective(
    float fovDegrees,
    float aspectRatio,
    float nearPlane,
    float farPlane) noexcept {
    fovDegrees_ = std::clamp(fovDegrees, 30.0f, 120.0f);
    aspectRatio_ = std::max(0.1f, aspectRatio);
    nearPlane_ = std::max(0.01f, nearPlane);
    farPlane_ = std::max(nearPlane_ + 1.0f, farPlane);
}

void FirstPersonCamera::setAspectRatio(float aspectRatio) noexcept {
    aspectRatio_ = std::max(0.1f, aspectRatio);
}

void FirstPersonCamera::rotate(float deltaYawDegrees, float deltaPitchDegrees) noexcept {
    yawDegrees_ += deltaYawDegrees;
    pitchDegrees_ = std::clamp(pitchDegrees_ + deltaPitchDegrees, -89.0f, 89.0f);
    updateVectors();
}

void FirstPersonCamera::moveForward(float distance) noexcept {
    position_ += forward_ * distance;
}

void FirstPersonCamera::moveRight(float distance) noexcept {
    position_ += right_ * distance;
}

void FirstPersonCamera::moveUp(float distance) noexcept {
    position_ += worldUp_ * distance;
}

glm::vec3 FirstPersonCamera::flatForward() const noexcept {
    glm::vec3 flat(forward_.x, 0.0f, forward_.z);
    const float len = glm::length(flat);
    if (len <= 1e-6f) {
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }
    return flat / len;
}

glm::mat4 FirstPersonCamera::viewMatrix() const noexcept {
    return glm::lookAt(position_, position_ + forward_, up_);
}

glm::mat4 FirstPersonCamera::projectionMatrix() const noexcept {
    return glm::perspective(glm::radians(fovDegrees_), aspectRatio_, nearPlane_, farPlane_);
}

glm::mat4 FirstPersonCamera::viewProjectionMatrix() const noexcept {
    return projectionMatrix() * viewMatrix();
}

Frustum FirstPersonCamera::extractFrustum() const noexcept {
    const glm::mat4 vp = viewProjectionMatrix();
    // GLM matrices are column-major: vp[col][row]
    const glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
    const glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
    const glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
    const glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

    Frustum frustum{};
    frustum.planes[0] = normalizePlane(row3 + row0); // Left
    frustum.planes[1] = normalizePlane(row3 - row0); // Right
    frustum.planes[2] = normalizePlane(row3 + row1); // Bottom
    frustum.planes[3] = normalizePlane(row3 - row1); // Top
    frustum.planes[4] = normalizePlane(row3 + row2); // Near
    frustum.planes[5] = normalizePlane(row3 - row2); // Far
    return frustum;
}

void FirstPersonCamera::updateVectors() noexcept {
    const float yawRad = glm::radians(yawDegrees_);
    const float pitchRad = glm::radians(pitchDegrees_);

    glm::vec3 dir;
    dir.x = std::cos(yawRad) * std::cos(pitchRad);
    dir.y = std::sin(pitchRad);
    dir.z = std::sin(yawRad) * std::cos(pitchRad);

    forward_ = glm::normalize(dir);
    right_ = glm::normalize(glm::cross(forward_, worldUp_));
    up_ = glm::normalize(glm::cross(right_, forward_));
}

} // namespace classicfps::renderer
