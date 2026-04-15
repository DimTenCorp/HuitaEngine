#pragma once

#include <GL/glew.h> // Или ваш заголовок OpenGL

class GLStateCache {
public:
    static void init();
    
    // Depth Test
    static void enableDepthTest(bool enable);
    static bool isDepthTestEnabled();

    // Depth Mask
    static void setDepthMask(bool enable);
    static bool isDepthMaskEnabled();

    // Blending
    static void enableBlending(bool enable);
    static bool isBlendingEnabled();

    // Cull Face
    static void enableCullFace(bool enable);
    static void setCullMode(GLenum mode);

    // Depth Func
    static void setDepthFunc(GLenum func);

private:
    static bool depthTestEnabled;
    static bool depthMaskEnabled;
    static bool blendingEnabled;
    static bool cullFaceEnabled;
    static GLenum cullMode;
    static GLenum depthFunc;
    static bool initialized;
};
