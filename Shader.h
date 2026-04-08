#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <unordered_map>

class Shader {
public:
    Shader();
    ~Shader();
    
    // Non-copyable, movable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Compile from source strings with optional #version injection
    bool compile(const std::string& vertexSource, const std::string& fragmentSource);
    
    // Bind/unbind
    void bind() const;
    static void unbind();
    
    // Uniform setters
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setUInt(const std::string& name, unsigned int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    
    // Query
    unsigned int getID() const { return programID; }
    bool isValid() const { return programID != 0; }
    const std::string& getLastError() const { return lastError; }
    
    // Clear uniform cache (call after relinking)
    void clearCache();

private:
    unsigned int programID = 0;
    mutable std::unordered_map<std::string, int> uniformCache;
    std::string lastError;
    
    int getUniformLocation(const std::string& name) const;
    static unsigned int compileShader(GLenum type, const std::string& source, std::string& errorLog);
};