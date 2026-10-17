#include "engine/renderer/Mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace classicfps::renderer {

AABB AABB::transform(const glm::mat4& matrix) const noexcept {
    const std::array<glm::vec3, 8> corners = {{
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {min.x, max.y, max.z},
        {max.x, max.y, max.z},
    }};

    glm::vec3 outMin(std::numeric_limits<float>::max());
    glm::vec3 outMax(std::numeric_limits<float>::lowest());

    for (const glm::vec3& c : corners) {
        const glm::vec4 worldPos = matrix * glm::vec4(c, 1.0f);
        const glm::vec3 p(worldPos.x, worldPos.y, worldPos.z);
        outMin = glm::min(outMin, p);
        outMax = glm::max(outMax, p);
    }

    return AABB{.min = outMin, .max = outMax};
}

Mesh::Mesh(std::vector<Vertex3D> vertices, std::vector<std::uint32_t> indices)
    : vertices_(std::move(vertices)), indices_(std::move(indices)) {
    computeBounds();
}

void Mesh::computeBounds() noexcept {
    if (vertices_.empty()) {
        localBounds_ = AABB{};
        return;
    }

    glm::vec3 bMin(std::numeric_limits<float>::max());
    glm::vec3 bMax(std::numeric_limits<float>::lowest());

    for (const Vertex3D& v : vertices_) {
        bMin = glm::min(bMin, v.position);
        bMax = glm::max(bMax, v.position);
    }
    localBounds_ = AABB{.min = bMin, .max = bMax};
}

