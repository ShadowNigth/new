#pragma once

#include "engine/renderer/Camera.hpp"
#include "engine/renderer/SceneTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace classicfps::platform {
class Window;
}

namespace classicfps::renderer {

enum class GraphicsBackendType : std::uint8_t {
    OpenGL33Core = 0,
    SoftwareFallback = 1
};

struct ClearColor {
    float r{0.08f};
    float g{0.11f};
    float b{0.15f};
    float a{1.00f};
};

struct GraphicsDeviceInfo {
    GraphicsBackendType backend{GraphicsBackendType::OpenGL33Core};
    std::string backendName;
    std::string vendor;
    std::string renderer;
    std::string apiVersion;
    std::string shadingLanguageVersion;
    int viewportWidth{0};
    int viewportHeight{0};
    bool vsyncEnabled{false};
};

class IGraphicsDevice {
public:
    virtual ~IGraphicsDevice() = default;

    [[nodiscard]] virtual bool initialize(platform::Window& window, bool vsync) = 0;
    virtual void shutdown() noexcept = 0;

    virtual void beginFrame() = 0;
    virtual void clear(const ClearColor& color, bool clearDepth = true) = 0;

    virtual RenderStats renderScene(
        const FirstPersonCamera& camera,
        const std::vector<RenderCommand>& commands,
        const DirectionalLight& dirLight,
        const std::vector<PointLight>& pointLights,
        const FogSettings& fog) = 0;

    virtual void endFrame(platform::Window& window) = 0;

    virtual void resizeViewport(int width, int height) = 0;
    virtual bool setVSync(bool enabled) = 0;
    [[nodiscard]] virtual bool saveScreenshotPPM(const std::string& filePath) const = 0;

    [[nodiscard]] virtual bool isInitialized() const noexcept = 0;
    [[nodiscard]] virtual GraphicsBackendType backendType() const noexcept = 0;
    [[nodiscard]] virtual const GraphicsDeviceInfo& deviceInfo() const noexcept = 0;
};

} // namespace classicfps::renderer
