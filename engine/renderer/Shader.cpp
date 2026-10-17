#include "engine/renderer/Shader.hpp"
#include "engine/core/Logger.hpp"

#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <vector>

namespace classicfps::renderer {

namespace {

[[nodiscard]] GLuint compileStage(
    const GL33Functions& gl,
    GLenum shaderType,
    std::string_view source,
    const char* stageLabel) {
    const GLuint shader = gl.CreateShader(shaderType);
    if (shader == 0) {
        core::Logger::instance().error("Shader", std::string("glCreateShader failed for ") + stageLabel);
        return 0;
    }

    const char* srcPtr = source.data();
    const auto srcLen = static_cast<GLint>(source.size());
    gl.ShaderSource(shader, 1, &srcPtr, &srcLen);
    gl.CompileShader(shader);

    GLint compiled = GL_FALSE;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint logLen = 0;
        gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::string infoLog(static_cast<std::size_t>(std::max(1, logLen)), '\0');
        gl.GetShaderInfoLog(shader, logLen, nullptr, infoLog.data());
        core::Logger::instance().error(
            "Shader",
            std::string("GLSL compile error (") + stageLabel + "): " + infoLog);
        gl.DeleteShader(shader);
        return 0;
    }

    return shader;
}

} // namespace

bool ShaderProgram::loadSourceFromFiles(
    const std::string& vertexPath,
    const std::string& fragmentPath,
    std::string& outVertexSrc,
    std::string& outFragmentSrc) {
    std::ifstream vFile(vertexPath);
    std::ifstream fFile(fragmentPath);
    if (!vFile.is_open() || !fFile.is_open()) {
        return false;
    }

    std::ostringstream vBuf;
    std::ostringstream fBuf;
    vBuf << vFile.rdbuf();
    fBuf << fFile.rdbuf();

    outVertexSrc = vBuf.str();
    outFragmentSrc = fBuf.str();
    return !outVertexSrc.empty() && !outFragmentSrc.empty();
}

std::string ShaderProgram::defaultVertexShaderSource() {
    return R"(#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec3 vColor;
out float vViewDistance;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vec4 viewPos  = uView * worldPos;
    vWorldPos     = worldPos.xyz;
    vNormal       = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vTexCoord     = aTexCoord;
    vColor        = aColor;
    vViewDistance = length(viewPos.xyz);
    gl_Position   = uProjection * viewPos;
}
)";
}

std::string ShaderProgram::defaultFragmentShaderSource() {
    return R"(#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vColor;
in float vViewDistance;

out vec4 FragColor;

uniform sampler2D uAlbedoTex;
uniform vec3 uTintColor;

uniform vec3 uDirLightDirection;
uniform vec3 uDirLightColor;
uniform float uDirLightIntensity;
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;

const int MAX_POINT_LIGHTS = 4;
uniform int uPointLightCount;
uniform vec3 uPointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColors[MAX_POINT_LIGHTS];
uniform float uPointLightIntensities[MAX_POINT_LIGHTS];
uniform float uPointLightRadii[MAX_POINT_LIGHTS];

uniform int uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

void main() {
    vec4 texSample = texture(uAlbedoTex, vTexCoord);
    vec3 baseAlbedo = texSample.rgb * vColor * uTintColor;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uDirLightDirection);
    float ndotl = max(dot(N, L), 0.0);

    vec3 lighting = uAmbientColor * uAmbientIntensity +
                    uDirLightColor * (uDirLightIntensity * ndotl);

    for (int i = 0; i < uPointLightCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 toLight = uPointLightPositions[i] - vWorldPos;
        float dist = length(toLight);
        float radius = max(uPointLightRadii[i], 0.001);
        if (dist < radius) {
            vec3 pDir = toLight / max(dist, 0.0001);
            float pNdotL = max(dot(N, pDir), 0.0);
            float atten = 1.0 - (dist / radius);
            atten = atten * atten;
            lighting += uPointLightColors[i] * (uPointLightIntensities[i] * pNdotL * atten);
        }
    }

    vec3 litColor = baseAlbedo * lighting;

    if (uFogEnabled != 0) {
        float fogSpan = max(uFogEnd - uFogStart, 0.001);
        float fogFactor = clamp((uFogEnd - vViewDistance) / fogSpan, 0.0, 1.0);
        litColor = mix(uFogColor, litColor, fogFactor);
    }

    FragColor = vec4(litColor, texSample.a);
}
)";
}

