#include <glad/glad.h>

// Stub implementations for Glad functions
GLenum glGetError(void) { return 0; }
const GLubyte* glGetString(GLenum name) { return (const GLubyte*)""; }
void glGetIntegerv(GLenum pname, GLint* data) { if(data) *data = 0; }
void glGenVertexArrays(GLsizei n, GLuint* arrays) { for(int i=0;i<n;i++) arrays[i]=1; }
void glBindVertexArray(GLuint array) {}
void glGenBuffers(GLsizei n, GLuint* buffers) { for(int i=0;i<n;i++) buffers[i]=1; }
void glBindBuffer(GLenum target, GLuint buffer) {}
void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {}
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {}
void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {}
void glDeleteBuffers(GLsizei n, const GLuint* buffers) {}
void glEnableVertexAttribArray(GLuint index) {}
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {}
void glDisableVertexAttribArray(GLuint index) {}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {}
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {}
void glEnable(GLenum cap) {}
void glDisable(GLenum cap) {}
void glBlendFunc(GLenum sfactor, GLenum dfactor) {}
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {}
void glClear(GLbitfield mask) {}
void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {}
void glDepthMask(GLboolean flag) {}
void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {}
GLuint glCreateShader(GLenum type) { return 1; }
void glShaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length) {}
void glCompileShader(GLuint shader) {}
void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) { if(params) *params = 1; }
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {}
GLuint glCreateProgram(void) { return 1; }
void glAttachShader(GLuint program, GLuint shader) {}
void glLinkProgram(GLuint program) {}
void glGetProgramiv(GLuint program, GLenum pname, GLint* params) { if(params) *params = 1; }
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {}
void glUseProgram(GLuint program) {}
void glDeleteShader(GLuint shader) {}
void glDeleteProgram(GLuint program) {}
GLint glGetUniformLocation(GLuint program, const GLchar* name) { return 0; }
void glUniform1i(GLint location, GLint v0) {}
void glUniform1f(GLint location, GLfloat v0) {}
void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {}
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {}
void glGenTextures(GLsizei n, GLuint* textures) { for(int i=0;i<n;i++) textures[i]=1; }
void glBindTexture(GLenum target, GLuint texture) {}
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {}
void glTexParameteri(GLenum target, GLenum pname, GLint param) {}
void glGenerateMipmap(GLenum target) {}
void glActiveTexture(GLenum texture) {}
void glGenFramebuffers(GLsizei n, GLuint* framebuffers) { for(int i=0;i<n;i++) framebuffers[i]=1; }
void glBindFramebuffer(GLenum target, GLuint framebuffer) {}
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {}
void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) { for(int i=0;i<n;i++) renderbuffers[i]=1; }
void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {}
void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {}
void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {}
GLenum glCheckFramebufferStatus(GLenum target) { return 0x8CD5; }
void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {}
void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {}
void glDeleteTextures(GLsizei n, const GLuint* textures) {}
void glDrawBuffers(GLsizei n, const GLenum* bufs) {}
void glGetBooleanv(GLenum pname, GLboolean* params) { if(params) *params = 0; }
void glCullFace(GLenum mode) {}
void glDepthFunc(GLenum func) {}
int gladLoadGLLoader(GLADloadproc load) { return 1; }
