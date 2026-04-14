#pragma once
#include <glm/glm.hpp>
#include <string>
#include <functional>
#include "AABB.h"

enum class DoorType { SLIDING, ROTATING };
enum class DoorState { CLOSED, OPENING, OPEN, CLOSING };

namespace DoorFlags {
    constexpr unsigned int STARTS_OPEN = 1;
    constexpr unsigned int UNUSED_2 = 2;
    constexpr unsigned int NON_SOLID_PLAYER = 4;
    constexpr unsigned int PASSABLE = 8;
    constexpr unsigned int TOGGLE = 32;
    constexpr unsigned int USE_ONLY = 256;
    constexpr unsigned int NO_MONSTERS = 512;
    constexpr unsigned int TOUCH_OPENS = 1024;
    constexpr unsigned int START_LOCKED = 2048;
    constexpr unsigned int SILENT = 4096;
}

class DoorEntity {
public:
    DoorEntity();
    ~DoorEntity();

    void initFromEntity(const struct BSPEntity& entity, const class BSPLoader& bsp);
    bool intersectsCapsule(const Capsule& capsule) const;

    AABB getBounds() const;

    // === ТРАНСФОРМАЦИЯ ДЛЯ РЕНДЕРИНГА ===
    glm::mat4 getRenderTransform() const;
    glm::vec3 getCurrentOrigin() const { return currentPos; }
    float getCurrentAngle() const { return currentAngle; }

    bool tryActivate(float touchDistance, bool isPlayerUse = false);
    void update(float deltaTime);

    bool hasFlag(unsigned int flag) const { return (spawnFlags & flag) != 0; }
    bool isTouchOpens() const { return hasFlag(DoorFlags::TOUCH_OPENS); }
    bool isUseOnly() const { return hasFlag(DoorFlags::USE_ONLY); }
    bool isLocked() const { return locked; }
    bool isPassable() const { return hasFlag(DoorFlags::PASSABLE); }
    bool isSilent() const { return hasFlag(DoorFlags::SILENT); }

    DoorType getType() const { return type; }
    DoorState getState() const { return state; }
    const std::string& getTargetName() const { return targetName; }
    const std::string& getClassName() const { return className; }

    glm::vec3 getCurrentMins() const { return currentMins; }
    glm::vec3 getCurrentMaxs() const { return currentMaxs; }

    void setTouched(bool t) { touched = t; }
    bool wasTouched() const { return touched; }

private:
    DoorType type;
    DoorState state;

    glm::vec3 origin;        // Оригинальная позиция (закрыто)
    glm::vec3 angles;        // Оригинальные углы
    glm::vec3 mins, maxs;    // Оригинальные bounds (относительно origin)
    glm::vec3 currentMins, currentMaxs;  // Текущие мировые bounds

    glm::vec3 pos1;          // Закрытая позиция origin
    glm::vec3 pos2;          // Открытая позиция origin  
    glm::vec3 currentPos;    // Текущая позиция origin
    glm::vec3 moveDir;       // Направление движения (для sliding)
    float moveDistance;      // Расстояние движения

    float speed;             // Скорость (юнитов в секунду)
    float wait;              // Время до автозакрытия (-1 = не закрывать)
    float lip;               // Сколько остаётся торчать
    float dmg;               // Урон при блокировке
    int health;              // Здоровье (0 = не ломается)
    unsigned int spawnFlags; // Флаги спавна
    bool locked;             // Закрыта на замок

    float moveProgress;      // 0.0 = закрыта, 1.0 = открыта
    float nextCloseTime;     // Время когда закрываться
    bool touchLogged;        // Флаг для логирования
    bool touched;            // Касались ли в этом кадре

    std::string targetName;  // Имя для активации триггерами
    std::string className;   // Класс энтити

    float currentAngle;      // Текущий угол вращения (для rotating)

    void calculateBoundsFromModel(int modelIndex, const class BSPLoader& bsp);
    void updateRotatedBounds();
    void open();
    void close();
    void stop();
    void updatePosition(float progress);
};