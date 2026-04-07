#pragma once
#include <glm/glm.hpp>
#include "Entity.h"

class CLight : public CBaseEntity {
public:
    enum LightType {
        POINT_LIGHT = 0,
        SPOT_LIGHT = 1,
        DIRECTIONAL_LIGHT = 2
    };

private:
    LightType lightType;
    glm::vec3 color;
    float brightness;
    float radius;
    float constant;
    float linear;
    float quadratic;
    float spotCutoff;
    float spotOuterCutoff;
    glm::vec3 direction;

public:
    CLight();
    virtual ~CLight();

    // Override методы базового класса
    virtual void Update(float deltaTime) override;
    virtual void Render() override;

    // Геттеры и сеттеры
    LightType getLightType() const { return lightType; }
    void setLightType(LightType type) { lightType = type; }

    glm::vec3 getColor() const { return color; }
    void setColor(const glm::vec3& col) { color = col; }

    float getBrightness() const { return brightness; }
    void SetBrightness(float b) { brightness = b; }

    float getRadius() const { return radius; }
    void setRadius(float r) { radius = r; }

    float getConstant() const { return constant; }
    void setConstant(float c) { constant = c; }

    float getLinear() const { return linear; }
    void setLinear(float l) { linear = l; }

    float getQuadratic() const { return quadratic; }
    void setQuadratic(float q) { quadratic = q; }

    float getSpotCutoff() const { return spotCutoff; }
    void setSpotCutoff(float cutoff) { spotCutoff = cutoff; }

    float getSpotOuterCutoff() const { return spotOuterCutoff; }
    void setSpotOuterCutoff(float outerCutoff) { spotOuterCutoff = outerCutoff; }

    glm::vec3 getDirection() const { return direction; }
    void setDirection(const glm::vec3& dir) { direction = dir; }

    // Вектор позиции (наследуется от CBaseEntity)
    vec3_t getPosition() const;
    void setPosition(const vec3_t& pos);
};
