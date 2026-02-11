#pragma once
/// Minimal OpenGL 3.3 Core Profile loader.
/// Loads only the functions needed by the God Simulation renderer.
/// Uses glfwGetProcAddress — must be called AFTER glfwMakeContextCurrent().

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>

// ═══════════════════════════════════════════════════════════════
//  GL TYPES
// ═══════════════════════════════════════════════════════════════
typedef unsigned int    GLenum;
typedef unsigned char   GLboolean;
typedef unsigned int    GLbitfield;
typedef void            GLvoid;
typedef int8_t          GLbyte;
typedef uint8_t         GLubyte;
typedef int16_t         GLshort;
typedef uint16_t        GLushort;
typedef int             GLint;
typedef unsigned int    GLuint;
typedef int             GLsizei;
typedef float           GLfloat;
typedef double          GLdouble;
typedef char            GLchar;
typedef ptrdiff_t       GLsizeiptr;
typedef ptrdiff_t       GLintptr;

// ═══════════════════════════════════════════════════════════════
//  GL CONSTANTS
// ═══════════════════════════════════════════════════════════════

// Boolean
#define GL_TRUE                     1
#define GL_FALSE                    0

// Clear bits
#define GL_DEPTH_BUFFER_BIT         0x00000100
#define GL_COLOR_BUFFER_BIT         0x00004000

// Primitive types
#define GL_POINTS                   0x0000
#define GL_LINES                    0x0001
#define GL_LINE_STRIP               0x0003
#define GL_TRIANGLES                0x0004
#define GL_TRIANGLE_STRIP           0x0005
#define GL_TRIANGLE_FAN             0x0006

// Data types
#define GL_BYTE                     0x1400
#define GL_UNSIGNED_BYTE            0x1401
#define GL_SHORT                    0x1402
#define GL_UNSIGNED_SHORT           0x1403
#define GL_INT                      0x1404
#define GL_UNSIGNED_INT             0x1405
#define GL_FLOAT                    0x1406

// Enable/Disable caps
#define GL_DEPTH_TEST               0x0B71
#define GL_CULL_FACE                0x0B44
#define GL_BLEND                    0x0BE2
#define GL_LINE_SMOOTH              0x0B20
#define GL_MULTISAMPLE              0x809D

// Depth functions
#define GL_LESS                     0x0201
#define GL_LEQUAL                   0x0203

// Blend functions
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303

// Face culling
#define GL_FRONT                    0x0404
#define GL_BACK                     0x0405
#define GL_CW                       0x0900
#define GL_CCW                      0x0901

// Polygon mode
#define GL_POINT                    0x1B00
#define GL_LINE                     0x1B01
#define GL_FILL                     0x1B02
#define GL_FRONT_AND_BACK           0x0408

// Buffer targets
#define GL_ARRAY_BUFFER             0x8892
#define GL_ELEMENT_ARRAY_BUFFER     0x8893

// Buffer usage
#define GL_STATIC_DRAW              0x88E4
#define GL_DYNAMIC_DRAW             0x88E8

// Shader types
#define GL_FRAGMENT_SHADER          0x8B30
#define GL_VERTEX_SHADER            0x8B31

// Shader queries
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82
#define GL_INFO_LOG_LENGTH          0x8B84

// Texture
#define GL_TEXTURE_2D               0x0DE1
#define GL_TEXTURE0                 0x84C0
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803
#define GL_LINEAR                   0x2601
#define GL_NEAREST                  0x2600
#define GL_CLAMP_TO_EDGE            0x812F
#define GL_RGB                      0x1907
#define GL_RGBA                     0x1908

// Get
#define GL_VENDOR                   0x1F00
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02

