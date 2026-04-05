#pragma once

#include "gl_includes.h"

class ShaderManager {
public:
    static bool initialize();
    static void cleanup();

    static void use();

    // Uniform setters
    static void setMVP(const glm::mat4& mvp);
    static void setModel(const glm::mat4& model);
    static void setLightDir(const glm::vec3& dir);
    static void setViewPos(const glm::vec3& pos);
    static void setColor(const glm::vec3& color);

    // Locations
    static int getMVPLoc() { return mvpLoc; }
    static int getModelLoc() { return modelLoc; }

private:
    static unsigned int program;
    static int mvpLoc, modelLoc, lightDirLoc, viewPosLoc, colorLoc;

    static const char* vertexSource;
    static const char* fragmentSource;

    static unsigned int compileShader(GLenum type, const char* source);
};