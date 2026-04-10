#pragma once
#include "Water.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "BSPLoader.h"
#include "Shader.h"

// Структура для хранения водной поверхности
struct WaterSurface {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    size_t indexCount = 0;
    SurfaceType type = SurfaceType::Water;
    int textureIndex = -1;
    
    WaterSurface() = default;
    WaterSurface(const WaterSurface&) = delete;
    WaterSurface& operator=(const WaterSurface&) = delete;
    WaterSurface(WaterSurface&& other) noexcept;
    WaterSurface& operator=(WaterSurface&& other) noexcept;
    ~WaterSurface();
    
    bool build(const std::vector<WaterPoly>& polys);
    void destroy();
    void bind() const;
    static void unbind();
};

// Класс для рендеринга воды с warp-эффектами
class WaterRenderer {
public:
    WaterRenderer();
    ~WaterRenderer();

    WaterRenderer(const WaterRenderer&) = delete;
    WaterRenderer& operator=(const WaterRenderer&) = delete;
    WaterRenderer(WaterRenderer&& other) noexcept;
    WaterRenderer& operator=(WaterRenderer&& other) noexcept;

    // Инициализация
    bool init();
    
    // Загрузка водных поверхностей из BSP
    bool loadFromBSP(BSPLoader& bsp, WADLoader& wad);
    void unload();
    
    // Обновление (время для анимации)
    void update(float deltaTime, float currentTime);
    
    // Рендеринг воды
    void render(const glm::mat4& view, const glm::mat4& projection, 
                const glm::vec3& viewPos, float time);
    
    // Рендеринг подводного эффекта (post-processing)
    void renderUnderwaterEffect(int screenWidth, int screenHeight);
    
    // Проверка, находится ли камера под водой
    bool isCameraUnderwater(const glm::vec3& cameraPos) const;
    
    // Получение менеджера воды
    WaterManager& getWaterManager() { return waterManager; }
    const WaterManager& getWaterManager() const { return waterManager; }
    
    // Настройки
    void setWireframe(bool wireframe) { useWireframe = wireframe; }
    bool getWireframe() const { return useWireframe; }

private:
    WaterManager waterManager;
    std::vector<WaterSurface> waterSurfaces;
    std::vector<GLuint> waterTextures;
    
    std::unique_ptr<Shader> waterShader;
    std::unique_ptr<Shader> underwaterShader;
    
    bool useWireframe = false;
    bool initialized = false;
    
    // Quad для underwater post-processing
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    
    // Статистика
    int totalWaterPolys = 0;
    int totalWaterVertices = 0;
    
    void createQuadMesh();
    void destroyQuadMesh();
    
    // Шейдеры
    static const char* getWaterVert();
    static const char* getWaterFrag();
    static const char* getUnderwaterVert();
    static const char* getUnderwaterFrag();
    
    // Вспомогательные функции
    void extractWaterSurfaces(BSPLoader& bsp, WADLoader& wad);
    bool isWaterTexture(const std::string& name) const;
    SurfaceType getSurfaceTypeFromTexture(const std::string& name) const;
};
