#pragma once
#include <glm/glm.hpp>

// Базовый тип вектора для движка
typedef glm::vec3 vec3_t;

// Базовый класс сущности в движке
class CBaseEntity {
protected:
    vec3_t position;
    vec3_t rotation;
    vec3_t scale;
    bool isActive;

public:
    inline CBaseEntity()
        : position(glm::vec3(0.0f, 0.0f, 0.0f)),
        rotation(glm::vec3(0.0f, 0.0f, 0.0f)),
        scale(glm::vec3(1.0f, 1.0f, 1.0f)),
        isActive(true) {}
    virtual inline ~CBaseEntity() {}

    // Виртуальные методы для переопределения
    virtual void Update(float deltaTime);
    virtual void Render();

    // Геттеры и сеттеры позиции
    virtual vec3_t getPosition() const { return position; }
    virtual void setPosition(const vec3_t& pos) { position = pos; }

    // Геттеры и сеттеры вращения
    virtual vec3_t getRotation() const { return rotation; }
    virtual void setRotation(const vec3_t& rot) { rotation = rot; }

    // Геттеры и сеттеры масштаба
    virtual vec3_t getScale() const { return scale; }
    virtual void setScale(const vec3_t& s) { scale = s; }

    // Статус активности
    virtual bool getIsActive() const { return isActive; }
    virtual void setIsActive(bool active) { isActive = active; }
};