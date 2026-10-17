#pragma once

#include "engine/renderer/IGraphicsDevice.hpp"

#include <cstdint>
#include <vector>

namespace classicfps::renderer {

class SoftwareGraphicsDevice final : public IGraphicsDevice {
public:
    SoftwareGraphicsDevice() = default;
    ~SoftwareGraphicsDevice() override;

    SoftwareGraphicsDevice(const SoftwareGraphicsDevice&) = delete;
    SoftwareGraphicsDevice& operator=(const SoftwareGraphicsDevice&) = delete;

    [[nodiscard]] bool initialize(platform::Window& window, bool vsync) override;
    void shutdown() noexcept override;

    void beginFrame() override;
    void clear(const ClearColor& color, bool clearDepth = true) override;

    RenderStats renderScene(
        const FirstPersonCamera& camera,
        const std::vector<RenderCommand>& commands,
        const DirectionalLight& dirLight,
        const std::vector<PointLight>& pointLights,
        const FogSettings& fog) override;

    void endFrame(platform::Window& window) override;

    void resizeViewport(int width, int height) override;
    bool setVSync(bool enabled) override;
    [[nodiscard]] bool saveScreenshotPPM(const std::string& filePath) const override;

    [[nodiscard]] bool isInitialized() const noexcept override { return initialized_; }
    [[nodiscard]] GraphicsBackendType backendType() const noexcept override {
        return GraphicsBackendType::SoftwareFallback;
    }
    [[nodiscard]] const GraphicsDeviceInfo& deviceInfo() const noexcept override { return info_; }

    [[nodiscard]] const std::vector<std::uint32_t>& colorBuffer() const noexcept {
        return colorBuffer_;
    }
    [[nodiscard]] std::vector<std::uint32_t>& mutableColorBuffer() noexcept {
        return colorBuffer_;
    }
    [[nodiscard]] const std::vector<float>& depthBuffer() const noexcept {
        return depthBuffer_;
    }

private:
    bool initialized_{false};
    GraphicsDeviceInfo info_{};
    std::vector<std::uint32_t> colorBuffer_;
    std::vector<float> depthBuffer_;
};

} // namespace classicfps::renderer