std::shared_ptr<Mesh> Mesh::createBox(
    const glm::vec3& size,
    float uvScalePerMeter,
    const glm::vec3& vertexColor) {
    const glm::vec3 half = glm::max(size * 0.5f, glm::vec3(0.001f));
    const float s = std::max(0.01f, uvScalePerMeter);

    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;
    verts.reserve(24);
    idx.reserve(36);

    auto addQuad = [&](const glm::vec3& p0,
                       const glm::vec3& p1,
                       const glm::vec3& p2,
                       const glm::vec3& p3,
                       const glm::vec3& normal,
                       float uSpan,
                       float vSpan) {
        const auto base = static_cast<std::uint32_t>(verts.size());
        verts.push_back(Vertex3D{p0, normal, {0.0f, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{p1, normal, {uSpan, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{p2, normal, {uSpan, vSpan}, vertexColor});
        verts.push_back(Vertex3D{p3, normal, {0.0f, vSpan}, vertexColor});

        idx.push_back(base + 0U);
        idx.push_back(base + 1U);
        idx.push_back(base + 2U);
        idx.push_back(base + 0U);
        idx.push_back(base + 2U);
        idx.push_back(base + 3U);
    };

    const float uX = size.x * s;
    const float uY = size.y * s;
    const float uZ = size.z * s;

    // Front (+Z)
    addQuad(
        {-half.x, -half.y, +half.z},
        {+half.x, -half.y, +half.z},
        {+half.x, +half.y, +half.z},
        {-half.x, +half.y, +half.z},
        {0.0f, 0.0f, +1.0f},
        uX, uY);

    // Back (-Z)
    addQuad(
        {+half.x, -half.y, -half.z},
        {-half.x, -half.y, -half.z},
        {-half.x, +half.y, -half.z},
        {+half.x, +half.y, -half.z},
        {0.0f, 0.0f, -1.0f},
        uX, uY);

    // Right (+X)
    addQuad(
        {+half.x, -half.y, +half.z},
        {+half.x, -half.y, -half.z},
        {+half.x, +half.y, -half.z},
        {+half.x, +half.y, +half.z},
        {+1.0f, 0.0f, 0.0f},
        uZ, uY);

    // Left (-X)
    addQuad(
        {-half.x, -half.y, -half.z},
        {-half.x, -half.y, +half.z},
        {-half.x, +half.y, +half.z},
        {-half.x, +half.y, -half.z},
        {-1.0f, 0.0f, 0.0f},
        uZ, uY);

    // Top (+Y)
    addQuad(
        {-half.x, +half.y, +half.z},
        {+half.x, +half.y, +half.z},
        {+half.x, +half.y, -half.z},
        {-half.x, +half.y, -half.z},
        {0.0f, +1.0f, 0.0f},
        uX, uZ);

    // Bottom (-Y)
    addQuad(
        {-half.x, -half.y, -half.z},
        {+half.x, -half.y, -half.z},
        {+half.x, -half.y, +half.z},
        {-half.x, -half.y, +half.z},
        {0.0f, -1.0f, 0.0f},
        uX, uZ);

    return std::make_shared<Mesh>(std::move(verts), std::move(idx));
}

std::shared_ptr<Mesh> Mesh::createRamp(
    float width,
    float height,
    float depth,
    float uvScalePerMeter,
    const glm::vec3& vertexColor) {
    const float hw = std::max(0.01f, width * 0.5f);
    const float hh = std::max(0.01f, height * 0.5f);
    const float hd = std::max(0.01f, depth * 0.5f);
    const float s = std::max(0.01f, uvScalePerMeter);

    std::vector<Vertex3D> verts;
    std::vector<std::uint32_t> idx;

    // Slope normal pointing up and forward (+Y, +Z)
    const glm::vec3 slopeNormal = glm::normalize(glm::vec3(0.0f, depth, height));
    const float slopeLen = std::sqrt(depth * depth + height * height);

    // 1. Sloped top quad (front-bottom at +hd/-hh to back-top at -hd/+hh)
    {
        const auto base = static_cast<std::uint32_t>(verts.size());
        verts.push_back(Vertex3D{{-hw, -hh, +hd}, slopeNormal, {0.0f, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{+hw, -hh, +hd}, slopeNormal, {width * s, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{+hw, +hh, -hd}, slopeNormal, {width * s, slopeLen * s}, vertexColor});
        verts.push_back(Vertex3D{{-hw, +hh, -hd}, slopeNormal, {0.0f, slopeLen * s}, vertexColor});
        idx.insert(idx.end(), {base + 0U, base + 1U, base + 2U, base + 0U, base + 2U, base + 3U});
    }

    // 2. Back vertical quad (-Z)
    {
        const auto base = static_cast<std::uint32_t>(verts.size());
        const glm::vec3 n(0.0f, 0.0f, -1.0f);
        verts.push_back(Vertex3D{{+hw, -hh, -hd}, n, {0.0f, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{-hw, -hh, -hd}, n, {width * s, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{-hw, +hh, -hd}, n, {width * s, height * s}, vertexColor});
        verts.push_back(Vertex3D{{+hw, +hh, -hd}, n, {0.0f, height * s}, vertexColor});
        idx.insert(idx.end(), {base + 0U, base + 1U, base + 2U, base + 0U, base + 2U, base + 3U});
    }

    // 3. Bottom quad (-Y)
    {
        const auto base = static_cast<std::uint32_t>(verts.size());
        const glm::vec3 n(0.0f, -1.0f, 0.0f);
        verts.push_back(Vertex3D{{-hw, -hh, -hd}, n, {0.0f, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{+hw, -hh, -hd}, n, {width * s, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{+hw, -hh, +hd}, n, {width * s, depth * s}, vertexColor});
        verts.push_back(Vertex3D{{-hw, -hh, +hd}, n, {0.0f, depth * s}, vertexColor});
        idx.insert(idx.end(), {base + 0U, base + 1U, base + 2U, base + 0U, base + 2U, base + 3U});
    }

    // 4. Right triangular face (+X)
    {
        const auto base = static_cast<std::uint32_t>(verts.size());
        const glm::vec3 n(+1.0f, 0.0f, 0.0f);
        verts.push_back(Vertex3D{{+hw, -hh, +hd}, n, {0.0f, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{+hw, -hh, -hd}, n, {depth * s, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{+hw, +hh, -hd}, n, {depth * s, height * s}, vertexColor});
        idx.insert(idx.end(), {base + 0U, base + 1U, base + 2U});
    }

    // 5. Left triangular face (-X)
    {
        const auto base = static_cast<std::uint32_t>(verts.size());
        const glm::vec3 n(-1.0f, 0.0f, 0.0f);
        verts.push_back(Vertex3D{{-hw, -hh, -hd}, n, {0.0f, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{-hw, -hh, +hd}, n, {depth * s, 0.0f}, vertexColor});
        verts.push_back(Vertex3D{{-hw, +hh, -hd}, n, {0.0f, height * s}, vertexColor});
        idx.insert(idx.end(), {base + 0U, base + 1U, base + 2U});
    }

    return std::make_shared<Mesh>(std::move(verts), std::move(idx));
}

} // namespace classicfps::renderer
