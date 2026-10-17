#include "engine/renderer/GL33Loader.hpp"
#include "engine/core/Logger.hpp"

#include <SDL2/SDL.h>
#include <cstring>

namespace classicfps::renderer {

namespace {

template <typename FuncPtr>
void loadProc(FuncPtr& target, const char* symbolName) noexcept {
    void* raw = SDL_GL_GetProcAddress(symbolName);
    static_assert(sizeof(FuncPtr) == sizeof(void*), "Function pointer size must match void*");
    std::memcpy(&target, &raw, sizeof(FuncPtr));
}

} // namespace

bool GL33Functions::loadFromCurrentContext() {
    loadProc(GetString, "glGetString");
    loadProc(GetIntegerv, "glGetIntegerv");
    loadProc(GetError, "glGetError");
    loadProc(Viewport, "glViewport");
    loadProc(ClearColor, "glClearColor");
    loadProc(ClearDepth, "glClearDepth");
    loadProc(Clear, "glClear");
    loadProc(Enable, "glEnable");
    loadProc(Disable, "glDisable");
    loadProc(DepthFunc, "glDepthFunc");
    loadProc(DepthMask, "glDepthMask");
    loadProc(BlendFunc, "glBlendFunc");
    loadProc(CullFace, "glCullFace");
    loadProc(FrontFace, "glFrontFace");
    loadProc(ReadPixels, "glReadPixels");
    loadProc(PixelStorei, "glPixelStorei");

    loadProc(GenVertexArrays, "glGenVertexArrays");
    loadProc(BindVertexArray, "glBindVertexArray");
    loadProc(DeleteVertexArrays, "glDeleteVertexArrays");
    loadProc(GenBuffers, "glGenBuffers");
    loadProc(BindBuffer, "glBindBuffer");
    loadProc(BufferData, "glBufferData");
    loadProc(DeleteBuffers, "glDeleteBuffers");
    loadProc(VertexAttribPointer, "glVertexAttribPointer");
    loadProc(EnableVertexAttribArray, "glEnableVertexAttribArray");

    loadProc(GenTextures, "glGenTextures");
    loadProc(BindTexture, "glBindTexture");
    loadProc(DeleteTextures, "glDeleteTextures");
    loadProc(TexImage2D, "glTexImage2D");
    loadProc(TexParameteri, "glTexParameteri");
    loadProc(ActiveTexture, "glActiveTexture");
    loadProc(GenerateMipmap, "glGenerateMipmap");

    loadProc(CreateShader, "glCreateShader");
    loadProc(ShaderSource, "glShaderSource");
    loadProc(CompileShader, "glCompileShader");
    loadProc(GetShaderiv, "glGetShaderiv");
    loadProc(GetShaderInfoLog, "glGetShaderInfoLog");
    loadProc(DeleteShader, "glDeleteShader");
    loadProc(CreateProgram, "glCreateProgram");
    loadProc(AttachShader, "glAttachShader");
    loadProc(LinkProgram, "glLinkProgram");
    loadProc(GetProgramiv, "glGetProgramiv");
    loadProc(GetProgramInfoLog, "glGetProgramInfoLog");
    loadProc(UseProgram, "glUseProgram");
    loadProc(DeleteProgram, "glDeleteProgram");

    loadProc(GetUniformLocation, "glGetUniformLocation");
    loadProc(Uniform1i, "glUniform1i");
    loadProc(Uniform1f, "glUniform1f");
    loadProc(Uniform3f, "glUniform3f");
    loadProc(Uniform3fv, "glUniform3fv");
    loadProc(Uniform4fv, "glUniform4fv");
    loadProc(UniformMatrix4fv, "glUniformMatrix4fv");
    loadProc(DrawArrays, "glDrawArrays");
    loadProc(DrawElements, "glDrawElements");

    if (!hasCoreMinimum()) {
        core::Logger::instance().error(
            "GL33Loader",
            "Failed to resolve required OpenGL 3.3 Core entry points via SDL_GL_GetProcAddress");
        return false;
    }

    return true;
}

bool GL33Functions::hasCoreMinimum() const noexcept {
    return GetString != nullptr &&
           GetIntegerv != nullptr &&
           GetError != nullptr &&
           Viewport != nullptr &&
           ClearColor != nullptr &&
           Clear != nullptr &&
           Enable != nullptr &&
           GenVertexArrays != nullptr &&
           GenBuffers != nullptr &&
           GenTextures != nullptr &&
           BindTexture != nullptr &&
           TexImage2D != nullptr &&
           CreateShader != nullptr &&
           CreateProgram != nullptr &&
           DrawElements != nullptr;
}

} // namespace classicfps::renderer
