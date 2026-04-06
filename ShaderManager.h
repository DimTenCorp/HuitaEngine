#pragma once

#include "gl_includes.h"

struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    float radius;
};

class ShaderManager {
public:
    static bool initialize();
    static void cleanup();

    static void use();

    // Uniform setters
    static void setMVP(const glm::mat4& mvp);
    static void setModel(const glm::mat4& model);
    static void setView(const glm::mat4& view);
    static void setProjection(const glm::mat4& projection);
    static void setLightDir(const glm::vec3& dir);
    static void setViewPos(const glm::vec3& pos);
    static void setColor(const glm::vec3& color);
    
    // Point Light
    static void setPointLight(const PointLight& light);
    static void setSunLightSpaceMatrix(const glm::mat4& matrix);
    static void enableShadows(bool enabled);
    static void bindShadowMap(GLuint textureID);

    // Shadow pass
    static void useShadowShader();
    static void setShadowMVP(const glm::mat4& mvp);

    // Locations
    static int getMVPLoc() { return mvpLoc; }
    static int getModelLoc() { return modelLoc; }

private:
    static unsigned int program;
    static unsigned int shadowProgram;
    static int mvpLoc, modelLoc, viewLoc, projectionLoc;
    static int lightDirLoc, viewPosLoc, colorLoc;
    
    // Point light uniforms
    static int pointLightPosLoc, pointLightColorLoc, pointLightIntensityLoc, pointLightRadiusLoc;
    static int sunLightSpaceMatrixLoc, shadowsEnabledLoc;
    static int shadowMapLoc;
    
    // Shadow shader locations
    static int shadowMVPLoc;

    static const char* vertexSource;
    static const char* fragmentSource;
    static const char* shadowVertexSource;
    static const char* shadowFragmentSource;

    static unsigned int compileShader(GLenum type, const char* source);
};