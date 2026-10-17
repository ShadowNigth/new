#pragma once

#include <GL/glcorearb.h>

namespace classicfps::renderer {

struct GL33Functions {
    // State & queries
    PFNGLGETSTRINGPROC               GetString{nullptr};
    PFNGLGETINTEGERVPROC             GetIntegerv{nullptr};
    PFNGLGETERRORPROC                GetError{nullptr};
    PFNGLVIEWPORTPROC                Viewport{nullptr};
    PFNGLCLEARCOLORPROC              ClearColor{nullptr};
    PFNGLCLEARDEPTHPROC              ClearDepth{nullptr};
    PFNGLCLEARPROC                   Clear{nullptr};
    PFNGLENABLEPROC                  Enable{nullptr};
    PFNGLDISABLEPROC                 Disable{nullptr};
    PFNGLDEPTHFUNCPROC               DepthFunc{nullptr};
    PFNGLDEPTHMASKPROC               DepthMask{nullptr};
    PFNGLBLENDFUNCPROC               BlendFunc{nullptr};
    PFNGLCULLFACEPROC                CullFace{nullptr};
    PFNGLFRONTFACEPROC               FrontFace{nullptr};
    PFNGLREADPIXELSPROC              ReadPixels{nullptr};
    PFNGLPIXELSTOREIPROC             PixelStorei{nullptr};

    // VAOs & VBOs (OpenGL 3.3 Core)
    PFNGLGENVERTEXARRAYSPROC         GenVertexArrays{nullptr};
    PFNGLBINDVERTEXARRAYPROC         BindVertexArray{nullptr};
    PFNGLDELETEVERTEXARRAYSPROC      DeleteVertexArrays{nullptr};
    PFNGLGENBUFFERSPROC              GenBuffers{nullptr};
    PFNGLBINDBUFFERPROC              BindBuffer{nullptr};
    PFNGLBUFFERDATAPROC              BufferData{nullptr};
    PFNGLDELETEBUFFERSPROC           DeleteBuffers{nullptr};
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer{nullptr};
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray{nullptr};

    // Textures (OpenGL 3.3 Core)
    PFNGLGENTEXTURESPROC             GenTextures{nullptr};
    PFNGLBINDTEXTUREPROC             BindTexture{nullptr};
    PFNGLDELETETEXTURESPROC          DeleteTextures{nullptr};
    PFNGLTEXIMAGE2DPROC              TexImage2D{nullptr};
    PFNGLTEXPARAMETERIPROC           TexParameteri{nullptr};
    PFNGLACTIVETEXTUREPROC           ActiveTexture{nullptr};
    PFNGLGENERATEMIPMAPPROC          GenerateMipmap{nullptr};

    // Shaders & Programs (OpenGL 3.3 Core)
    PFNGLCREATESHADERPROC            CreateShader{nullptr};
    PFNGLSHADERSOURCEPROC            ShaderSource{nullptr};
    PFNGLCOMPILESHADERPROC           CompileShader{nullptr};
    PFNGLGETSHADERIVPROC             GetShaderiv{nullptr};
    PFNGLGETSHADERINFOLOGPROC        GetShaderInfoLog{nullptr};
    PFNGLDELETESHADERPROC            DeleteShader{nullptr};
    PFNGLCREATEPROGRAMPROC           CreateProgram{nullptr};
    PFNGLATTACHSHADERPROC            AttachShader{nullptr};
    PFNGLLINKPROGRAMPROC             LinkProgram{nullptr};
    PFNGLGETPROGRAMIVPROC            GetProgramiv{nullptr};
    PFNGLGETPROGRAMINFOLOGPROC       GetProgramInfoLog{nullptr};
    PFNGLUSEPROGRAMPROC              UseProgram{nullptr};
    PFNGLDELETEPROGRAMPROC           DeleteProgram{nullptr};

    // Uniforms & Drawing
    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation{nullptr};
    PFNGLUNIFORM1IPROC               Uniform1i{nullptr};
    PFNGLUNIFORM1FPROC               Uniform1f{nullptr};
    PFNGLUNIFORM3FPROC               Uniform3f{nullptr};
    PFNGLUNIFORM3FVPROC              Uniform3fv{nullptr};
    PFNGLUNIFORM4FVPROC              Uniform4fv{nullptr};
    PFNGLUNIFORMMATRIX4FVPROC        UniformMatrix4fv{nullptr};
    PFNGLDRAWARRAYSPROC              DrawArrays{nullptr};
    PFNGLDRAWELEMENTSPROC            DrawElements{nullptr};

    [[nodiscard]] bool loadFromCurrentContext();
    [[nodiscard]] bool hasCoreMinimum() const noexcept;
};

} // namespace classicfps::renderer
