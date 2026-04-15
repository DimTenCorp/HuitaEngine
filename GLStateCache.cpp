#include "GLStateCache.h"
#include <glad/glad.h>

// Статические переменные
bool GLStateCache::depthTestEnabled = false;
bool GLStateCache::depthMaskEnabled = true;
bool GLStateCache::blendingEnabled = false;
bool GLStateCache::cullFaceEnabled = true;
GLenum GLStateCache::cullMode = GL_BACK;
GLenum GLStateCache::depthFunc = GL_LESS;
bool GLStateCache::initialized = false;

void GLStateCache::init() {
    if (initialized) return;
    
    // Получаем начальное состояние OpenGL
    GLboolean tempBool;
    glGetBooleanv(GL_DEPTH_TEST, &tempBool); depthTestEnabled = (tempBool != 0);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &tempBool); depthMaskEnabled = (tempBool != 0);
    glGetBooleanv(GL_BLEND, &tempBool); blendingEnabled = (tempBool != 0);
    glGetBooleanv(GL_CULL_FACE, &tempBool); cullFaceEnabled = (tempBool != 0);
    glGetIntegerv(GL_CULL_FACE_MODE, (GLint*)&cullMode);
    glGetIntegerv(GL_DEPTH_FUNC, (GLint*)&depthFunc);
    
    initialized = true;
}

void GLStateCache::enableDepthTest(bool enable) {
    if (depthTestEnabled == enable) return;
    depthTestEnabled = enable;
    if (enable) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

bool GLStateCache::isDepthTestEnabled() {
    return depthTestEnabled;
}

void GLStateCache::setDepthMask(bool enable) {
    if (depthMaskEnabled == enable) return;
    depthMaskEnabled = enable;
    glDepthMask(enable ? GL_TRUE : GL_FALSE);
}

bool GLStateCache::isDepthMaskEnabled() {
    return depthMaskEnabled;
}

void GLStateCache::enableBlending(bool enable) {
    if (blendingEnabled == enable) return;
    blendingEnabled = enable;
    if (enable) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}

bool GLStateCache::isBlendingEnabled() {
    return blendingEnabled;
}

void GLStateCache::enableCullFace(bool enable) {
    if (cullFaceEnabled == enable) return;
    cullFaceEnabled = enable;
    if (enable) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
}

void GLStateCache::setCullMode(GLenum mode) {
    if (cullFaceEnabled && cullMode == mode) return;
    cullMode = mode;
    glCullFace(mode);
}

void GLStateCache::setDepthFunc(GLenum func) {
    if (depthFunc == func) return;
    depthFunc = func;
    glDepthFunc(func);
}
