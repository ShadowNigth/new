#include "engine/renderer/SoftwareGraphicsDevice.hpp"
#include "engine/core/Logger.hpp"
#include "engine/platform/Window.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <sstream>

namespace classicfps::renderer {

namespace {

[[nodiscard]] std::uint8_t floatToByte(float v) noexcept {
    const float clamped = std::clamp(v, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(clamped * 255.0f + 0.5f);
}

[[nodiscard]] std::uint32_t packARGB8888(const glm::vec3& rgb, float alpha = 1.0f) noexcept {
    const std::uint32_t a = floatToByte(alpha);
    const std::uint32_t r = floatToByte(rgb.r);
    const std::uint32_t g = floatToByte(rgb.g);
    const std::uint32_t b = floatToByte(rgb.b);
    return (a << 24U) | (r << 16U) | (g << 8U) | b;
}

struct ShadedVertex {
    glm::vec4 viewPos{0.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 lighting{1.0f};
    float viewDist{0.0f};
};

[[nodiscard]] ShadedVertex lerpVertex(const ShadedVertex& a, const ShadedVertex& b, float t) noexcept {
    return ShadedVertex{
        .viewPos = glm::mix(a.viewPos, b.viewPos, t),
        .uv = glm::mix(a.uv, b.uv, t),
        .lighting = glm::mix(a.lighting, b.lighting, t),
        .viewDist = std::lerp(a.viewDist, b.viewDist, t)
    };
}

// Clips a triangle against the camera near plane (z_view <= -nearZ)
[[nodiscard]] int clipTriangleNearPlane(
    const std::array<ShadedVertex, 3>& inTri,
    float nearZ,
    std::array<ShadedVertex, 4>& outPoly) noexcept {
    const float limitZ = -nearZ;
    std::array<ShadedVertex, 4> temp{};
    int count = 0;

    for (std::size_t i = 0; i < 3; ++i) {
        const ShadedVertex& curr = inTri[i];
        const ShadedVertex& prev = inTri[(i + 2) % 3];

        const bool currInside = (curr.viewPos.z <= limitZ);
        const bool prevInside = (prev.viewPos.z <= limitZ);

        if (currInside != prevInside) {
            const float denom = curr.viewPos.z - prev.viewPos.z;
            const float t = (std::fabs(denom) > 1e-6f)
                ? std::clamp((limitZ - prev.viewPos.z) / denom, 0.0f, 1.0f)
                : 0.0f;
            temp[static_cast<std::size_t>(count++)] = lerpVertex(prev, curr, t);
        }
        if (currInside) {
            temp[static_cast<std::size_t>(count++)] = curr;
        }
    }

    outPoly = temp;
    return count;
}

[[nodiscard]] glm::vec3 evaluateVertexLighting(
    const glm::vec3& worldPos,
    const glm::vec3& worldNormal,
    const glm::vec3& vertexColor,
    const glm::vec3& tintColor,
    const DirectionalLight& dirLight,
    const std::vector<PointLight>& pointLights) noexcept {
    const glm::vec3 N = glm::normalize(worldNormal);
    const glm::vec3 L = glm::normalize(-dirLight.direction);
    const float ndotl = std::max(glm::dot(N, L), 0.0f);

    glm::vec3 totalLight = dirLight.ambientColor * dirLight.ambientIntensity +
                           dirLight.color * (dirLight.intensity * ndotl);

    const std::size_t maxLights = std::min<std::size_t>(pointLights.size(), 4U);
    for (std::size_t i = 0; i < maxLights; ++i) {
        const PointLight& pl = pointLights[i];
        const glm::vec3 toLight = pl.position - worldPos;
        const float dist = glm::length(toLight);
        const float radius = std::max(pl.radius, 0.001f);
        if (dist < radius) {
            const glm::vec3 pDir = toLight / std::max(dist, 0.0001f);
            const float pNdotL = std::max(glm::dot(N, pDir), 0.0f);
            const float atten = (1.0f - dist / radius) * (1.0f - dist / radius);
            totalLight += pl.color * (pl.intensity * pNdotL * atten);
        }
    }

    return totalLight * vertexColor * tintColor;
}

} // namespace

SoftwareGraphicsDevice::~SoftwareGraphicsDevice() {
    shutdown();
}

bool SoftwareGraphicsDevice::initialize(platform::Window& window, bool vsync) {
    if (initialized_) {
        shutdown();
    }
    if (!window.isInitialized() || window.nativeHandle() == nullptr) {
        core::Logger::instance().error(
            "SoftwareDevice",
            "Cannot initialize SoftwareGraphicsDevice without a valid SDL_Window");
        return false;
    }

    info_.backend = GraphicsBackendType::SoftwareFallback;
    info_.backendName = "Software Framebuffer (SDL2)";
    info_.vendor = "ClassicFPS Engine";
    info_.renderer = "CPU 3D Rasterizer + Z-Buffer (" + window.videoDriverName() + ")";
    info_.apiVersion = "Software 3D 1.0 (OpenGL 3.3 Core Fallback)";
    info_.shadingLanguageVersion = "CPU Pipeline (Perspective-Correct UV + Fog)";
    info_.vsyncEnabled = vsync;

    resizeViewport(window.width(), window.height());
    initialized_ = true;

    std::ostringstream oss;
    oss << "Initialized SoftwareGraphicsDevice (" << info_.viewportWidth << "x"
        << info_.viewportHeight << ", driver=" << window.videoDriverName() << ")";
    core::Logger::instance().info("SoftwareDevice", oss.str());
    return true;
}

void SoftwareGraphicsDevice::shutdown() noexcept {
    if (initialized_) {
        core::Logger::instance().info("SoftwareDevice", "Shutting down SoftwareGraphicsDevice");
    }
    colorBuffer_.clear();
    depthBuffer_.clear();
    info_ = GraphicsDeviceInfo{};
    initialized_ = false;
}

void SoftwareGraphicsDevice::beginFrame() {}

void SoftwareGraphicsDevice::clear(const ClearColor& color, bool clearDepth) {
    if (!initialized_) {
        return;
    }
    const std::uint32_t packed = packARGB8888({color.r, color.g, color.b}, color.a);
    std::fill(colorBuffer_.begin(), colorBuffer_.end(), packed);
    if (clearDepth) {
        std::fill(depthBuffer_.begin(), depthBuffer_.end(), 1.0f);
    }
}

RenderStats SoftwareGraphicsDevice::renderScene(
    const FirstPersonCamera& camera,
    const std::vector<RenderCommand>& commands,
    const DirectionalLight& dirLight,
    const std::vector<PointLight>& pointLights,
    const FogSettings& fog) {
    RenderStats stats{};
    stats.submittedObjects = static_cast<std::uint32_t>(commands.size());
    if (!initialized_ || info_.viewportWidth <= 0 || info_.viewportHeight <= 0) {
        return stats;
    }

    const Frustum frustum = camera.extractFrustum();
    const glm::mat4 view = camera.viewMatrix();
    const glm::mat4 proj = camera.projectionMatrix();
    const float nearPlane = camera.nearPlane();
    const int vpW = info_.viewportWidth;
    const int vpH = info_.viewportHeight;
    const float halfW = static_cast<float>(vpW) * 0.5f;
    const float halfH = static_cast<float>(vpH) * 0.5f;

    struct ScreenVertex {
        float sx{0.0f};
        float sy{0.0f};
        float depth{0.0f};
        float invW{1.0f};
        glm::vec2 uvOverW{0.0f};
        glm::vec3 lightOverW{0.0f};
        float distOverW{0.0f};
    };

    auto projectVertex = [&](const ShadedVertex& sv) -> ScreenVertex {
        const glm::vec4 clip = proj * sv.viewPos;
        const float w = std::max(clip.w, 1e-5f);
        const float invW = 1.0f / w;
        const float ndcX = clip.x * invW;
        const float ndcY = clip.y * invW;
        const float ndcZ = clip.z * invW;

        return ScreenVertex{
            .sx = (ndcX + 1.0f) * halfW,
            .sy = (1.0f - ndcY) * halfH,
            .depth = (ndcZ + 1.0f) * 0.5f,
            .invW = invW,
            .uvOverW = sv.uv * invW,
            .lightOverW = sv.lighting * invW,
            .distOverW = sv.viewDist * invW
        };
    };

    auto edgeFunction = [](float ax, float ay, float bx, float by, float px, float py) noexcept {
        return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    };

    for (const RenderCommand& cmd : commands) {
        if (!cmd.mesh || !cmd.mesh->isValid()) {
            continue;
        }

        const AABB worldBounds = cmd.mesh->localBounds().transform(cmd.modelMatrix);
        if (!frustum.intersectsAABB(worldBounds.min, worldBounds.max)) {
            ++stats.culledObjects;
            continue;
        }

        ++stats.visibleObjects;
        ++stats.drawCalls;

        const auto& verts = cmd.mesh->vertices();
        const auto& indices = cmd.mesh->indices();
        const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(cmd.modelMatrix)));
        const Texture2D* tex = (cmd.texture && cmd.texture->isValid()) ? cmd.texture.get() : nullptr;

        for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            std::array<ShadedVertex, 3> tri{};
            for (std::size_t k = 0; k < 3; ++k) {
                const Vertex3D& v = verts[indices[i + k]];
                const glm::vec4 worldPos4 = cmd.modelMatrix * glm::vec4(v.position, 1.0f);
                const glm::vec3 worldPos(worldPos4.x, worldPos4.y, worldPos4.z);
                const glm::vec3 worldNormal = normalMat * v.normal;
                const glm::vec4 viewPos = view * worldPos4;

                tri[k] = ShadedVertex{
                    .viewPos = viewPos,
                    .uv = v.uv,
                    .lighting = evaluateVertexLighting(
                        worldPos, worldNormal, v.color, cmd.tintColor, dirLight, pointLights),
                    .viewDist = glm::length(glm::vec3(viewPos))
                };
            }

            std::array<ShadedVertex, 4> clipped{};
            const int clipCount = clipTriangleNearPlane(tri, nearPlane, clipped);
            if (clipCount < 3) {
                continue;
            }

            for (int fan = 1; fan + 1 < clipCount; ++fan) {
                const ScreenVertex v0 = projectVertex(clipped[0]);
                const ScreenVertex v1 = projectVertex(clipped[static_cast<std::size_t>(fan)]);
                const ScreenVertex v2 = projectVertex(clipped[static_cast<std::size_t>(fan + 1)]);

                // Note: screen Y goes down while NDC Y goes up, so CCW in NDC has negative signed area in screen space
                const float area = edgeFunction(v0.sx, v0.sy, v1.sx, v1.sy, v2.sx, v2.sy);
                if (area >= -1e-4f) {
                    // Back-facing or degenerate triangle
                    continue;
                }
                const float invArea = 1.0f / area;

                const int minX = std::clamp(
                    static_cast<int>(std::floor(std::min({v0.sx, v1.sx, v2.sx}))), 0, vpW - 1);
                const int maxX = std::clamp(
                    static_cast<int>(std::ceil(std::max({v0.sx, v1.sx, v2.sx}))), 0, vpW - 1);
                const int minY = std::clamp(
                    static_cast<int>(std::floor(std::min({v0.sy, v1.sy, v2.sy}))), 0, vpH - 1);
                const int maxY = std::clamp(
                    static_cast<int>(std::ceil(std::max({v0.sy, v1.sy, v2.sy}))), 0, vpH - 1);

                if (minX > maxX || minY > maxY) {
                    continue;
                }

                ++stats.trianglesRendered;

                for (int py = minY; py <= maxY; ++py) {
                    const float sampleY = static_cast<float>(py) + 0.5f;
                    const std::size_t rowOffset =
                        static_cast<std::size_t>(py) * static_cast<std::size_t>(vpW);

                    for (int px = minX; px <= maxX; ++px) {
                        const float sampleX = static_cast<float>(px) + 0.5f;

                        const float w0 = edgeFunction(v1.sx, v1.sy, v2.sx, v2.sy, sampleX, sampleY) * invArea;
                        const float w1 = edgeFunction(v2.sx, v2.sy, v0.sx, v0.sy, sampleX, sampleY) * invArea;
                        const float w2 = 1.0f - w0 - w1;

                        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                            continue;
                        }

                        const float depth = w0 * v0.depth + w1 * v1.depth + w2 * v2.depth;
                        if (depth < 0.0f || depth > 1.0f) {
                            continue;
                        }

                        const std::size_t pixelIdx = rowOffset + static_cast<std::size_t>(px);
                        if (depth > depthBuffer_[pixelIdx]) {
                            continue;
                        }
                        depthBuffer_[pixelIdx] = depth;

                        const float interpInvW =
                            std::max(w0 * v0.invW + w1 * v1.invW + w2 * v2.invW, 1e-7f);
                        const float corrW = 1.0f / interpInvW;

                        const glm::vec2 uv =
                            (w0 * v0.uvOverW + w1 * v1.uvOverW + w2 * v2.uvOverW) * corrW;
                        const glm::vec3 lighting =
                            (w0 * v0.lightOverW + w1 * v1.lightOverW + w2 * v2.lightOverW) * corrW;
                        const float viewDist =
                            (w0 * v0.distOverW + w1 * v1.distOverW + w2 * v2.distOverW) * corrW;

                        const glm::vec4 texel = (tex != nullptr)
                            ? tex->sampleNearest(uv.x, uv.y)
                            : glm::vec4(1.0f);

                        glm::vec3 color = glm::vec3(texel) * lighting;
                        if (fog.enabled) {
                            const float fogSpan = std::max(fog.endDistance - fog.startDistance, 0.001f);
                            const float fogFactor =
                                std::clamp((fog.endDistance - viewDist) / fogSpan, 0.0f, 1.0f);
                            color = glm::mix(fog.color, color, fogFactor);
                        }

                        colorBuffer_[pixelIdx] = packARGB8888(color, texel.a);
                    }
                }
            }
        }
    }

    return stats;
}

