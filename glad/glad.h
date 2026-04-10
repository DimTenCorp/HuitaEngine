#ifndef GLAD_H_
#define GLAD_H_
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GLAD_GLAPI_H_IMPLEMENTATION
typedef void* (*GLADloadproc)(const char *name);
#ifndef GLAPI
#define GLAPI extern
#endif
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef size_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef unsigned char GLboolean;
typedef double GLdouble;
typedef unsigned char GLubyte;
typedef char GLchar;
typedef short GLshort;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NONE 0
#define GL_NO_ERROR 0
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_EXTENSIONS 0x1F03
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_CCW 0x0901
#define GL_CW 0x0900
#define GL_FILL 0x1B02
#define GL_LINE 0x1B01
#define GL_FRONT_AND_BACK 0x0408
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_INT 0x1405
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_FAN 0x0006
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_RGBA16F 0x881A
#define GL_FLOAT 0x1406
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
GLAPI int gladLoadGLLoader(GLADloadproc load);
GLAPI void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
GLAPI void glClearDepth(GLdouble d);
GLAPI void glClear(GLbitfield mask);
GLAPI void glEnable(GLenum cap);
GLAPI void glDisable(GLenum cap);
GLAPI void glCullFace(GLenum mode);
GLAPI void glFrontFace(GLenum mode);
GLAPI void glPolygonMode(GLenum face, GLenum mode);
GLAPI void glBlendFunc(GLenum sfactor, GLenum dfactor);
GLAPI void glDepthFunc(GLenum func);
GLAPI void glDepthMask(GLboolean flag);
GLAPI void glGenVertexArrays(GLsizei n, GLuint *arrays);
GLAPI void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
GLAPI void glGenBuffers(GLsizei n, GLuint *buffers);
GLAPI void glDeleteBuffers(GLsizei n, const GLuint *buffers);
GLAPI void glBindVertexArray(GLuint array);
GLAPI void glBindBuffer(GLenum target, GLuint buffer);
GLAPI void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
GLAPI void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
GLAPI void glEnableVertexAttribArray(GLuint index);
GLAPI void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
GLAPI void glDrawArrays(GLenum mode, GLint first, GLsizei count);
GLAPI void glGenTextures(GLsizei n, GLuint *textures);
GLAPI void glDeleteTextures(GLsizei n, const GLuint *textures);
GLAPI void glBindTexture(GLenum target, GLuint texture);
GLAPI void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
GLAPI void glTexParameteri(GLenum target, GLenum pname, GLint param);
GLAPI void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLAPI void glGenFramebuffers(GLsizei n, GLuint *framebuffers);
GLAPI void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
GLAPI void glBindFramebuffer(GLenum target, GLuint framebuffer);
GLAPI GLenum glCheckFramebufferStatus(GLenum target);
GLAPI void glActiveTexture(GLenum texture);
GLAPI const GLubyte* glGetString(GLenum name);
GLAPI GLenum glGetError(void);
GLAPI void glGetIntegerv(GLenum pname, GLint *data);
GLAPI GLuint glCreateShader(GLenum type);
GLAPI void glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
GLAPI void glCompileShader(GLuint shader);
GLAPI void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
GLAPI void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLAPI GLuint glCreateProgram(void);
GLAPI void glAttachShader(GLuint program, GLuint shader);
GLAPI void glLinkProgram(GLuint program);
GLAPI void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
GLAPI void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLAPI void glUseProgram(GLuint program);
GLAPI void glDeleteShader(GLuint shader);
GLAPI void glDeleteProgram(GLuint program);
GLAPI GLint glGetUniformLocation(GLuint program, const GLchar *name);
GLAPI void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
GLAPI void glUniform1i(GLint location, GLint v0);
GLAPI void glUniform1f(GLint location, GLfloat v0);
GLAPI void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
GLAPI void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
GLAPI void glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
#ifdef __cplusplus
}
#endif
#endif
