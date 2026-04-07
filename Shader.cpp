#include "Shader.h"
#include <iostream>
#include <fstream>
#include <sstream>

Shader::Shader() : ID(0), lastError("") {}

Shader::Shader(const char* vertexPath, const char* fragmentPath) : ID(0), lastError("") {
    std::string vertexCode = readFile(vertexPath);
    std::string fragmentCode = readFile(fragmentPath);

    if (vertexCode.empty() || fragmentCode.empty()) {
        std::cerr << "ERROR::SHADER: Empty shader file" << std::endl;
        return;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex, fragment;

    // Vertex Shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    GLint success;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(vertex);
        return;
    }

    // Fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return;
    }

    // Shader Program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteProgram(ID);
        ID = 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::Shader(const std::string& vertexCode, const std::string& fragmentCode) : ID(0), lastError("") {
    loadFromStrings(vertexCode, fragmentCode);
}

bool Shader::loadFromStrings(const std::string& vertexCode, const std::string& fragmentCode) {
    if (ID != 0) {
        glDeleteProgram(ID);
        ID = 0;
    }
    
    if (vertexCode.empty() || fragmentCode.empty()) {
        lastError = "Empty shader code provided";
        std::cerr << "ERROR::SHADER: Empty shader code provided" << std::endl;
        return false;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex, fragment;

    // Vertex Shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    GLint success;
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        lastError = "Vertex shader compilation failed";
        std::cerr << "Vertex shader compilation failed!" << std::endl;
        glDeleteShader(vertex);
        return false;
    }

    // Fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        lastError = "Fragment shader compilation failed";
        std::cerr << "Fragment shader compilation failed!" << std::endl;
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }

    // Shader Program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        lastError = "Shader program linking failed";
        std::cerr << "Shader program linking failed!" << std::endl;
        char infoLog[512];
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        std::cerr << infoLog << std::endl;
        glDeleteProgram(ID);
        ID = 0;
        return false;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    if (ID != 0) {
        std::cout << "Shader compiled successfully! ID: " << ID << std::endl;
    }
    
    return true;
}

Shader::~Shader() {
    if (ID != 0) {
        glDeleteProgram(ID);
    }
}

std::string Shader::readFile(const char* filePath) {
    std::string content;
    std::ifstream fileStream(filePath, std::ios::in);

    if (!fileStream.is_open()) {
        std::cerr << "Cannot open shader file: " << filePath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << fileStream.rdbuf();
    content = buffer.str();
    fileStream.close();

    return content;
}

void Shader::checkCompileErrors(unsigned int shader, std::string type) {
    int success;
    char infoLog[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "Shader compilation error (" << type << "):\n" << infoLog << std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "Shader linking error:\n" << infoLog << std::endl;
        }
    }
}

void Shader::use() {
    if (ID != 0) {
        glUseProgram(ID);
    }
}

void Shader::setBool(const std::string& name, bool value) const {
    if (ID == 0) return;
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const {
    if (ID == 0) return;
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    if (ID == 0) return;
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setMat4(const std::string& name, const float* value) const {
    if (ID == 0) return;
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, value);
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    if (ID == 0) return;
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    if (ID == 0) return;
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::bind() {
    use();
}

void Shader::unbind() {
    glUseProgram(0);
}

int Shader::getLocation(const std::string& name) {
    if (ID == 0) return -1;
    return glGetUniformLocation(ID, name.c_str());
}