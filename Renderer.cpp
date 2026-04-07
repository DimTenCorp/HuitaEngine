#include "Renderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Статические строки шейдеров вынесены в cpp файл для избежания дублирования памяти
static const char* s_geometryVert = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;
void main() {
    vTexCoord = aTexCoord;
    vNormal = aNormal;
    vFragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

static const char* s_geometryFrag = R"(
#version 330 core
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;
layout (location = 0) out vec4 FragPosition;
layout (location = 1) out vec4 FragNormal;
layout (location = 2) out vec4 FragAlbedo;
uniform sampler2D uTexture;
void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.5) discard;
    FragPosition = vec4(vFragPos, 1.0);
    FragNormal = vec4(normalize(vNormal), 1.0);
    FragAlbedo = texColor;
}
)";

static const char* s_lightingVert = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)";

static const char* s_lightingFrag = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform vec3 uViewPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;
void main() {
    vec3 fragPos = texture(gPosition, vTexCoord).xyz;
    vec3 normal = normalize(texture(gNormal, vTexCoord).xyz);
    vec4 albedo = texture(gAlbedo, vTexCoord);
    
    // Ambient
    vec3 ambient = 0.1 * albedo.rgb;
    
    // Sun directional light
    float diff = max(dot(normal, -uSunDir), 0.0);
    vec3 diffuse = diff * uSunColor * uSunIntensity;
    
    // View direction for specular
    vec3 viewDir = normalize(uViewPos - fragPos);
    vec3 reflectDir = reflect(uSunDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * uSunColor * uSunIntensity * 0.5;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result * albedo.rgb, albedo.a);
}
)";

const char* Renderer::getGeometryVert() { return s_geometryVert; }
const char* Renderer::getGeometryFrag() { return s_geometryFrag; }
const char* Renderer::getLightingVert() { return s_lightingVert; }
const char* Renderer::getLightingFrag() { return s_lightingFrag; }

Renderer::Renderer() = default;
Renderer::~Renderer() { cleanup(); }

bool Renderer::init(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    geometryShader = std::make_unique<Shader>();
    if (!geometryShader->loadFromStrings(getGeometryVert(), getGeometryFrag())) {
        std::cerr << "Geometry shader fail: " << geometryShader->getError() << std::endl;
        geometryShader.reset();
        return false;
    }

    lightingShader = std::make_unique<Shader>();
    if (!lightingShader->loadFromStrings(getLightingVert(), getLightingFrag())) {
        std::cerr << "Lighting shader fail: " << lightingShader->getError() << std::endl;
        geometryShader.reset();
        lightingShader.reset();
        return false;
    }

    lightingShader->bind();
    lightingShader->setInt("gPosition", 0);
    lightingShader->setInt("gNormal", 1);
    lightingShader->setInt("gAlbedo", 2);
    lightingShader->unbind();

    if (!createGBuffer(width, height)) {
        std::cerr << "Failed to create G-Buffer!" << std::endl;
        geometryShader.reset();
        lightingShader.reset();
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    createHitboxMesh();
    return true;
}

bool Renderer::createGBuffer(int w, int h) {
    gBuffer.width = w;
    gBuffer.height = h;

    glGenFramebuffers(1, &gBuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);

    glGenTextures(1, &gBuffer.position);
    glBindTexture(GL_TEXTURE_2D, gBuffer.position);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gBuffer.position, 0);

    glGenTextures(1, &gBuffer.normal);
    glBindTexture(GL_TEXTURE_2D, gBuffer.normal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gBuffer.normal, 0);

    glGenTextures(1, &gBuffer.albedo);
    glBindTexture(GL_TEXTURE_2D, gBuffer.albedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gBuffer.albedo, 0);

    glGenTextures(1, &gBuffer.depth);
    glBindTexture(GL_TEXTURE_2D, gBuffer.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gBuffer.depth, 0);

    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "G-Buffer incomplete!" << std::endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Renderer::setFlashlight(const glm::vec3& pos, const glm::vec3& dir, bool enabled) {
    flashlight.enabled = enabled;
    if (enabled) {
        flashlight.position = pos;
        flashlight.direction = dir;
    }
}

bool Renderer::loadWorld(BSPLoader& bsp) {
    unloadWorld();
    if (!bsp.isLoaded()) return false;

    const auto& vertices = bsp.getMeshVertices();
    const auto& indices = bsp.getMeshIndices();
    if (vertices.empty() || indices.empty()) return false;

    glGenVertexArrays(1, &bspVAO);
    glGenBuffers(1, &bspVBO);
    glGenBuffers(1, &bspEBO);

    glBindVertexArray(bspVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bspVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex),
        vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bspEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex),
        (void*)offsetof(BSPVertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    bspIndexCount = indices.size();
    drawCalls = bsp.getDrawCalls();

    std::sort(drawCalls.begin(), drawCalls.end(),
        [](const FaceDrawCall& a, const FaceDrawCall& b) { return a.texID < b.texID; });

    worldLoaded = true;
    return true;
}

void Renderer::unloadWorld() {
    if (bspVAO) { glDeleteVertexArrays(1, &bspVAO); bspVAO = 0; }
    if (bspVBO) { glDeleteBuffers(1, &bspVBO); bspVBO = 0; }
    if (bspEBO) { glDeleteBuffers(1, &bspEBO); bspEBO = 0; }
    drawCalls.clear();
    worldLoaded = false;
}

void Renderer::beginFrame(const glm::vec3& clearColor) {
    stats = RenderStats();

    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    stats.drawCalls = 0;
    stats.triangles = 0;
}

void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos) {
    renderWorld(view, viewPos, glm::vec3(0.5f, -1.0f, 0.3f));
}

