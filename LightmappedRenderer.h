#pragma once
#include "Renderer.h"
#include "LightmapManager.h"
#include <memory>

// Расширенный vertex для lightmapped меша
struct BSPVertexLightmapped {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;      // Альбедо UV
    glm::vec2 lightmapCoord; // Lightmap UV
    int lightmapIndex;       // Индекс lightmap (-1 если нет)
};

struct LMFaceDrawCall {
    GLuint texID;
    unsigned int indexOffset;
    unsigned int indexCount;
    int faceIndex;
};

// Класс для рендеринга с запечённым светом
class LightmappedRenderer {
public:
    LightmappedRenderer();
    ~LightmappedRenderer();

    // Запрет копирования
    LightmappedRenderer(const LightmappedRenderer&) = delete;
    LightmappedRenderer& operator=(const LightmappedRenderer&) = delete;

    // Перемещение разрешено
    LightmappedRenderer(LightmappedRenderer&& other) noexcept;
    LightmappedRenderer& operator=(LightmappedRenderer&& other) noexcept;

    // Инициализация
    bool init(int width, int height);

    // Загрузка мира с lightmaps
    bool loadWorld(BSPLoader& bsp, LightmapManager& lightmapManager);
    void unloadWorld();

    // Рендеринг
    void beginFrame(const glm::vec3& clearColor);
    void renderWorld(const glm::mat4& view, const glm::vec3& viewPos,
        BSPLoader& bsp,  // <-- Добавили bsp
        const glm::vec3& ambientColor = glm::vec3(0.05f));

    // Настройки
    void setLightmapIntensity(float intensity) { lightmapIntensity = intensity; }
    float getLightmapIntensity() const { return lightmapIntensity; }

    void setShowLightmapsOnly(bool show) { showLightmapsOnly = show; }
    bool getShowLightmapsOnly() const { return showLightmapsOnly; }

    // Получение статистики
    const Renderer::RenderStats& getStats() const { return stats; }

private:
    // Шейдеры
    std::unique_ptr<Shader> lightmappedShader;

    // Меши
    GLuint worldVAO = 0, worldVBO = 0, worldEBO = 0;
    size_t indexCount = 0;

    // Lightmap менеджер (ссылка)
    LightmapManager* lmManager = nullptr;

    // Параметры
    float lightmapIntensity = 1.0f;
    bool showLightmapsOnly = false;
    int screenWidth = 1280, screenHeight = 720;
    Renderer::RenderStats stats;

    // G-Buffer для deferred rendering (опционально)
    GBuffer gBuffer;

    // Инициализация шейдеров
    bool initShaders();

    // Создание меша с lightmap UV
    bool buildLightmappedMesh(BSPLoader& bsp, LightmapManager& lmManager);

    // Рендеринг проходы
    void geometryPass(const glm::mat4& view, const glm::mat4& proj);
    void lightingPass(const glm::vec3& viewPos, const glm::vec3& ambient);

    // Очистка
    void cleanup();

    // Исходники шейдеров
    static const char* getVertexShaderSource();
    static const char* getFragmentShaderSource();

    std::vector<LMFaceDrawCall> faceDrawCalls;
};