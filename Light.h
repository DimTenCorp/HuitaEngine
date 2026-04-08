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
// Выравнивание: структура должна быть кратна 16 байтам для GLSL uniform buffer
// ============================================================================
struct alignas(16) LightData {
    glm::vec4 positionRadius;    // xyz = позиция, w = радиус
    glm::vec4 colorIntensity;    // xyz = цвет, w = интенсивность
    glm::vec4 directionCutoff;   // xyz = направление, w = inner cutoff (cos)
    glm::vec4 outerEnabled;      // x = outer cutoff (cos), y = enabled (1.0/0.0), z = lightType, w = padding
    
    LightData() 
        : positionRadius(0.0f)
        , colorIntensity(1.0f, 1.0f, 1.0f, 1.0f)
        , directionCutoff(0.0f, -1.0f, 0.0f, 0.0f)
        , outerEnabled(0.0f, 1.0f, 0.0f, 0.0f) {}
};

static_assert(sizeof(LightData) == 64, "LightData must be exactly 64 bytes (4 vec4)");
static_assert(alignof(LightData) == 16, "LightData must be 16-byte aligned");

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
    ELightType getType() const { return static_cast<ELightType>(static_cast<uint8_t>(data.outerEnabled.z)); }
    void setType(ELightType type);

    // Позиция (для point/spot lights)
    glm::vec3 getPosition() const { return data.positionRadius.xyz; }
    void setPosition(const glm::vec3& pos) { data.positionRadius.xyz = pos; }

    // Направление (для spot/directional lights)
    glm::vec3 getDirection() const { return data.directionCutoff.xyz; }
    void setDirection(const glm::vec3& dir) { data.directionCutoff.xyz = glm::normalize(dir); }

    // Цвет
    glm::vec3 getColor() const { return data.colorIntensity.xyz; }
    void setColor(const glm::vec3& col) { data.colorIntensity.xyz = col; }

    // Интенсивность
    float getIntensity() const { return data.colorIntensity.w; }
    void setIntensity(float intensity) { data.colorIntensity.w = glm::max(0.0f, intensity); }

    // Радиус затухания (для point/spot lights)
    float getRadius() const { return data.positionRadius.w; }
    void setRadius(float radius) { data.positionRadius.w = glm::max(0.1f, radius); }

    // Углы прожектора (для spot lights)
    float getSpotInnerCutoff() const { return glm::degrees(glm::acos(data.directionCutoff.w)); }
    float getSpotOuterCutoff() const { return glm::degrees(glm::acos(data.outerEnabled.x)); }
    void setSpotCutoff(float innerDegrees, float outerDegrees);

    // Состояние
    bool isEnabled() const { return data.outerEnabled.y > 0.5f; }
    void setEnabled(bool enable) { data.outerEnabled.y = enable ? 1.0f : 0.0f; }

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
