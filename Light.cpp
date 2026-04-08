#include "Light.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// CLight Implementation
// ============================================================================

CLight::CLight() 
    : data{} {
    setType(ELightType::Point);
    setPosition(glm::vec3(0.0f));
    setColor(glm::vec3(1.0f));
    setIntensity(1.0f);
    setRadius(10.0f);
    setDirection(glm::vec3(0.0f, -1.0f, 0.0f));
    setEnabled(true);
}

CLight::CLight(ELightType type) 
    : data{} {
    setType(type);
}

CLight::~CLight() = default;

CLight::CLight(CLight&& other) noexcept 
    : data(other.data) {
    other.data = LightData{};
}

CLight& CLight::operator=(CLight&& other) noexcept {
    if (this != &other) {
        data = other.data;
        other.data = LightData{};
    }
    return *this;
}

void CLight::Update(float deltaTime) {
    // Базовая логика обновления может быть расширена в подклассах
    (void)deltaTime;
}

void CLight::setType(ELightType type) {
    data.outerEnabled.z = static_cast<float>(static_cast<uint8_t>(type));
    
    // Установка значений по умолчанию для разных типов света
    switch (type) {
        case ELightType::Directional:
            data.positionRadius.w = 0.0f; // Не используется для directional
            break;
        case ELightType::Point:
            data.positionRadius.w = 10.0f;
            data.directionCutoff.w = 0.0f;
            data.outerEnabled.x = 0.0f;
            break;
        case ELightType::Spot:
            data.positionRadius.w = 20.0f;
            setSpotCutoff(12.5f, 17.5f);
            break;
    }
}

void CLight::setSpotCutoff(float innerDegrees, float outerDegrees) {
    data.directionCutoff.w = glm::cos(glm::radians(innerDegrees));
    data.outerEnabled.x = glm::cos(glm::radians(outerDegrees));
}

LightData CLight::createDirectional(const glm::vec3& direction,
                                     const glm::vec3& color,
                                     float intensity) {
    LightData data;
    data.outerEnabled.z = static_cast<float>(static_cast<uint8_t>(ELightType::Directional));
    data.directionCutoff = glm::vec4(glm::normalize(direction), 0.0f);
    data.colorIntensity = glm::vec4(color, intensity);
    data.positionRadius.w = 0.0f;
    data.outerEnabled.y = 1.0f;
    return data;
}

LightData CLight::createPoint(const glm::vec3& position,
                               const glm::vec3& color,
                               float intensity,
                               float radius) {
    LightData data;
    data.outerEnabled.z = static_cast<float>(static_cast<uint8_t>(ELightType::Point));
    data.positionRadius = glm::vec4(position, radius);
    data.colorIntensity = glm::vec4(color, intensity);
    data.directionCutoff = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    data.outerEnabled.x = 0.0f;
    data.outerEnabled.y = 1.0f;
    return data;
}

LightData CLight::createSpot(const glm::vec3& position,
                              const glm::vec3& direction,
                              const glm::vec3& color,
                              float intensity,
                              float innerDegrees,
                              float outerDegrees,
                              float radius) {
    LightData data;
    data.outerEnabled.z = static_cast<float>(static_cast<uint8_t>(ELightType::Spot));
    data.positionRadius = glm::vec4(position, radius);
    data.directionCutoff = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(innerDegrees)));
    data.colorIntensity = glm::vec4(color, intensity);
    data.outerEnabled.x = glm::cos(glm::radians(outerDegrees));
    data.outerEnabled.y = 1.0f;
    return data;
}

// ============================================================================
// LightManager Implementation
// ============================================================================

LightManager::LightManager() = default;

LightManager::~LightManager() = default;

size_t LightManager::addLight(std::unique_ptr<CLight> light) {
    if (lights.size() >= MAX_LIGHTS) {
        return static_cast<size_t>(-1);
    }
    
    size_t id = nextId++;
    lights.push_back(std::move(light));
    rebuildLightData();
    return id;
}

void LightManager::removeLight(size_t id) {
    if (id >= lights.size()) {
        return;
    }
    
    lights.erase(lights.begin() + static_cast<long long>(id));
    rebuildLightData();
}

CLight* LightManager::getLight(size_t id) {
    if (id >= lights.size()) {
        return nullptr;
    }
    return lights[id].get();
}

const CLight* LightManager::getLight(size_t id) const {
    if (id >= lights.size()) {
        return nullptr;
    }
    return lights[id].get();
}

void LightManager::clear() {
    lights.clear();
    lightData.clear();
}

size_t LightManager::getActiveLightCount() const {
    size_t count = 0;
    for (const auto& light : lights) {
        if (light && light->isEnabled()) {
            ++count;
        }
    }
    return count;
}

void LightManager::rebuildLightData() {
    lightData.clear();
    lightData.reserve(lights.size());
    
    for (const auto& light : lights) {
        if (light) {
            lightData.push_back(light->getData());
        }
    }
}
