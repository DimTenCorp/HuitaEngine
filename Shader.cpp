#include "Shader.h"
#include <iostream>
#include <fstream>
#include <sstream>

Shader::Shader() : ID(0) {}

Shader::Shader(const char* vertexPath, const char* fragmentPath) : ID(0) {
    std::string vertexCode, fragmentCode;
    std::ifstream vFile, fFile;
    
    vFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    try {
        std::stringstream vBuffer, fBuffer;
        
        vFile.open(vertexPath);
        vBuffer << vFile.rdbuf();
        vFile.close();
        vertexCode = vBuffer.str();
        
        fFile.open(fragmentPath);
        fBuffer << fFile.rdbuf();
        fFile.close();
        fragmentCode = fBuffer.str();
        
        ID = createFromSource(vertexCode.c_str(), fragmentCode.c_str()).ID;
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }
}

Shader::~Shader() {
    if (ID != 0) {
        glDeleteProgram(ID);
    }
}

unsigned int Shader::compileShader(ShaderType type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

bool Shader::checkCompileErrors(unsigned int shader, ShaderType type, std::string& errorLog) {
    int success;
    char infoLog[1024];
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        errorLog = infoLog;
        return false;
    }
    return true;
}

bool Shader::checkLinkErrors(unsigned int program, std::string& errorLog) {
    int success;
    char infoLog[1024];
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        errorLog = infoLog;
        return false;
    }
    return true;
}

Shader Shader::createFromSource(const char* vertexSource, const char* fragmentSource) {
    Shader shader;
    
    unsigned int vertex = shader.compileShader(SHADER_VERTEX, vertexSource);
    unsigned int fragment = shader.compileShader(SHADER_FRAGMENT, fragmentSource);
    
    std::string errorLog;
    if (!shader.checkCompileErrors(vertex, SHADER_VERTEX, errorLog)) {
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << errorLog << std::endl;
        glDeleteShader(vertex);
        return shader;
    }
    
    if (!shader.checkCompileErrors(fragment, SHADER_FRAGMENT, errorLog)) {
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << errorLog << std::endl;
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return shader;
    }
    
    shader.ID = glCreateProgram();
    glAttachShader(shader.ID, vertex);
    glAttachShader(shader.ID, fragment);
    glLinkProgram(shader.ID);
    
    if (!shader.checkLinkErrors(shader.ID, errorLog)) {
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << errorLog << std::endl;
        glDeleteProgram(shader.ID);
        shader.ID = 0;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    
    return shader;
}

Shader Shader::createFromSource(const char* vertexSource, const char* fragmentSource, const char* geometrySource) {
    Shader shader;
    
    unsigned int vertex = shader.compileShader(SHADER_VERTEX, vertexSource);
    unsigned int fragment = shader.compileShader(SHADER_FRAGMENT, fragmentSource);
    unsigned int geometry = shader.compileShader(SHADER_GEOMETRY, geometrySource);
    
    std::string errorLog;
    if (!shader.checkCompileErrors(vertex, SHADER_VERTEX, errorLog) ||
        !shader.checkCompileErrors(fragment, SHADER_FRAGMENT, errorLog) ||
        !shader.checkCompileErrors(geometry, SHADER_GEOMETRY, errorLog)) {
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << errorLog << std::endl;
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteShader(geometry);
        return shader;
    }
    
    shader.ID = glCreateProgram();
    glAttachShader(shader.ID, vertex);
    glAttachShader(shader.ID, fragment);
    glAttachShader(shader.ID, geometry);
    glLinkProgram(shader.ID);
    
    if (!shader.checkLinkErrors(shader.ID, errorLog)) {
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << errorLog << std::endl;
        glDeleteProgram(shader.ID);
        shader.ID = 0;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glDeleteShader(geometry);
    
    return shader;
}

void Shader::use() const {
    if (ID != 0) {
        glUseProgram(ID);
    }
}

void Shader::setBool(const std::string& name, bool value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setVec2(const std::string& name, float x, float y) const {
    glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) const {
    glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
}

void Shader::setMat2(const std::string& name, const glm::mat2& mat) const {
    glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setMat3(const std::string& name, const glm::mat3& mat) const {
    glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}