// ═══════════════════════════════════════════════════════════════
//  GL FUNCTION POINTERS (C++17 inline variables)
// ═══════════════════════════════════════════════════════════════
namespace gl {

// --- Basic State ---
using ClearColorFn       = void(*)(GLfloat, GLfloat, GLfloat, GLfloat);
using ClearFn            = void(*)(GLbitfield);
using ViewportFn         = void(*)(GLint, GLint, GLsizei, GLsizei);
using EnableFn           = void(*)(GLenum);
using DisableFn          = void(*)(GLenum);
using DepthFuncFn        = void(*)(GLenum);
using CullFaceFn         = void(*)(GLenum);
using FrontFaceFn        = void(*)(GLenum);
using PolygonModeFn      = void(*)(GLenum, GLenum);
using BlendFuncFn        = void(*)(GLenum, GLenum);
using LineWidthFn        = void(*)(GLfloat);
using GetStringFn        = const GLubyte*(*)(GLenum);

inline ClearColorFn     ClearColor     = nullptr;
inline ClearFn          Clear          = nullptr;
inline ViewportFn       Viewport       = nullptr;
inline EnableFn         Enable         = nullptr;
inline DisableFn        Disable        = nullptr;
inline DepthFuncFn      DepthFunc      = nullptr;
inline CullFaceFn       CullFace       = nullptr;
inline FrontFaceFn      FrontFace      = nullptr;
inline PolygonModeFn    PolygonMode    = nullptr;
inline BlendFuncFn      BlendFunc      = nullptr;
inline LineWidthFn      LineWidth      = nullptr;
inline GetStringFn      GetString      = nullptr;

// --- Shaders ---
using CreateShaderFn     = GLuint(*)(GLenum);
using ShaderSourceFn     = void(*)(GLuint, GLsizei, const GLchar**, const GLint*);
using CompileShaderFn    = void(*)(GLuint);
using GetShaderivFn      = void(*)(GLuint, GLenum, GLint*);
using GetShaderInfoLogFn = void(*)(GLuint, GLsizei, GLsizei*, GLchar*);
using DeleteShaderFn     = void(*)(GLuint);
using CreateProgramFn    = GLuint(*)();
using AttachShaderFn     = void(*)(GLuint, GLuint);
using LinkProgramFn      = void(*)(GLuint);
using GetProgramivFn     = void(*)(GLuint, GLenum, GLint*);
using GetProgramInfoLogFn= void(*)(GLuint, GLsizei, GLsizei*, GLchar*);
using UseProgramFn       = void(*)(GLuint);
using DeleteProgramFn    = void(*)(GLuint);

inline CreateShaderFn      CreateShader      = nullptr;
inline ShaderSourceFn      ShaderSource      = nullptr;
inline CompileShaderFn     CompileShader     = nullptr;
inline GetShaderivFn       GetShaderiv       = nullptr;
inline GetShaderInfoLogFn  GetShaderInfoLog  = nullptr;
inline DeleteShaderFn      DeleteShader      = nullptr;
inline CreateProgramFn     CreateProgram     = nullptr;
inline AttachShaderFn      AttachShader      = nullptr;
inline LinkProgramFn       LinkProgram       = nullptr;
inline GetProgramivFn      GetProgramiv      = nullptr;
inline GetProgramInfoLogFn GetProgramInfoLog = nullptr;
inline UseProgramFn        UseProgram        = nullptr;
inline DeleteProgramFn     DeleteProgram     = nullptr;

// --- Uniforms ---
using GetUniformLocationFn = GLint(*)(GLuint, const GLchar*);
using UniformMatrix4fvFn   = void(*)(GLint, GLsizei, GLboolean, const GLfloat*);
using Uniform3fvFn         = void(*)(GLint, GLsizei, const GLfloat*);
using Uniform3fFn          = void(*)(GLint, GLfloat, GLfloat, GLfloat);
using Uniform1fFn          = void(*)(GLint, GLfloat);
using Uniform1iFn          = void(*)(GLint, GLint);

inline GetUniformLocationFn GetUniformLocation = nullptr;
inline UniformMatrix4fvFn   UniformMatrix4fv   = nullptr;
inline Uniform3fvFn         Uniform3fv         = nullptr;
inline Uniform3fFn          Uniform3f          = nullptr;
inline Uniform1fFn          Uniform1f          = nullptr;
inline Uniform1iFn          Uniform1i          = nullptr;

// --- Buffers ---
using GenBuffersFn    = void(*)(GLsizei, GLuint*);
using BindBufferFn    = void(*)(GLenum, GLuint);
using BufferDataFn    = void(*)(GLenum, GLsizeiptr, const void*, GLenum);
using DeleteBuffersFn = void(*)(GLsizei, const GLuint*);

inline GenBuffersFn    GenBuffers    = nullptr;
inline BindBufferFn    BindBuffer    = nullptr;
inline BufferDataFn    BufferData    = nullptr;
inline DeleteBuffersFn DeleteBuffers = nullptr;

// --- Vertex Arrays ---
using GenVertexArraysFn    = void(*)(GLsizei, GLuint*);
using BindVertexArrayFn    = void(*)(GLuint);
using DeleteVertexArraysFn = void(*)(GLsizei, const GLuint*);

inline GenVertexArraysFn    GenVertexArrays    = nullptr;
inline BindVertexArrayFn    BindVertexArray    = nullptr;
inline DeleteVertexArraysFn DeleteVertexArrays = nullptr;

// --- Vertex Attribs ---
using VertexAttribPointerFn    = void(*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using EnableVertexAttribArrayFn = void(*)(GLuint);

inline VertexAttribPointerFn     VertexAttribPointer     = nullptr;
inline EnableVertexAttribArrayFn EnableVertexAttribArray  = nullptr;

// --- Textures ---
using GenTexturesFn      = void(*)(GLsizei, GLuint*);
using BindTextureFn      = void(*)(GLenum, GLuint);
using DeleteTexturesFn   = void(*)(GLsizei, const GLuint*);
using TexImage2DFn       = void(*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using TexParameteriFn    = void(*)(GLenum, GLenum, GLint);
using ActiveTextureFn    = void(*)(GLenum);

inline GenTexturesFn    GenTextures    = nullptr;
inline BindTextureFn    BindTexture    = nullptr;
inline DeleteTexturesFn DeleteTextures = nullptr;
inline TexImage2DFn     TexImage2D     = nullptr;
inline TexParameteriFn  TexParameteri  = nullptr;
inline ActiveTextureFn  ActiveTexture  = nullptr;

// --- Draw ---
using DrawElementsFn = void(*)(GLenum, GLsizei, GLenum, const void*);
using DrawArraysFn   = void(*)(GLenum, GLint, GLsizei);

inline DrawElementsFn DrawElements = nullptr;
inline DrawArraysFn   DrawArrays   = nullptr;

// ═══════════════════════════════════════════════════════════════
//  LOADER
// ═══════════════════════════════════════════════════════════════

/// Call AFTER glfwMakeContextCurrent(). Returns false if critical functions failed.
inline bool load() {
    auto get = [](const char* name) { return glfwGetProcAddress(name); };

    // Basic state
    ClearColor  = (ClearColorFn)  get("glClearColor");
    Clear       = (ClearFn)       get("glClear");
    Viewport    = (ViewportFn)    get("glViewport");
    Enable      = (EnableFn)      get("glEnable");
    Disable     = (DisableFn)     get("glDisable");
    DepthFunc   = (DepthFuncFn)   get("glDepthFunc");
    CullFace    = (CullFaceFn)    get("glCullFace");
    FrontFace   = (FrontFaceFn)   get("glFrontFace");
    PolygonMode = (PolygonModeFn) get("glPolygonMode");
    BlendFunc   = (BlendFuncFn)   get("glBlendFunc");
    LineWidth   = (LineWidthFn)   get("glLineWidth");
    GetString   = (GetStringFn)   get("glGetString");

    // Shaders
    CreateShader      = (CreateShaderFn)      get("glCreateShader");
    ShaderSource      = (ShaderSourceFn)      get("glShaderSource");
    CompileShader     = (CompileShaderFn)     get("glCompileShader");
    GetShaderiv       = (GetShaderivFn)       get("glGetShaderiv");
    GetShaderInfoLog  = (GetShaderInfoLogFn)  get("glGetShaderInfoLog");
    DeleteShader      = (DeleteShaderFn)      get("glDeleteShader");
    CreateProgram     = (CreateProgramFn)     get("glCreateProgram");
    AttachShader      = (AttachShaderFn)      get("glAttachShader");
    LinkProgram       = (LinkProgramFn)       get("glLinkProgram");
    GetProgramiv      = (GetProgramivFn)      get("glGetProgramiv");
    GetProgramInfoLog = (GetProgramInfoLogFn) get("glGetProgramInfoLog");
    UseProgram        = (UseProgramFn)        get("glUseProgram");
    DeleteProgram     = (DeleteProgramFn)     get("glDeleteProgram");

    // Uniforms
    GetUniformLocation = (GetUniformLocationFn) get("glGetUniformLocation");
    UniformMatrix4fv   = (UniformMatrix4fvFn)   get("glUniformMatrix4fv");
    Uniform3fv         = (Uniform3fvFn)         get("glUniform3fv");
    Uniform3f          = (Uniform3fFn)          get("glUniform3f");
    Uniform1f          = (Uniform1fFn)          get("glUniform1f");
    Uniform1i          = (Uniform1iFn)          get("glUniform1i");

    // Buffers
    GenBuffers    = (GenBuffersFn)    get("glGenBuffers");
    BindBuffer    = (BindBufferFn)    get("glBindBuffer");
    BufferData    = (BufferDataFn)    get("glBufferData");
    DeleteBuffers = (DeleteBuffersFn) get("glDeleteBuffers");

    // VAOs
    GenVertexArrays    = (GenVertexArraysFn)    get("glGenVertexArrays");
    BindVertexArray    = (BindVertexArrayFn)    get("glBindVertexArray");
    DeleteVertexArrays = (DeleteVertexArraysFn) get("glDeleteVertexArrays");

    // Attribs
    VertexAttribPointer    = (VertexAttribPointerFn)    get("glVertexAttribPointer");
    EnableVertexAttribArray = (EnableVertexAttribArrayFn) get("glEnableVertexAttribArray");

    // Textures
    GenTextures    = (GenTexturesFn)    get("glGenTextures");
    BindTexture    = (BindTextureFn)    get("glBindTexture");
    DeleteTextures = (DeleteTexturesFn) get("glDeleteTextures");
    TexImage2D     = (TexImage2DFn)     get("glTexImage2D");
    TexParameteri  = (TexParameteriFn)  get("glTexParameteri");
    ActiveTexture  = (ActiveTextureFn)  get("glActiveTexture");

    // Draw
    DrawElements = (DrawElementsFn) get("glDrawElements");
    DrawArrays   = (DrawArraysFn)   get("glDrawArrays");

    // Verify critical functions loaded
    return CreateShader && CreateProgram && GenBuffers &&
           GenVertexArrays && DrawElements && Clear;
}

} // namespace gl
