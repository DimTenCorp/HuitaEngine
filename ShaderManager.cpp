#include "ShaderManager.h"
#include "gl_includes.h"
#include <iostream>

unsigned int ShaderManager::program = 0;
unsigned int ShaderManager::shadowProgram = 0;
int ShaderManager::mvpLoc = -1, ShaderManager::modelLoc = -1;
int ShaderManager::viewLoc = -1, ShaderManager::projectionLoc = -1;
int ShaderManager::lightDirLoc = -1, ShaderManager::viewPosLoc = -1, ShaderManager::colorLoc = -1;
int ShaderManager::pointLightPosLoc = -1, ShaderManager::pointLightColorLoc = -1;
int ShaderManager::pointLightIntensityLoc = -1, ShaderManager::pointLightRadiusLoc = -1;
int ShaderManager::sunLightSpaceMatrixLoc = -1, ShaderManager::shadowsEnabledLoc = -1;
int ShaderManager::shadowMapLoc = -1;
int ShaderManager::shadowMVPLoc = -1;
int ShaderManager::sunColorLoc = -1, ShaderManager::sunIntensityLoc = -1;

const char* ShaderManager::vertexSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoord;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform mat4 sunLightSpaceMatrix;
    
    out vec3 vNormal;
    out vec3 vPos;
    out vec2 vTexCoord;
    out vec4 vPosLightSpace;
    
    void main() {
        vec4 worldPos = model * vec4(aPos, 1.0);
        vPos = worldPos.xyz;
        vNormal = mat3(transpose(inverse(model))) * aNormal;
        vTexCoord = aTexCoord;
        vPosLightSpace = sunLightSpaceMatrix * worldPos;
        
        gl_Position = projection * view * worldPos;
    }
)";

