#include "Shader.h"
#include <iostream>
#include <fstream>
#include <sstream>

Shader::Shader() = default;

Shader::~Shader() {
    unload();
}

bool Shader::loadFromFiles(const std::string& vPath, const std::string& fPath) {
    vertPath = vPath;
    fragPath = fPath;

    std::string vSource, fSource;
    if (!readFile(vPath, vSource)) {
        lastError = "Failed to read vertex shader: " + vPath;
        return false;
    }
    if (!readFile(fPath, fSource)) {
        lastError = "Failed to read fragment shader: " + fPath;
        return false;
    }

    vertSourceCached = vSource;
    fragSourceCached = fSource;

    return loadFromStrings(vSource, fSource);
}

bool Shader::loadFromStrings(const std::string& vSource, const std::string& fSource) {
    unload();

    GLuint vs = compile(GL_VERTEX_SHADER, vSource);
    if (!vs) return false;

    GLuint fs = compile(GL_FRAGMENT_SHADER, fSource);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    if (!link(vs, fs)) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    uniformCache.clear();
    return true;
}

void Shader::unload() {
    if (programID) {
        glDeleteProgram(programID);
        programID = 0;
    }
    uniformCache.clear();
}

void Shader::bind() const {
    glUseProgram(programID);
}

void Shader::unbind() const {
    glUseProgram(0);
}

void Shader::setBool(const std::string& name, bool value) {
    glUniform1i(getUniformLocation(name), value ? 1 : 0);
}

void Shader::setInt(const std::string& name, int value) {
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) {
    glUniform1f(getUniformLocation(name), value);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) {
    glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec2(const std::string& name, float x, float y) {
    glUniform2f(getUniformLocation(name), x, y);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) {
    glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, float x, float y, float z) {
    glUniform3f(getUniformLocation(name), x, y, z);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) {
    glUniform4fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setMat3(const std::string& name, const glm::mat3& value) {
    glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const float* value) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, value);
}

void Shader::setIntArray(const std::string& name, int* values, int count) {
    glUniform1iv(getUniformLocation(name), count, values);
}

void Shader::setFloatArray(const std::string& name, float* values, int count) {
    glUniform1fv(getUniformLocation(name), count, values);
}

void Shader::setVec3Array(const std::string& name, const std::vector<glm::vec3>& values) {
    glUniform3fv(getUniformLocation(name), (GLsizei)values.size(),
        (const float*)values.data());
}

bool Shader::readFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

GLuint Shader::compile(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);

        std::string typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        lastError = "Failed to compile " + typeStr + " shader:\n" + infoLog;

        std::cerr << lastError << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool Shader::link(GLuint vs, GLuint fs) {
    programID = glCreateProgram();
    glAttachShader(programID, vs);
    glAttachShader(programID, fs);
    glLinkProgram(programID);

    GLint success;
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(programID, 1024, nullptr, infoLog);
        lastError = std::string("Failed to link shader program:\n") + infoLog;
        std::cerr << lastError << std::endl;

        glDeleteProgram(programID);
        programID = 0;
        return false;
    }

    return true;
}

GLint Shader::getUniformLocation(const std::string& name) const {
    auto it = uniformCache.find(name);
    if (it != uniformCache.end()) return it->second;

    GLint loc = glGetUniformLocation(programID, name.c_str());
    uniformCache[name] = loc;

    if (loc == -1) {
        static std::unordered_map<std::string, bool> warned;
        if (!warned[name]) {
            std::cerr << "Warning: uniform '" << name << "' not found in shader\n";
            warned[name] = true;
        }
    }

    return loc;
}

void Shader::printActiveUniforms() const {
    GLint count;
    glGetProgramiv(programID, GL_ACTIVE_UNIFORMS, &count);

    std::cout << "Active uniforms (" << count << "):\n";
    for (int i = 0; i < count; i++) {
        char name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        glGetActiveUniform(programID, i, 256, &length, &size, &type, name);

        std::cout << "  [" << i << "] " << name << "\n";
    }
}