#pragma once

#include "engine/renderer/GL33Loader.hpp"
#include "engine/renderer/IGraphicsDevice.hpp"
#include "engine/renderer/Shader.hpp"

#include <unordered_map>

namespace classicfps::renderer {

class OpenGLGraphicsDevice final : public IGraphicsDevice {
public:
    OpenGLGraphicsDevice() = default;
    ~OpenGLGraphicsDevice() override;

    OpenGLGraphicsDevice(const OpenGLGraphicsDevice&) = delete;
    OpenGLGraphicsDevice& operator=(const OpenGLGraphicsDevice&) = delete;

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

    [[nodiscard]] bool isInitialized() const noexcept override { return glContext_ != nullptr; }
    [[nodiscard]] GraphicsBackendType backendType() const noexcept override {
        return GraphicsBackendType::OpenGL33Core;
    }
    [[nodiscard]] const GraphicsDeviceInfo& deviceInfo() const noexcept override { return info_; }
    [[nodiscard]] const GL33Functions& gl() const noexcept { return gl_; }

private:
    struct GpuMeshHandles {
        GLuint vao{0};
        GLuint vbo{0};
        GLuint ebo{0};
        GLsizei indexCount{0};
    };

    [[nodiscard]] const GpuMeshHandles& getOrCreateGpuMesh(const Mesh& mesh);
    [[nodiscard]] GLuint getOrCreateGpuTexture(const Texture2D& texture);
    void releaseGpuResources() noexcept;
    void checkGlError(const char* operationName);

    void* glContext_{nullptr};
    GL33Functions gl_{};
    GraphicsDeviceInfo info_{};
    ShaderProgram sceneShader_;
    std::shared_ptr<Texture2D> defaultWhiteTexture_;
    std::unordered_map<const Mesh*, GpuMeshHandles> meshCache_;
    std::unordered_map<const Texture2D*, GLuint> textureCache_;
};

} // namespace classicfps::renderer
