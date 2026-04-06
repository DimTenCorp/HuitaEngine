#include "ShaderManager.h"
#include "gl_includes.h"
#include <iostream>

unsigned int ShaderManager::program = 0;
unsigned int ShaderManager::shadowProgram = 0;
int ShaderManager::mvpLoc = -1, ShaderManager::modelLoc = -1;
int ShaderManager::viewLoc = -1, ShaderManager::projectionLoc = -1;
int ShaderManager::viewPosLoc = -1, ShaderManager::colorLoc = -1;

const char* ShaderManager::vertexSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoord;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    out vec3 vNormal;
    out vec3 vPos;
    out vec2 vTexCoord;
    
    void main() {
        vec4 worldPos = model * vec4(aPos, 1.0);
        vPos = worldPos.xyz;
        vNormal = mat3(transpose(inverse(model))) * aNormal;
        vTexCoord = aTexCoord;
        
        gl_Position = projection * view * worldPos;
    }
)";

const char* ShaderManager::fragmentSource = R"(
    #version 330 core
    in vec3 vNormal;
    in vec3 vPos;
    in vec2 vTexCoord;
    
    out vec4 FragColor;
    
    uniform sampler2D uTexture;
    uniform vec3 viewPos;
    
    void main() {
        vec4 texColor = texture(uTexture, vTexCoord);
        if(texColor.a < 0.5) discard;
        
        FragColor = vec4(texColor.rgb, 1.0);
    }
)";

const char* ShaderManager::shadowVertexSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    
    uniform mat4 shadowMVP;
    
    void main() {
        gl_Position = shadowMVP * vec4(aPos, 1.0);
    }
)";

const char* ShaderManager::shadowFragmentSource = R"(
    #version 330 core
    void main() {
        // Empty - depth only
    }
)";

bool ShaderManager::initialize() {
    // Main rendering shader
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    if (!vs || !fs) return false;

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Main shader linking failed: " << infoLog << "\n";
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Shadow shader
    unsigned int shadowVS = compileShader(GL_VERTEX_SHADER, shadowVertexSource);
    unsigned int shadowFS = compileShader(GL_FRAGMENT_SHADER, shadowFragmentSource);
    
    if (!shadowVS || !shadowFS) return false;
    
    shadowProgram = glCreateProgram();
    glAttachShader(shadowProgram, shadowVS);
    glAttachShader(shadowProgram, shadowFS);
    glLinkProgram(shadowProgram);
    
    glGetProgramiv(shadowProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shadowProgram, 512, nullptr, infoLog);
        std::cerr << "Shadow shader linking failed: " << infoLog << "\n";
        return false;
    }
    
    glDeleteShader(shadowVS);
    glDeleteShader(shadowFS);

    // Cache locations for main shader
    mvpLoc = glGetUniformLocation(program, "mvp");
    modelLoc = glGetUniformLocation(program, "model");
    viewLoc = glGetUniformLocation(program, "view");
    projectionLoc = glGetUniformLocation(program, "projection");
    viewPosLoc = glGetUniformLocation(program, "viewPos");
    colorLoc = glGetUniformLocation(program, "color");

    // Default values
    use();
    
    return true;
}

void ShaderManager::cleanup() {
    if (program) {
        glDeleteProgram(program);
        program = 0;
    }
    if (shadowProgram) {
        glDeleteProgram(shadowProgram);
        shadowProgram = 0;
    }
}

void ShaderManager::use() {
    glUseProgram(program);
}

void ShaderManager::setMVP(const glm::mat4& mvp) {
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
}

void ShaderManager::setModel(const glm::mat4& model) {
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
}

void ShaderManager::setView(const glm::mat4& view) {
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void ShaderManager::setProjection(const glm::mat4& projection) {
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void ShaderManager::setViewPos(const glm::vec3& pos) {
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(pos));
}

void ShaderManager::setColor(const glm::vec3& c) {
    glUniform3f(colorLoc, c.x, c.y, c.z);
}

unsigned int ShaderManager::compileShader(GLenum type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed (" << type << "): " << infoLog << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}