#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "BSPLoader.h"
#include "Shader.h"
#include "Camera.h"

// Структура для хранения поверхности рендеринга
struct RenderSurface {
    unsigned int faceIndex = 0;           // Индекс грани в BSP
    unsigned int materialIdx = 0;         // Индекс материала/текстуры
    unsigned int vertexOffset = 0;        // Смещение вершин в буфере
    unsigned int vertexCount = 0;         // Количество вершин
    glm::vec3 origin{0.0f};               // Центр поверхности
    unsigned int flags = 0;               // Флаги поверхности (DRAWSKY и т.д.)
    glm::uvec2 lightmapCoord{0, 0};       // Координаты в lightmap атласе
};

// Список видимых поверхностей для текущего кадра
struct WorldSurfaceList {
    std::vector<std::vector<unsigned>> textureChain;      // Цепочки поверхностей по текстурам
    std::vector<unsigned> textureChainFrames;             // Фреймы для каждой цепочки
    std::vector<unsigned> skySurfaces;                    // Небесные поверхности
    unsigned textureChainFrame = 0;                       // Текущий фрейм цепочки

    unsigned visFrame = 0;                                // Текущий фрейм видимости
    int viewLeaf = 0;                                     // Индекс листа просмотра
    std::vector<unsigned> nodeVisFrame;                   // Висфреймы узлов
    std::vector<unsigned> leafVisFrame;                   // Висфреймы листьев
};

class WorldRenderer {
public:
    WorldRenderer();
    ~WorldRenderer();

    // Инициализация рендерера после загрузки BSP
    bool initialize(const BSPLoader& bspLoader);
    void cleanup();

    // Получение видимых поверхностей для камеры
    void getVisibleSurfaces(const Camera& camera, WorldSurfaceList& surfList);

    // Рендеринг мира
    void drawTexturedWorld(WorldSurfaceList& surfList, const Shader& shader);
    void drawSkybox(WorldSurfaceList& surfList, const Shader& shader);
    void drawWireframe(WorldSurfaceList& surfList, const Shader& wireframeShader, bool drawSky);

    // Настройка lightstyle для анимации освещения
    void setLightstyleScale(int style, float scale);

    // Статистика
    struct Stats {
        unsigned int drawCalls = 0;
        unsigned int worldPolys = 0;
        unsigned int skyPolys = 0;
        void reset() { drawCalls = 0; worldPolys = 0; skyPolys = 0; }
    };
    const Stats& getStats() const { return stats; }

private:
    // Узлы BSP для рендеринга
    struct BaseNode {
        int contents = 0;
        unsigned parentIdx = 0;
    };

    struct Node : public BaseNode {
        unsigned firstSurface = 0;
        unsigned numSurfaces = 0;
        const BSPPlane* plane = nullptr;
        unsigned children[2] = {0, 0};
    };

    struct Leaf : public BaseNode {
        const uint8_t* compressedVis = nullptr;
    };

    // Данные мира
    std::vector<Node> nodes;
    std::vector<Leaf> leaves;
    std::vector<RenderSurface> surfaces;
    unsigned int nextMaterialIndex = 0;

    // Буферы
    GLuint worldEBO = 0;
    std::vector<uint16_t> worldEBOBuf;

    // Lightstyles
    float lightstyleScales[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Методы
    void createLeaves(const BSPLoader& bspLoader);
    void createNodes(const BSPLoader& bspLoader);
    void updateNodeParents(int nodeIdx, int parent);
    void createBuffers();
    void markLeaves(const Camera& camera, WorldSurfaceList& surfList);
    void recursiveWorldNodesTextured(const Camera& camera, WorldSurfaceList& surfList, int nodeIdx);

    Stats stats;
    bool initialized = false;
};
