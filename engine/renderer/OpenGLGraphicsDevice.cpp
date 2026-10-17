#include "engine/renderer/OpenGLGraphicsDevice.hpp"
#include "engine/core/Logger.hpp"
#include "engine/platform/Window.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstddef>
#include <sstream>

namespace classicfps::renderer {

namespace {

[[nodiscard]] std::string safeGlString(const GLubyte* bytes) {
    if (bytes == nullptr) {
        return "unknown";
    }
    return reinterpret_cast<const char*>(bytes);
}

} // namespace

OpenGLGraphicsDevice::~OpenGLGraphicsDevice() {
    shutdown();
}

bool OpenGLGraphicsDevice::initialize(platform::Window& window, bool vsync) {
    if (glContext_ != nullptr) {
        shutdown();
    }

    if (!window.isInitialized() || window.nativeHandle() == nullptr || !window.hasOpenGLFlag()) {
        core::Logger::instance().warn(
            "OpenGLDevice",
            "Window does not have SDL_WINDOW_OPENGL enabled; cannot create OpenGL 3.3 Core context.");
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window.nativeHandle());
    if (glContext_ == nullptr) {
        core::Logger::instance().warn(
            "OpenGLDevice",
            std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
        return false;
    }

    if (SDL_GL_MakeCurrent(window.nativeHandle(), glContext_) != 0) {
        core::Logger::instance().error(
            "OpenGLDevice",
            std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError());
        shutdown();
        return false;
    }

    if (!gl_.loadFromCurrentContext()) {
        shutdown();
        return false;
    }

    GLint major = 0;
    GLint minor = 0;
    gl_.GetIntegerv(GL_MAJOR_VERSION, &major);
    gl_.GetIntegerv(GL_MINOR_VERSION, &minor);

    if (major < 3 || (major == 3 && minor < 3)) {
        std::ostringstream oss;
        oss << "OpenGL 3.3 Core required, but driver reported version " << major << '.' << minor;
        core::Logger::instance().error("OpenGLDevice", oss.str());
        shutdown();
        return false;
    }

    info_.backend = GraphicsBackendType::OpenGL33Core;
    info_.backendName = "OpenGL 3.3 Core";
    info_.vendor = safeGlString(gl_.GetString(GL_VENDOR));
    info_.renderer = safeGlString(gl_.GetString(GL_RENDERER));
    info_.apiVersion = safeGlString(gl_.GetString(GL_VERSION));
    info_.shadingLanguageVersion = safeGlString(gl_.GetString(GL_SHADING_LANGUAGE_VERSION));
    info_.viewportWidth = window.width();
    info_.viewportHeight = window.height();

    gl_.Enable(GL_DEPTH_TEST);
    gl_.DepthFunc(GL_LEQUAL);
    gl_.Enable(GL_CULL_FACE);
    gl_.CullFace(GL_BACK);
    gl_.FrontFace(GL_CCW);

    std::string vertSrc;
    std::string fragSrc;
    if (!ShaderProgram::loadSourceFromFiles(
            "assets/shaders/classic_lit.vert",
            "assets/shaders/classic_lit.frag",
            vertSrc,
            fragSrc)) {
        vertSrc = ShaderProgram::defaultVertexShaderSource();
        fragSrc = ShaderProgram::defaultFragmentShaderSource();
    }

    if (!sceneShader_.compileAndLink(gl_, vertSrc, fragSrc)) {
        core::Logger::instance().error("OpenGLDevice", "Failed to compile/link default 3D scene shader");
        shutdown();
        return false;
    }

    defaultWhiteTexture_ = std::make_shared<Texture2D>(
        2, 2, std::vector<std::uint8_t>(2U * 2U * 4U, 255U));

    resizeViewport(window.width(), window.height());
    setVSync(vsync);

    checkGlError("OpenGLGraphicsDevice::initialize");

    std::ostringstream oss;
    oss << "Initialized OpenGL 3.3 Core context | Vendor: " << info_.vendor
        << " | Renderer: " << info_.renderer
        << " | Version: " << info_.apiVersion
        << " | GLSL: " << info_.shadingLanguageVersion;
    core::Logger::instance().info("OpenGLDevice", oss.str());
    return true;
}

void OpenGLGraphicsDevice::shutdown() noexcept {
    if (glContext_ != nullptr) {
        core::Logger::instance().info("OpenGLDevice", "Destroying OpenGL 3.3 Core context and GPU resources");
        releaseGpuResources();
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    defaultWhiteTexture_.reset();
    gl_ = GL33Functions{};
    info_ = GraphicsDeviceInfo{};
}

void OpenGLGraphicsDevice::beginFrame() {
    if (gl_.GetError != nullptr) {
        while (gl_.GetError() != GL_NO_ERROR) {
        }
    }
}

void OpenGLGraphicsDevice::clear(const ClearColor& color, bool clearDepth) {
    if (glContext_ == nullptr) {
        return;
    }
    gl_.ClearColor(color.r, color.g, color.b, color.a);
    GLbitfield mask = GL_COLOR_BUFFER_BIT;
    if (clearDepth) {
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    gl_.Clear(mask);
}

RenderStats OpenGLGraphicsDevice::renderScene(
    const FirstPersonCamera& camera,
    const std::vector<RenderCommand>& commands,
    const DirectionalLight& dirLight,
    const std::vector<PointLight>& pointLights,
    const FogSettings& fog) {
    RenderStats stats{};
    stats.submittedObjects = static_cast<std::uint32_t>(commands.size());

    if (glContext_ == nullptr || !sceneShader_.isValid()) {
        return stats;
    }

    const Frustum frustum = camera.extractFrustum();

    sceneShader_.bind(gl_);
    sceneShader_.setMat4(gl_, "uView", camera.viewMatrix());
    sceneShader_.setMat4(gl_, "uProjection", camera.projectionMatrix());

    sceneShader_.setVec3(gl_, "uDirLightDirection", dirLight.direction);
    sceneShader_.setVec3(gl_, "uDirLightColor", dirLight.color);
    sceneShader_.setFloat(gl_, "uDirLightIntensity", dirLight.intensity);
    sceneShader_.setVec3(gl_, "uAmbientColor", dirLight.ambientColor);
    sceneShader_.setFloat(gl_, "uAmbientIntensity", dirLight.ambientIntensity);

    const int plCount = static_cast<int>(std::min<std::size_t>(pointLights.size(), 4U));
    sceneShader_.setInt(gl_, "uPointLightCount", plCount);
    for (int i = 0; i < plCount; ++i) {
        const std::string idx = std::to_string(i);
        sceneShader_.setVec3(
            gl_, ("uPointLightPositions[" + idx + "]").c_str(), pointLights[static_cast<std::size_t>(i)].position);
        sceneShader_.setVec3(
            gl_, ("uPointLightColors[" + idx + "]").c_str(), pointLights[static_cast<std::size_t>(i)].color);
        sceneShader_.setFloat(
            gl_, ("uPointLightIntensities[" + idx + "]").c_str(), pointLights[static_cast<std::size_t>(i)].intensity);
        sceneShader_.setFloat(
            gl_, ("uPointLightRadii[" + idx + "]").c_str(), pointLights[static_cast<std::size_t>(i)].radius);
    }

    sceneShader_.setInt(gl_, "uFogEnabled", fog.enabled ? 1 : 0);
    sceneShader_.setVec3(gl_, "uFogColor", fog.color);
    sceneShader_.setFloat(gl_, "uFogStart", fog.startDistance);
    sceneShader_.setFloat(gl_, "uFogEnd", fog.endDistance);
    sceneShader_.setInt(gl_, "uAlbedoTex", 0);

    gl_.ActiveTexture(GL_TEXTURE0);

    for (const RenderCommand& cmd : commands) {
        if (!cmd.mesh || !cmd.mesh->isValid()) {
            continue;
        }

        const AABB worldBounds = cmd.mesh->localBounds().transform(cmd.modelMatrix);
        if (!frustum.intersectsAABB(worldBounds.min, worldBounds.max)) {
            ++stats.culledObjects;
            continue;
        }

        const GpuMeshHandles& gpuMesh = getOrCreateGpuMesh(*cmd.mesh);
        const Texture2D& tex = (cmd.texture && cmd.texture->isValid())
            ? *cmd.texture
            : *defaultWhiteTexture_;
        const GLuint texId = getOrCreateGpuTexture(tex);

        gl_.BindTexture(GL_TEXTURE_2D, texId);
        sceneShader_.setMat4(gl_, "uModel", cmd.modelMatrix);
        sceneShader_.setVec3(gl_, "uTintColor", cmd.tintColor);

        gl_.BindVertexArray(gpuMesh.vao);
        gl_.DrawElements(GL_TRIANGLES, gpuMesh.indexCount, GL_UNSIGNED_INT, nullptr);

        ++stats.visibleObjects;
        ++stats.drawCalls;
        stats.trianglesRendered += static_cast<std::uint32_t>(gpuMesh.indexCount / 3);
    }

    gl_.BindVertexArray(0);
    return stats;
}

void OpenGLGraphicsDevice::endFrame(platform::Window& window) {
    if (glContext_ == nullptr || window.nativeHandle() == nullptr) {
        return;
    }
    checkGlError("endFrame");
    SDL_GL_SwapWindow(window.nativeHandle());
}

void OpenGLGraphicsDevice::resizeViewport(int width, int height) {
    const int safeW = std::max(1, width);
    const int safeH = std::max(1, height);
    info_.viewportWidth = safeW;
    info_.viewportHeight = safeH;
    if (gl_.Viewport != nullptr) {
        gl_.Viewport(0, 0, safeW, safeH);
    }
}

bool OpenGLGraphicsDevice::setVSync(bool enabled) {
    if (glContext_ == nullptr) {
        return false;
    }
    if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0) {
        info_.vsyncEnabled = enabled;
        return true;
    }
    core::Logger::instance().warn(
        "OpenGLDevice",
        std::string("SDL_GL_SetSwapInterval not supported by current driver: ") + SDL_GetError());
    info_.vsyncEnabled = false;
    return false;
}

bool OpenGLGraphicsDevice::saveScreenshotPPM(const std::string& filePath) const {
    if (glContext_ == nullptr || gl_.ReadPixels == nullptr) {
        return false;
    }
    const int w = info_.viewportWidth;
    const int h = info_.viewportHeight;
    if (w <= 0 || h <= 0) {
        return false;
    }

    std::vector<std::uint8_t> raw(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4U);
    if (gl_.PixelStorei != nullptr) {
        gl_.PixelStorei(GL_PACK_ALIGNMENT, 1);
    }
    gl_.ReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());

    // Flip vertically from OpenGL bottom-left origin to top-left image origin
    std::vector<std::uint8_t> flipped(raw.size());
    const std::size_t rowBytes = static_cast<std::size_t>(w) * 4U;
    for (int y = 0; y < h; ++y) {
        const auto srcRow = static_cast<std::size_t>(h - 1 - y) * rowBytes;
        const auto dstRow = static_cast<std::size_t>(y) * rowBytes;
        std::copy_n(raw.data() + srcRow, rowBytes, flipped.data() + dstRow);
    }

    const Texture2D image(w, h, std::move(flipped));
    return image.saveToPPM(filePath);
}

const OpenGLGraphicsDevice::GpuMeshHandles& OpenGLGraphicsDevice::getOrCreateGpuMesh(const Mesh& mesh) {
    const auto it = meshCache_.find(&mesh);
    if (it != meshCache_.end()) {
        return it->second;
    }

    GpuMeshHandles handles{};
    gl_.GenVertexArrays(1, &handles.vao);
    gl_.GenBuffers(1, &handles.vbo);
    gl_.GenBuffers(1, &handles.ebo);
    handles.indexCount = static_cast<GLsizei>(mesh.indices().size());

    gl_.BindVertexArray(handles.vao);

    gl_.BindBuffer(GL_ARRAY_BUFFER, handles.vbo);
    gl_.BufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mesh.vertices().size() * sizeof(Vertex3D)),
        mesh.vertices().data(),
        GL_STATIC_DRAW);

