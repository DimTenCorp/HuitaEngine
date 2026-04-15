#pragma once
#include <glm/glm.hpp>
#include <string>
#include <functional>
#include <vector>
#include "AABB.h"

enum class DoorType { SLIDING, ROTATING };
enum class DoorState { CLOSED, OPENING, OPEN, CLOSING };

namespace DoorFlags {
    // Half-Life spawnflags для func_door:
    constexpr unsigned int STARTS_OPEN = 1;           // SF_DOOR_START_OPEN - дверь начинает открытой
    constexpr unsigned int ROTATE_Y = 0;              // SF_DOOR_ROTATE_Y (по умолчанию вращение вокруг Y)
    constexpr unsigned int PASSABLE = 8;              // SF_DOOR_PASSABLE - проходимая (SOLID_NOT)
    constexpr unsigned int ONEWAY = 16;               // SF_DOOR_ONEWAY - односторонняя
    constexpr unsigned int TOGGLE = 32;               // SF_DOOR_NO_AUTO_RETURN - не закрывать автоматически
    constexpr unsigned int ROTATE_Z = 64;             // SF_DOOR_ROTATE_Z - вращение вокруг Z
    constexpr unsigned int ROTATE_X = 128;            // SF_DOOR_ROTATE_X - вращение вокруг X
    constexpr unsigned int USE_ONLY = 256;            // SF_DOOR_USE_ONLY - только по использованию (E)
    constexpr unsigned int NO_MONSTERS = 512;         // SF_DOOR_NOMONSTERS - монстры не могут открыть
    constexpr unsigned int SILENT = 0x80000000;       // SF_DOOR_SILENT - без звука
    
    // Наши дополнительные флаги (не из HL):
    constexpr unsigned int TOUCH_OPENS = 1024;        // Открывается при касании (как HL двери без targetname)
    constexpr unsigned int START_LOCKED = 2048;       // Заблокирована (требует ключ/кнопку)
}

// OBB для точных коллизий вращающихся дверей
struct OBB {
    glm::vec3 center;
    glm::vec3 halfExtents;
    glm::mat3 rotation;  // Матрица вращения (колонки - локальные оси)

    OBB() : center(0.0f), halfExtents(0.0f), rotation(1.0f) {}

    // Проверка пересечения OBB с AABB
    bool intersectsAABB(const AABB& aabb) const;
};

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

    bool tryActivate(bool isPlayerUse = false);
    void update(float deltaTime);
    bool checkBlocked(const Capsule& playerCapsule, float deltaTime);
    void applyDamageToPlayer(class Player* player, float deltaTime);

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
    float timeSinceOpen;     // Время с момента открытия (для автозакрытия)
    bool touchLogged;        // Флаг для логирования
    bool touched;            // Касались ли в этом кадре

    float damageAccumulator; // Накопленный урон для игрока

    std::string targetName;  // Имя для активации триггерами
    std::string className;   // Класс энтити

    float currentAngle;      // Текущий угол вращения (для rotating)

    // OBB для вращающихся дверей
    OBB obb;
    void updateOBB();

    void calculateBoundsFromModel(int modelIndex, const class BSPLoader& bsp);
    void updateRotatedBounds();
    void open();
    void close();
    void stop();
    void updatePosition(float progress);
};
