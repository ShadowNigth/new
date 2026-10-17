#pragma once

#include "engine/renderer/GL33Loader.hpp"

#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

namespace classicfps::renderer {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram() = default;

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    [[nodiscard]] static bool loadSourceFromFiles(
        const std::string& vertexPath,
        const std::string& fragmentPath,
        std::string& outVertexSrc,
        std::string& outFragmentSrc);

    [[nodiscard]] static std::string defaultVertexShaderSource();
    [[nodiscard]] static std::string defaultFragmentShaderSource();

    [[nodiscard]] static bool validateGlsl330Source(
        std::string_view source,
        std::string& outError);

    [[nodiscard]] bool compileAndLink(
        const GL33Functions& gl,
        std::string_view vertexSource,
        std::string_view fragmentSource);

    void destroy(const GL33Functions& gl) noexcept;
    void bind(const GL33Functions& gl) const noexcept;

    [[nodiscard]] bool isValid() const noexcept { return programId_ != 0; }
    [[nodiscard]] GLuint programId() const noexcept { return programId_; }

    void setInt(const GL33Functions& gl, const char* name, int value);
    void setFloat(const GL33Functions& gl, const char* name, float value);
    void setVec3(const GL33Functions& gl, const char* name, const glm::vec3& value);
    void setMat4(const GL33Functions& gl, const char* name, const glm::mat4& value);

private:
    [[nodiscard]] GLint uniformLocation(const GL33Functions& gl, const char* name);

    GLuint programId_{0};
    std::unordered_map<std::string, GLint> uniformCache_;
};

} // namespace classicfps::renderer