    gl_.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, handles.ebo);
    gl_.BufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mesh.indices().size() * sizeof(std::uint32_t)),
        mesh.indices().data(),
        GL_STATIC_DRAW);

    const auto stride = static_cast<GLsizei>(sizeof(Vertex3D));
    // location 0: position (vec3)
    gl_.EnableVertexAttribArray(0);
    gl_.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex3D, position)));
    // location 1: normal (vec3)
    gl_.EnableVertexAttribArray(1);
    gl_.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex3D, normal)));
    // location 2: uv (vec2)
    gl_.EnableVertexAttribArray(2);
    gl_.VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex3D, uv)));
    // location 3: color (vec3)
    gl_.EnableVertexAttribArray(3);
    gl_.VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex3D, color)));

    gl_.BindVertexArray(0);

    const auto [insertedIt, _] = meshCache_.emplace(&mesh, handles);
    return insertedIt->second;
}

GLuint OpenGLGraphicsDevice::getOrCreateGpuTexture(const Texture2D& texture) {
    const auto it = textureCache_.find(&texture);
    if (it != textureCache_.end()) {
        return it->second;
    }

    GLuint texId = 0;
    gl_.GenTextures(1, &texId);
    gl_.BindTexture(GL_TEXTURE_2D, texId);
    // Classic GoldSrc pixelated nearest-neighbor filtering + repeat wrapping
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    gl_.TexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        texture.width(),
        texture.height(),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        texture.pixels().data());

    textureCache_.emplace(&texture, texId);
    return texId;
}

