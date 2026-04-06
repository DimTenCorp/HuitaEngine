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
    static void setView(const glm::mat4& view);
    static void setProjection(const glm::mat4& projection);
    static void setViewPos(const glm::vec3& pos);
    static void setColor(const glm::vec3& color);

    // Locations
    static int getMVPLoc() { return mvpLoc; }
    static int getModelLoc() { return modelLoc; }

private:
    static unsigned int program;
    static unsigned int shadowProgram;
    static int mvpLoc, modelLoc, viewLoc, projectionLoc;
    static int viewPosLoc, colorLoc;

    static const char* vertexSource;
    static const char* fragmentSource;
    static const char* shadowVertexSource;
    static const char* shadowFragmentSource;

    static unsigned int compileShader(GLenum type, const char* source);
};