const char* ShaderManager::fragmentSource = R"(
    #version 330 core
    in vec3 vNormal;
    in vec3 vPos;
    in vec2 vTexCoord;
    in vec4 vPosLightSpace;
    
    out vec4 FragColor;
    
    uniform sampler2D uTexture;
    uniform sampler2D shadowMap;
    uniform vec3 viewPos;
    
    // Sun light
    uniform vec3 sunDir;
    uniform vec3 sunColor;
    uniform float sunIntensity;
    uniform bool shadowsEnabled;
    
    // Point light
    uniform vec3 pointLightPos;
    uniform vec3 pointLightColor;
    uniform float pointLightIntensity;
    uniform float pointLightRadius;
    
    float calcShadow(vec4 fragPosLightSpace) {
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords = projCoords * 0.5 + 0.5;
        
        if(projCoords.z > 1.0) return 0.0;
        
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        float currentDepth = projCoords.z;
        
        vec3 norm = normalize(vNormal);
        vec3 lightDir = normalize(-sunDir);
        float bias = max(0.0005 * (1.0 - dot(norm, lightDir)), 0.0001);
        
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x,y) * texelSize).r;
                shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
            }
        }
        
        return shadow / 9.0;
    }
    
    void main() {
        vec4 texColor = texture(uTexture, vTexCoord);
        if(texColor.a < 0.5) discard;
        
        vec3 norm = normalize(vNormal);
        vec3 result = texColor.rgb * 0.15; // Ambient
        
        // Sun light
        vec3 sunLightDir = normalize(-sunDir);
        float diffSun = max(dot(norm, sunLightDir), 0.0);
        float shadow = shadowsEnabled ? calcShadow(vPosLightSpace) : 0.0;
        result += texColor.rgb * sunColor * diffSun * sunIntensity * (1.0 - shadow);
        
        // Point light
        vec3 toPointLight = pointLightPos - vPos;
        float distToPoint = length(toPointLight);
        if(distToPoint < pointLightRadius) {
            vec3 pointLightDir = normalize(toPointLight);
            float diffPoint = max(dot(norm, pointLightDir), 0.0);
            float attenuation = 1.0 - (distToPoint / pointLightRadius);
            attenuation *= attenuation;
            result += texColor.rgb * pointLightColor * diffPoint * pointLightIntensity * attenuation;
        }
        
        // Fog
        float dist = length(viewPos - vPos);
        float fog = clamp(exp(-dist * 0.03), 0.0, 1.0);
        result = mix(vec3(0.05, 0.05, 0.1), result, fog);
        
        // Gamma
        result = pow(result, vec3(1.0/2.2));
        
        FragColor = vec4(result, texColor.a);
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
    lightDirLoc = glGetUniformLocation(program, "sunDir");
    viewPosLoc = glGetUniformLocation(program, "viewPos");
    colorLoc = glGetUniformLocation(program, "color");
    
    pointLightPosLoc = glGetUniformLocation(program, "pointLightPos");
    pointLightColorLoc = glGetUniformLocation(program, "pointLightColor");
    pointLightIntensityLoc = glGetUniformLocation(program, "pointLightIntensity");
    pointLightRadiusLoc = glGetUniformLocation(program, "pointLightRadius");
    
    sunLightSpaceMatrixLoc = glGetUniformLocation(program, "sunLightSpaceMatrix");
    shadowsEnabledLoc = glGetUniformLocation(program, "shadowsEnabled");
    shadowMapLoc = glGetUniformLocation(program, "shadowMap");
    sunColorLoc = glGetUniformLocation(program, "sunColor");
    sunIntensityLoc = glGetUniformLocation(program, "sunIntensity");
    
    // Shadow shader locations
    shadowMVPLoc = glGetUniformLocation(shadowProgram, "shadowMVP");

    // Default values
    use();
    glUniform3f(lightDirLoc, 0.3f, -0.7f, 0.5f);
    glUniform3f(sunColorLoc, 1.0f, 1.0f, 0.9f);
    glUniform1f(sunIntensityLoc, 0.8f);
    glUniform1i(shadowsEnabledLoc, 0);
    glUniform3f(pointLightPosLoc, 0.0f, 5.0f, 0.0f);
    glUniform3f(pointLightColorLoc, 1.0f, 0.8f, 0.6f);
    glUniform1f(pointLightIntensityLoc, 1.0f);
    glUniform1f(pointLightRadiusLoc, 20.0f);
    
    glUniformMatrix4fv(sunLightSpaceMatrixLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    
    glUniform1i(shadowMapLoc, 1); // Texture unit 1

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

void ShaderManager::setLightDir(const glm::vec3& dir) {
    glUniform3f(lightDirLoc, dir.x, dir.y, dir.z);
}

void ShaderManager::setViewPos(const glm::vec3& pos) {
    glUniform3fv(viewPosLoc, 1, glm::value_ptr(pos));
}

void ShaderManager::setColor(const glm::vec3& c) {
    glUniform3f(colorLoc, c.x, c.y, c.z);
}

void ShaderManager::setPointLight(const PointLight& light) {
    glUniform3f(pointLightPosLoc, light.position.x, light.position.y, light.position.z);
    glUniform3f(pointLightColorLoc, light.color.x, light.color.y, light.color.z);
    glUniform1f(pointLightIntensityLoc, light.intensity);
    glUniform1f(pointLightRadiusLoc, light.radius);
}

void ShaderManager::setSunLightSpaceMatrix(const glm::mat4& matrix) {
    glUniformMatrix4fv(sunLightSpaceMatrixLoc, 1, GL_FALSE, glm::value_ptr(matrix));
}

void ShaderManager::enableShadows(bool enabled) {
    glUniform1i(shadowsEnabledLoc, enabled ? 1 : 0);
}

void ShaderManager::bindShadowMap(GLuint textureID) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void ShaderManager::useShadowShader() {
    glUseProgram(shadowProgram);
}

void ShaderManager::setShadowMVP(const glm::mat4& mvp) {
    glUniformMatrix4fv(shadowMVPLoc, 1, GL_FALSE, glm::value_ptr(mvp));
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