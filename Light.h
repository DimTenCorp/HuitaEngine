#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

// ============================================================================
// Light Types Enumeration
// ============================================================================
enum class ELightType : uint8_t {
    Point = 0,
    Spot = 1,
    Directional = 2
};

// ============================================================================
// Light Data Structure - POD для передачи в шейдеры
// ============================================================================
struct LightData {
    glm::vec3 position{0.0f};      // Для directional: направление
    float radius{10.0f};
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // Для spot/directional
    float spotInnerCutoff{0.0f};   // cos(innerAngle)
    float spotOuterCutoff{0.0f};   // cos(outerAngle)
    bool enabled{true};
    uint8_t lightType{0};          // ELightType
    uint8_t padding[3]{0};         // Выравнивание до 16 байт
};

static_assert(sizeof(LightData) % 16 == 0, "LightData must be 16-byte aligned for GPU");

// ============================================================================
// CLight Class - высокоуровневый класс источника света
// ============================================================================
class CLight {
public:
    CLight();
    explicit CLight(ELightType type);
    virtual ~CLight();

    // Move semantics
    CLight(CLight&& other) noexcept;
    CLight& operator=(CLight&& other) noexcept;
    
    // Copy semantics (deleted for resource safety)
    CLight(const CLight&) = delete;
    CLight& operator=(const CLight&) = delete;

    // Обновление данных света
    void Update(float deltaTime);
    
    // Получить данные для шейдера
    const LightData& getData() const { return data; }
    LightData& getData() { return data; }

    // Тип света
    ELightType getType() const { return static_cast<ELightType>(data.lightType); }
    void setType(ELightType type);

    // Позиция (для point/spot lights)
    glm::vec3 getPosition() const { return data.position; }
    void setPosition(const glm::vec3& pos) { data.position = pos; }

    // Направление (для spot/directional lights)
    glm::vec3 getDirection() const { return data.direction; }
    void setDirection(const glm::vec3& dir) { data.direction = glm::normalize(dir); }

    // Цвет
    glm::vec3 getColor() const { return data.color; }
    void setColor(const glm::vec3& col) { data.color = col; }

    // Интенсивность
    float getIntensity() const { return data.intensity; }
    void setIntensity(float intensity) { data.intensity = glm::max(0.0f, intensity); }

    // Радиус затухания (для point/spot lights)
    float getRadius() const { return data.radius; }
    void setRadius(float radius) { data.radius = glm::max(0.1f, radius); }

    // Углы прожектора (для spot lights)
    float getSpotInnerCutoff() const { return glm::degrees(glm::acos(data.spotInnerCutoff)); }
    float getSpotOuterCutoff() const { return glm::degrees(glm::acos(data.spotOuterCutoff)); }
    void setSpotCutoff(float innerDegrees, float outerDegrees);

    // Состояние
    bool isEnabled() const { return data.enabled; }
    void setEnabled(bool enable) { data.enabled = enable; }

    // Утилиты
    static LightData createDirectional(const glm::vec3& direction, 
                                        const glm::vec3& color, 
                                        float intensity);
    static LightData createPoint(const glm::vec3& position,
                                  const glm::vec3& color,
                                  float intensity,
                                  float radius);
    static LightData createSpot(const glm::vec3& position,
                                 const glm::vec3& direction,
                                 const glm::vec3& color,
                                 float intensity,
                                 float innerDegrees,
                                 float outerDegrees,
                                 float radius);

private:
    LightData data;
};

// ============================================================================
// LightManager Class - управляет коллекцией источников света
// ============================================================================
class LightManager {
public:
    static constexpr size_t MAX_LIGHTS = 64;
    static constexpr size_t MAX_POINT_LIGHTS = 16;
    static constexpr size_t MAX_SPOT_LIGHTS = 16;

    LightManager();
    ~LightManager();

    // Non-copyable
    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

    // Добавление光源
    size_t addLight(std::unique_ptr<CLight> light);
    void removeLight(size_t id);
    CLight* getLight(size_t id);
    const CLight* getLight(size_t id) const;

    // Очистка
    void clear();

    // Получение данных для шейдера
    const std::vector<LightData>& getLightData() const { return lightData; }
    size_t getActiveLightCount() const;
    
    // Главный направленный свет (солнце/луна)
    void setSunDirection(const glm::vec3& dir) { sunDirection = glm::normalize(dir); }
    glm::vec3 getSunDirection() const { return sunDirection; }
    
    void setSunColor(const glm::vec3& color) { sunColor = color; }
    glm::vec3 getSunColor() const { return sunColor; }
    
    void setSunIntensity(float intensity) { sunIntensity = glm::max(0.0f, intensity); }
    float getSunIntensity() const { return sunIntensity; }

    void setAmbientStrength(float strength) { ambientStrength = glm::clamp(strength, 0.0f, 1.0f); }
    float getAmbientStrength() const { return ambientStrength; }

private:
    std::vector<std::unique_ptr<CLight>> lights;
    std::vector<LightData> lightData;
    size_t nextId{0};
    
    glm::vec3 sunDirection{0.5f, -1.0f, 0.3f};
    glm::vec3 sunColor{1.0f, 0.95f, 0.8f};
    float sunIntensity{1.0f};
    float ambientStrength{0.1f};

    void rebuildLightData();
};