void Renderer::renderWorld(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir) {
    if (!worldLoaded) return;

    glm::mat4 projection = glm::perspective(glm::radians(75.0f),
        (float)screenWidth / screenHeight, 0.1f, 1000.0f);

    geometryPass(view, projection);
    lightingPass(view, viewPos, sunDir);
}

void Renderer::geometryPass(const glm::mat4& view, const glm::mat4& proj) {
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    geometryShader->bind();
    GLint modelLoc = geometryShader->getLocation("model");
    GLint viewLoc = geometryShader->getLocation("view");
    GLint projLoc = geometryShader->getLocation("projection");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(bspVAO);
    for (const auto& dc : drawCalls) {
        glBindTexture(GL_TEXTURE_2D, dc.texID);
        glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
            (void*)(dc.indexOffset * sizeof(unsigned int)));
        stats.drawCalls++;
        stats.triangles += dc.indexCount / 3;
    }
    glBindVertexArray(0);
    geometryShader->unbind();
}

void Renderer::lightingPass(const glm::mat4& view, const glm::vec3& viewPos, const glm::vec3& sunDir) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    lightingShader->bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.position);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gBuffer.normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gBuffer.albedo);

    lightingShader->setVec3("uViewPos", viewPos);
    lightingShader->setVec3("uSunDir", sunDir);
    lightingShader->setVec3("uSunColor", glm::vec3(1.0f, 0.95f, 0.8f));
    lightingShader->setFloat("uSunIntensity", 1.0f);

    static GLuint quadVAO = 0;
    static GLuint quadVBO = 0;
    if (quadVAO == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    lightingShader->unbind();
}

void Renderer::renderHitbox(const glm::mat4& view, const glm::mat4& projection,
    const glm::vec3& position, bool visible) {
    if (!visible || !showHitbox) return;

    // Проверка что cubeVAO был создан
    if (cubeVAO == 0) {
        createHitboxMesh();
        if (cubeVAO == 0) return; // Не удалось создать mesh
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
    model = model * scaleMat;

    // Отрисовка хитбокса wireframe
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
}

void Renderer::createHitboxMesh() {
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Renderer::cleanup() {
    destroyGBuffer();

    // Unique pointers automatically clean up - just reset them
    geometryShader.reset();
    lightingShader.reset();
    forwardShader.reset();

    if (bspVAO) glDeleteVertexArrays(1, &bspVAO);
    if (bspVBO) glDeleteBuffers(1, &bspVBO);
    if (bspEBO) glDeleteBuffers(1, &bspEBO);

    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO) glDeleteBuffers(1, &cubeEBO);

    if (flashlight.spotFBO) glDeleteFramebuffers(1, &flashlight.spotFBO);
    if (flashlight.spotShadowMap) glDeleteTextures(1, &flashlight.spotShadowMap);
}

void Renderer::destroyGBuffer() {
    if (gBuffer.fbo) {
        glDeleteFramebuffers(1, &gBuffer.fbo);
        gBuffer.fbo = 0;
    }
    if (gBuffer.position) {
        glDeleteTextures(1, &gBuffer.position);
        gBuffer.position = 0;
    }
    if (gBuffer.normal) {
        glDeleteTextures(1, &gBuffer.normal);
        gBuffer.normal = 0;
    }
    if (gBuffer.albedo) {
        glDeleteTextures(1, &gBuffer.albedo);
        gBuffer.albedo = 0;
    }
    if (gBuffer.depth) {
        glDeleteTextures(1, &gBuffer.depth);
        gBuffer.depth = 0;
    }
}

void Renderer::setViewport(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    destroyGBuffer();
    createGBuffer(width, height);
}