bool ShaderProgram::validateGlsl330Source(std::string_view source, std::string& outError) {
    if (source.find("#version 330 core") == std::string_view::npos) {
        outError = "Missing '#version 330 core' directive";
        return false;
    }
    if (source.find("void main()") == std::string_view::npos &&
        source.find("void main(") == std::string_view::npos) {
        outError = "Missing 'void main()' entry point";
        return false;
    }

    int braceDepth = 0;
    for (char c : source) {
        if (c == '{') ++braceDepth;
        if (c == '}') --braceDepth;
        if (braceDepth < 0) {
            outError = "Unbalanced closing brace '}' in GLSL source";
            return false;
        }
    }
    if (braceDepth != 0) {
        outError = "Unbalanced braces '{}' in GLSL source";
        return false;
    }

    outError.clear();
    return true;
}

bool ShaderProgram::compileAndLink(
    const GL33Functions& gl,
    std::string_view vertexSource,
    std::string_view fragmentSource) {
    std::string validationErr;
    if (!validateGlsl330Source(vertexSource, validationErr)) {
        core::Logger::instance().error("Shader", "Vertex shader validation failed: " + validationErr);
        return false;
    }
    if (!validateGlsl330Source(fragmentSource, validationErr)) {
        core::Logger::instance().error("Shader", "Fragment shader validation failed: " + validationErr);
        return false;
    }

    destroy(gl);

    const GLuint vs = compileStage(gl, GL_VERTEX_SHADER, vertexSource, "vertex");
    if (vs == 0) {
        return false;
    }

    const GLuint fs = compileStage(gl, GL_FRAGMENT_SHADER, fragmentSource, "fragment");
    if (fs == 0) {
        gl.DeleteShader(vs);
        return false;
    }

    const GLuint prog = gl.CreateProgram();
    gl.AttachShader(prog, vs);
    gl.AttachShader(prog, fs);
    gl.LinkProgram(prog);

    gl.DeleteShader(vs);
    gl.DeleteShader(fs);

    GLint linked = GL_FALSE;
    gl.GetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint logLen = 0;
        gl.GetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string infoLog(static_cast<std::size_t>(std::max(1, logLen)), '\0');
        gl.GetProgramInfoLog(prog, logLen, nullptr, infoLog.data());
        core::Logger::instance().error("Shader", "GLSL program link error: " + infoLog);
        gl.DeleteProgram(prog);
        return false;
    }

    programId_ = prog;
    uniformCache_.clear();
    return true;
}

void ShaderProgram::destroy(const GL33Functions& gl) noexcept {
    if (programId_ != 0 && gl.DeleteProgram != nullptr) {
        gl.DeleteProgram(programId_);
    }
    programId_ = 0;
    uniformCache_.clear();
}

void ShaderProgram::bind(const GL33Functions& gl) const noexcept {
    if (programId_ != 0 && gl.UseProgram != nullptr) {
        gl.UseProgram(programId_);
    }
}

void ShaderProgram::setInt(const GL33Functions& gl, const char* name, int value) {
    const GLint loc = uniformLocation(gl, name);
    if (loc >= 0) {
        gl.Uniform1i(loc, value);
    }
}

void ShaderProgram::setFloat(const GL33Functions& gl, const char* name, float value) {
    const GLint loc = uniformLocation(gl, name);
    if (loc >= 0) {
        gl.Uniform1f(loc, value);
    }
}

void ShaderProgram::setVec3(const GL33Functions& gl, const char* name, const glm::vec3& value) {
    const GLint loc = uniformLocation(gl, name);
    if (loc >= 0) {
        gl.Uniform3f(loc, value.x, value.y, value.z);
    }
}

void ShaderProgram::setMat4(const GL33Functions& gl, const char* name, const glm::mat4& value) {
    const GLint loc = uniformLocation(gl, name);
    if (loc >= 0) {
        gl.UniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    }
}

GLint ShaderProgram::uniformLocation(const GL33Functions& gl, const char* name) {
    if (programId_ == 0 || name == nullptr) {
        return -1;
    }
    const std::string key(name);
    const auto it = uniformCache_.find(key);
    if (it != uniformCache_.end()) {
        return it->second;
    }
    const GLint loc = gl.GetUniformLocation(programId_, name);
    uniformCache_.emplace(key, loc);
    return loc;
}

} // namespace classicfps::renderer
