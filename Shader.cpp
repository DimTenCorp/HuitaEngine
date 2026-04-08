#include "Shader.h"
#include <iostream>
#include <cstring>

Shader::Shader() : programID(0), lastError("") {}

Shader::~Shader() {
    if (programID != 0) {
        glDeleteProgram(programID);
        programID = 0;
    }
}

Shader::Shader(Shader&& other) noexcept
    : programID(other.programID), lastError(std::move(other.lastError)) {
    other.programID = 0;
    other.uniformCache.clear();
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (programID != 0) {
            glDeleteProgram(programID);
        }
        programID = other.programID;
        lastError = std::move(other.lastError);
        uniformCache = std::move(other.uniformCache);
        other.programID = 0;
    }
    return *this;
}

unsigned int Shader::compileShader(GLenum type, const std::string& source, std::string& errorLog) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        errorLog = log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::compile(const std::string& vertexSource, const std::string& fragmentSource) {
    if (programID != 0) {
        glDeleteProgram(programID);
        programID = 0;
        uniformCache.clear();
    }
    lastError.clear();

    if (vertexSource.empty() || fragmentSource.empty()) {
        lastError = "Empty shader source provided";
        std::cerr << "Shader error: " << lastError << std::endl;
        return false;
    }

    std::string vertError, fragError;
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, vertError);
    if (vertexShader == 0) {
        lastError = "Vertex shader compilation failed:\n" + vertError;
        std::cerr << "Vertex shader compilation failed!\n" << vertError << std::endl;
        return false;
    }

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragError);
    if (fragmentShader == 0) {
        lastError = "Fragment shader compilation failed:\n" + fragError;
        std::cerr << "Fragment shader compilation failed!\n" << fragError << std::endl;
        glDeleteShader(vertexShader);
        return false;
    }

    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    GLint linkSuccess = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess) {
        char log[2048];
        glGetProgramInfoLog(programID, sizeof(log), nullptr, log);
        lastError = "Shader program linking failed:\n";
        lastError += log;
        std::cerr << "Shader linking failed!\n" << log << std::endl;
        glDeleteProgram(programID);
        programID = 0;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glDetachShader(programID, vertexShader);
    glDetachShader(programID, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    std::cout << "Shader compiled and linked successfully! ID: " << programID << std::endl;
    return true;
}

// === НОВОЕ: Compile with geometry shader ===
bool Shader::compile(const std::string& vertexSource, const std::string& geometrySource,
    const std::string& fragmentSource) {
    if (programID != 0) {
        glDeleteProgram(programID);
        programID = 0;
        uniformCache.clear();
    }
    lastError.clear();

    if (vertexSource.empty() || geometrySource.empty() || fragmentSource.empty()) {
        lastError = "Empty shader source provided";
        return false;
    }

    std::string vertError, geomError, fragError;

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, vertError);
    if (vertexShader == 0) {
        lastError = "Vertex shader compilation failed:\n" + vertError;
        return false;
    }

    GLuint geometryShader = compileShader(GL_GEOMETRY_SHADER, geometrySource, geomError);
    if (geometryShader == 0) {
        lastError = "Geometry shader compilation failed:\n" + geomError;
        glDeleteShader(vertexShader);
        return false;
    }

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragError);
    if (fragmentShader == 0) {
        lastError = "Fragment shader compilation failed:\n" + fragError;
        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        return false;
    }

    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, geometryShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    GLint linkSuccess = 0;
    glGetProgramiv(programID, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess) {
        char log[2048];
        glGetProgramInfoLog(programID, sizeof(log), nullptr, log);
        lastError = "Shader program linking failed:\n";
        lastError += log;
        glDeleteProgram(programID);
        programID = 0;
        glDeleteShader(vertexShader);
        glDeleteShader(geometryShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glDetachShader(programID, vertexShader);
    glDetachShader(programID, geometryShader);
    glDetachShader(programID, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(geometryShader);
    glDeleteShader(fragmentShader);

    std::cout << "Shader with geometry shader compiled! ID: " << programID << std::endl;
    return true;
}

int Shader::getUniformLocation(const std::string& name) const {
    if (programID == 0) return -1;

    auto it = uniformCache.find(name);
    if (it != uniformCache.end()) {
        return it->second;
    }

    int location = glGetUniformLocation(programID, name.c_str());
    uniformCache[name] = location;
    return location;
}

void Shader::clearCache() {
    uniformCache.clear();
}

void Shader::setBool(const std::string& name, bool value) const {
    int loc = getUniformLocation(name);
    if (loc >= 0) glUniform1i(loc, value ? 1 : 0);
}

void Shader::setInt(const std::string& name, int value) const {
    int loc = getUniformLocation(name);
    if (loc >= 0) glUniform1i(loc, value);
}

void Shader::setUInt(const std::string& name, unsigned int value) const {
    int loc = getUniformLocation(name);
    if (loc >= 0) glUniform1ui(loc, value);
}

void Shader::setFloat(const std::string& name, float value) const {
    int loc = getUniformLocation(name);
    if (loc >= 0) glUniform1f(loc, value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    int loc = getUniformLocation(name);
    if (loc >= 0) glUniform3fv(loc, 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    int loc = getUniformLocation(name);
    if (loc >= 0) glUniform4fv(loc, 1, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    int loc = getUniformLocation(name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::bind() const {
    if (programID != 0) {
        glUseProgram(programID);
    }
}

void Shader::unbind() {
    glUseProgram(0);
}