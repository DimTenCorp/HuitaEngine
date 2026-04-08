#pragma once
#include <glm/glm.hpp>
#include <string>

enum class LightType : uint8_t {
    Point = 0,
    Spot = 1,
    Directional = 2
};

struct alignas(16) LightShaderData {
    glm::vec4 positionRadius;
    glm::vec4 colorIntensity;
    glm::vec4 directionCutoff;
    glm::vec4 outerEnabled;

    LightShaderData() {
        positionRadius = glm::vec4(0, 0, 0, 10);
        colorIntensity = glm::vec4(1, 1, 1, 1);
        directionCutoff = glm::vec4(0, -1, 0, 0);
        outerEnabled = glm::vec4(0, 1, 0, -1);
    }
};

static_assert(sizeof(LightShaderData) == 64, "LightShaderData must be 64 bytes");

class Light {
public:
    Light();
    explicit Light(LightType type);

    void setType(LightType t);
    void setPosition(const glm::vec3& pos);
    void setDirection(const glm::vec3& dir);
    void setColor(const glm::vec3& col);
    void setIntensity(float i);
    void setRadius(float r);
    void setSpotAngles(float innerDegrees, float outerDegrees);
    void setEnabled(bool e);
    void setShadowID(int id);

    const LightShaderData& getShaderData() const { return data; }
    LightType getType() const { return static_cast<LightType>(static_cast<uint8_t>(data.outerEnabled.z)); }
    glm::vec3 getPosition() const { return glm::vec3(data.positionRadius); }
    float getRadius() const { return data.positionRadius.w; }
    int getShadowID() const { return static_cast<int>(data.outerEnabled.w); }

    static Light fromBSPEntity(const struct BSPEntity& entity);

private:
    LightShaderData data;
};