void OpenGLGraphicsDevice::releaseGpuResources() noexcept {
    for (auto& [_, h] : meshCache_) {
        if (h.ebo != 0 && gl_.DeleteBuffers != nullptr) gl_.DeleteBuffers(1, &h.ebo);
        if (h.vbo != 0 && gl_.DeleteBuffers != nullptr) gl_.DeleteBuffers(1, &h.vbo);
        if (h.vao != 0 && gl_.DeleteVertexArrays != nullptr) gl_.DeleteVertexArrays(1, &h.vao);
    }
    meshCache_.clear();

    for (auto& [_, texId] : textureCache_) {
        if (texId != 0 && gl_.DeleteTextures != nullptr) {
            gl_.DeleteTextures(1, &texId);
        }
    }
    textureCache_.clear();

    sceneShader_.destroy(gl_);
}

void OpenGLGraphicsDevice::checkGlError(const char* operationName) {
    if (gl_.GetError == nullptr) {
        return;
    }
    GLenum err = gl_.GetError();
    while (err != GL_NO_ERROR) {
        std::ostringstream oss;
        oss << "OpenGL error 0x" << std::hex << err << " after " << operationName;
        core::Logger::instance().error("OpenGLDevice", oss.str());
        err = gl_.GetError();
    }
}

} // namespace classicfps::renderer
