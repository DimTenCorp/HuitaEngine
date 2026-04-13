#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "BSPLoader.h"

// ��������� lightmap ��� ������ face
struct FaceLightmap {
    GLuint textureID = 0;           // OpenGL �������� lightmap (��������������)
    int width = 0;                  // ������ lightmap � ��������
    int height = 0;                 // ������ lightmap � ��������
    int offset = 0;                 // �������� � lighting lump
    float minS = 0.0f, minT = 0.0f; // ����������� UV ��� ������� ���������
    glm::vec2 uvMin{ 0.0f };          // UV � ������ (min)
    glm::vec2 uvMax{ 0.0f };          // UV � ������ (max)
    bool valid = false;             // ������� �� lightmap
};

// ����� lightmaps
class LightmapAtlas {
public:
    GLuint atlasTexture = 0;
    int atlasWidth = 2048;   // Ширина атласа
    int atlasHeight = 2048;  // Высота атласа
    int currentX = 0, currentY = 0;
    int rowHeight = 0;
    bool initialized = false;

    bool init();
    bool init(int width, int height);  // Перегруженная версия с указанием размера
    // ���������� UV ���������� � ������: (minU, minV, maxU, maxV)
    glm::vec4 packLightmap(int width, int height, const uint8_t* data);
    void bind(GLuint unit) const;
    void cleanup();
};

// �������� lightmaps - ������������
class LightmapManager {
public:
    LightmapManager();
    ~LightmapManager();

    LightmapManager(const LightmapManager&) = delete;
    LightmapManager& operator=(const LightmapManager&) = delete;
    LightmapManager(LightmapManager&& other) noexcept;
    LightmapManager& operator=(LightmapManager&& other) noexcept;

    // ������������� �� BSP - �������� �����
    bool initializeFromBSP(const BSPLoader& bsp);

    // ��������� lightmap ��� face
    const FaceLightmap& getFaceLightmap(int faceIndex) const;

    // �������� ������� lightmaps
    bool hasLightmaps() const { return !faceLightmaps.empty() && hasValidLightmaps; }

    // ��������� ����������
    size_t getLightmapCount() const { return faceLightmaps.size(); }

    // �����
    const LightmapAtlas& getAtlas() const { return atlas; }

    void debugPrintLightmapInfo() const;

    // �������
    void cleanup();

private:
    std::vector<FaceLightmap> faceLightmaps;
    LightmapAtlas atlas;
    bool hasValidLightmaps = false;

    // �������� �������� �� raw ������
    GLuint createLightmapTexture(int width, int height, const uint8_t* data);
};