#ifndef GLAD_H_
#define GLAD_H_

#include <cstddef>
#include <KHR/khrplatform.h>

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*GLADloadproc)(const char *name);

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_STREAM_DRAW 0x88E0
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_INT 0x1405
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_GENERATE_MIPMAP 0x8191
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_MAX_TEXTURE_SIZE 0x0D33

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef unsigned char GLboolean;
typedef double GLdouble;
typedef unsigned long GLulong;
typedef unsigned short GLushort;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

extern GLenum glGetError(void);
extern const GLubyte* glGetString(GLenum name);
extern void glGetIntegerv(GLenum pname, GLint* data);
extern void glGenVertexArrays(GLsizei n, GLuint* arrays);
extern void glBindVertexArray(GLuint array);
extern void glGenBuffers(GLsizei n, GLuint* buffers);
extern void glBindBuffer(GLenum target, GLuint buffer);
extern void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
extern void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
extern void glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
extern void glDeleteBuffers(GLsizei n, const GLuint* buffers);
extern void glEnableVertexAttribArray(GLuint index);
extern void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
extern void glDisableVertexAttribArray(GLuint index);
extern void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
extern void glDrawArrays(GLenum mode, GLint first, GLsizei count);
extern void glEnable(GLenum cap);
extern void glDisable(GLenum cap);
extern void glBlendFunc(GLenum sfactor, GLenum dfactor);
extern void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
extern void glClear(GLbitfield mask);
extern void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void glDepthMask(GLboolean flag);
extern void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
extern GLuint glCreateShader(GLenum type);
extern void glShaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
extern void glCompileShader(GLuint shader);
extern void glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
extern void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern GLuint glCreateProgram(void);
extern void glAttachShader(GLuint program, GLuint shader);
extern void glLinkProgram(GLuint program);
extern void glGetProgramiv(GLuint program, GLenum pname, GLint* params);
extern void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void glUseProgram(GLuint program);
extern void glDeleteShader(GLuint shader);
extern void glDeleteProgram(GLuint program);
extern GLint glGetUniformLocation(GLuint program, const GLchar* name);
extern void glUniform1i(GLint location, GLint v0);
extern void glUniform1f(GLint location, GLfloat v0);
extern void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
extern void glGenTextures(GLsizei n, GLuint* textures);
extern void glBindTexture(GLenum target, GLuint texture);
extern void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
extern void glTexParameteri(GLenum target, GLenum pname, GLint param);
extern void glGenerateMipmap(GLenum target);
extern void glActiveTexture(GLenum texture);
extern void glGenFramebuffers(GLsizei n, GLuint* framebuffers);
extern void glBindFramebuffer(GLenum target, GLuint framebuffer);
extern void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers);
extern void glBindRenderbuffer(GLenum target, GLuint renderbuffer);
extern void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
extern GLenum glCheckFramebufferStatus(GLenum target);
extern void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
extern void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
extern void glDeleteTextures(GLsizei n, const GLuint* textures);
extern void glDrawBuffers(GLsizei n, const GLenum* bufs);

extern int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif
