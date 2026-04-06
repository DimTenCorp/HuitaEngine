#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "PointLight.h"
#include "BSPLoader.h"

class ShadowSystem {
public:
    ShadowSystem();
    ~ShadowSystem();

    bool init();
    void cleanup();

    void renderShadowMaps(const std::vector<PointLight*>& lights,
        GLuint bspVAO,
        size_t bspIndexCount,
        const std::vector<FaceDrawCall>& drawCalls,
        const glm::vec3& cameraPos = glm::vec3(0));

    // ИСПРАВЛЕНО: добавили const std::vector<FaceDrawCall>& drawCalls
    void renderFaceToAtlas(PointLight* light, int face,
        GLuint atlasFBO, int atlasX, int atlasY, int tileSize,
        GLuint bspVAO, size_t bspIndexCount,
        const std::vector<FaceDrawCall>& drawCalls);

    void renderSpotShadow(const glm::vec3& pos, const glm::vec3& dir, float radius, float angle,
        GLuint shadowFBO, int width, int height,
        GLuint bspVAO, size_t bspIndexCount,
        const std::vector<FaceDrawCall>& drawCalls);

    void renderCubemapToAtlas(PointLight* light,
        GLuint atlasFBO, int slotX, int slotY, int cubemapTileSize,
        GLuint bspVAO, size_t bspIndexCount,
        const std::vector<FaceDrawCall>& drawCalls);

    GLuint getDepthProgram() const { return depthProgram; }
    GLuint getSpotProgram() const { return spotProgram; }

private:
    GLuint depthProgram = 0;
    GLuint spotProgram = 0;

    bool compileDepthShader();
    bool compileSpotShader();

    // ДОБАВЛЕНА функция в класс
    glm::mat4 calculateViewMatrixForFace(const glm::vec3& pos, int face);

    void getCubemapViewMatrices(const glm::vec3& pos, glm::mat4 views[6], glm::mat4& proj, float radius);
};