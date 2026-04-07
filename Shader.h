#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>

class Shader {
private:
    unsigned int ID;
    std::string readFile(const char* filePath);
    void checkCompileErrors(unsigned int shader, std::string type);
    bool compiled = false;

public:
    Shader() : ID(0), compiled(false) {}
    Shader(const char* vertexPath, const char* fragmentPath);
    Shader(const std::string& vertexCode, const std::string& fragmentCode);
    ~Shader();

    bool loadFromStrings(const std::string& vertexCode, const std::string& fragmentCode);
    void use();
    void bind() { use(); }
    void unbind() { glUseProgram(0); }
    unsigned int getID() const { return ID; }
    int getLocation(const std::string& name) const;
    std::string getError() const { return errorMsg; }
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setMat4(const std::string& name, const float* value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;

private:
    std::string errorMsg;
};