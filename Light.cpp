#include "Light.h"
#include "BSPLoader.h"
#include <algorithm>
#include <iostream>

#define _CRT_SECURE_NO_WARNINGS

Light::Light() {
    setType(LightType::Point);
}

Light::Light(LightType type) {
    setType(type);
}

void Light::setType(LightType t) {
    data.outerEnabled.z = static_cast<float>(static_cast<uint8_t>(t));
}

void Light::setPosition(const glm::vec3& pos) {
    data.positionRadius.x = pos.x;
    data.positionRadius.y = pos.y;
    data.positionRadius.z = pos.z;
}

void Light::setDirection(const glm::vec3& dir) {
    data.directionCutoff.x = dir.x;
    data.directionCutoff.y = dir.y;
    data.directionCutoff.z = dir.z;
}

void Light::setColor(const glm::vec3& col) {
    data.colorIntensity.x = col.x;
    data.colorIntensity.y = col.y;
    data.colorIntensity.z = col.z;
}

void Light::setIntensity(float i) {
    data.colorIntensity.w = std::max(0.0f, i);
}

void Light::setRadius(float r) {
    data.positionRadius.w = std::max(0.1f, r);
}

void Light::setSpotAngles(float innerDegrees, float outerDegrees) {
    data.directionCutoff.w = glm::cos(glm::radians(innerDegrees));
    data.outerEnabled.x = glm::cos(glm::radians(outerDegrees));
}

void Light::setEnabled(bool e) {
    data.outerEnabled.y = e ? 1.0f : 0.0f;
}

void Light::setShadowID(int id) {
    data.outerEnabled.w = static_cast<float>(id);
}

Light Light::fromBSPEntity(const BSPEntity& entity) {
    Light light;

    if (entity.classname.find("light_spot") != std::string::npos) {
        light.setType(LightType::Spot);
    }
    else if (entity.classname.find("light_environment") != std::string::npos) {
        light.setType(LightType::Directional);
    }
    else {
        light.setType(LightType::Point);
    }

    light.setPosition(entity.origin);

    if (light.getType() == LightType::Spot) {
        float pitch = glm::radians(entity.angles.x);
        float yaw = glm::radians(entity.angles.y);

        glm::vec3 dir;
        dir.x = cos(pitch) * sin(yaw);
        dir.y = -sin(pitch);
        dir.z = cos(pitch) * cos(yaw);
        light.setDirection(glm::normalize(dir));
        light.setSpotAngles(30.0f, 45.0f);
    }

    auto it = entity.properties.find("_light");
    if (it != entity.properties.end()) {
        int r = 255, g = 255, b = 255, intensity = 200;
        sscanf_s(it->second.c_str(), "%d %d %d %d", &r, &g, &b, &intensity);

        light.setColor(glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f));
        light.setIntensity(intensity / 200.0f);
    }

    it = entity.properties.find("_distance");
    if (it != entity.properties.end()) {
        float dist = std::stof(it->second);
        light.setRadius(dist * 0.025f);
    }
    else {
        light.setRadius(10.0f);
    }

    it = entity.properties.find("_cone");
    if (it != entity.properties.end() && light.getType() == LightType::Spot) {
        float cone = std::stof(it->second);
        light.setSpotAngles(cone * 0.7f, cone);
    }

    light.setEnabled(true);
    light.setShadowID(-1);

    std::cout << "[Light] " << entity.classname << " at ("
        << entity.origin.x << "," << entity.origin.y << "," << entity.origin.z << ")"
        << " radius=" << light.getRadius() << std::endl;

    return light;
}