void SoftwareGraphicsDevice::endFrame(platform::Window& window) {
    if (!initialized_ || window.nativeHandle() == nullptr) {
        return;
    }

    SDL_Surface* surface = SDL_GetWindowSurface(window.nativeHandle());
    if (surface != nullptr && surface->pixels != nullptr) {
        const int copyW = std::min(info_.viewportWidth, surface->w);
        const int copyH = std::min(info_.viewportHeight, surface->h);

        if (SDL_MUSTLOCK(surface)) {
            SDL_LockSurface(surface);
        }

        auto* dstBytes = static_cast<std::uint8_t*>(surface->pixels);
        for (int y = 0; y < copyH; ++y) {
            const std::uint32_t* srcRow =
                colorBuffer_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(info_.viewportWidth);
            std::memcpy(
                dstBytes + static_cast<std::size_t>(y) * static_cast<std::size_t>(surface->pitch),
                srcRow,
                static_cast<std::size_t>(copyW) * sizeof(std::uint32_t));
        }

        if (SDL_MUSTLOCK(surface)) {
            SDL_UnlockSurface(surface);
        }
        SDL_UpdateWindowSurface(window.nativeHandle());
    }
}

void SoftwareGraphicsDevice::resizeViewport(int width, int height) {
    const int safeW = std::max(1, width);
    const int safeH = std::max(1, height);
    info_.viewportWidth = safeW;
    info_.viewportHeight = safeH;

    const std::size_t pixelCount =
        static_cast<std::size_t>(safeW) * static_cast<std::size_t>(safeH);
    colorBuffer_.assign(pixelCount, 0xFF000000U);
    depthBuffer_.assign(pixelCount, 1.0f);
}

bool SoftwareGraphicsDevice::setVSync(bool enabled) {
    info_.vsyncEnabled = enabled;
    return true;
}

bool SoftwareGraphicsDevice::saveScreenshotPPM(const std::string& filePath) const {
    if (!initialized_ || info_.viewportWidth <= 0 || info_.viewportHeight <= 0) {
        return false;
    }

    std::vector<std::uint8_t> rgba(colorBuffer_.size() * 4U);
    for (std::size_t i = 0; i < colorBuffer_.size(); ++i) {
        const std::uint32_t argb = colorBuffer_[i];
        rgba[i * 4U + 0] = static_cast<std::uint8_t>((argb >> 16U) & 0xFFU);
        rgba[i * 4U + 1] = static_cast<std::uint8_t>((argb >> 8U) & 0xFFU);
        rgba[i * 4U + 2] = static_cast<std::uint8_t>(argb & 0xFFU);
        rgba[i * 4U + 3] = static_cast<std::uint8_t>((argb >> 24U) & 0xFFU);
    }

    const Texture2D img(info_.viewportWidth, info_.viewportHeight, std::move(rgba));
    return img.saveToPPM(filePath);
}

} // namespace classicfps::renderer
