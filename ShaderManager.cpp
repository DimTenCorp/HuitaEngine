#include "ShaderManager.h"
#include "gl_includes.h"  // ← Добавляем наш хедер с правильным порядком
#include <iostream>

unsigned int ShaderManager::program = 0;
int ShaderManager::mvpLoc = -1, ShaderManager::modelLoc = -1;
int ShaderManager::lightDirLoc = -1, ShaderManager::viewPosLoc = -1, ShaderManager::colorLoc = -1;

const char* ShaderManager::vertexSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoord;
    uniform mat4 mvp;
    uniform mat4 model;
    out vec3 vNormal;
    out vec3 vPos;
    out vec2 vTexCoord;
    void main() {
        vPos = (model * vec4(aPos, 1.0)).xyz;
        vNormal = mat3(transpose(inverse(model))) * aNormal;
        vTexCoord = aTexCoord;
        gl_Position = mvp * vec4(aPos, 1.0);
    }
)";

const char* ShaderManager::fragmentSource = R"(
    #version 330 core
    in vec3 vNormal;
    in vec3 vPos;
    in vec2 vTexCoord;
    out vec4 FragColor;
    uniform sampler2D uTexture;
    uniform vec3 color;
    uniform vec3 lightDir;
    uniform vec3 viewPos;
    void main() {
        vec4 texColor = texture(uTexture, vTexCoord);
        if (texColor.a < 0.5) discard;
        vec3 norm = normalize(vNormal);
        float diff = max(dot(norm, -lightDir), 0.0);
        vec3 ambient = vec3(0.3);
        vec3 diffuse = texColor.rgb * diff * 0.7;
        float dist = length(viewPos - vPos);
        float fog = exp(-dist * 0.02);
        fog = clamp(fog, 0.0, 1.0);
        vec3 fogColor = vec3(0.1, 0.15, 0.2);
        vec3 result = mix(fogColor, texColor.rgb * (ambient + diffuse), fog);
        FragColor = vec4(result, texColor.a);
    }
)";

bool ShaderManager::initialize() {
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
        std::cerr << "Shader linking failed: " << infoLog << "\n";
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Cache locations
    mvpLoc = glGetUniformLocation(program, "mvp");
    modelLoc = glGetUniformLocation(program, "model");
    lightDirLoc = glGetUniformLocation(program, "lightDir");
    viewPosLoc = glGetUniformLocation(program, "viewPos");
    colorLoc = glGetUniformLocation(program, "color");

    // Default light
    glUniform3f(lightDirLoc, 0.3f, -0.7f, 0.5f);

    return true;
}

void ShaderManager::cleanup() {
    if (program) {
        glDeleteProgram(program);
        program = 0;
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

void ShaderManager::setLightDir(const glm::vec3& dir) {
    glUniform3f(lightDirLoc, dir.x, dir.y, dir.z);
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