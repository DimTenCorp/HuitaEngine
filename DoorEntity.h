#pragma once
#include <glm/glm.hpp>
#include <string>
#include <functional>
#include "AABB.h"

enum class DoorType { SLIDING, ROTATING };
enum class DoorState { CLOSED, OPENING, OPEN, CLOSING };

namespace DoorFlags {
    constexpr unsigned int STARTS_OPEN = 1;         // Дверь начинается открытой
    constexpr unsigned int DOOR_DONT_LINK = 2;      // Не связывать с другими дверями
    constexpr unsigned int PASSABLE = 4;            // Проходимая (не твердая)
    constexpr unsigned int TOGGLE = 32;             // Переключатель (открыть/закрыть)
    constexpr unsigned int USE_ONLY = 256;          // Только использование (+USE)
    constexpr unsigned int NO_MONSTERS = 512;       // Монстры не могут использовать
    constexpr unsigned int TOUCH_OPENS = 1024;      // Открывается при касании
    constexpr unsigned int START_LOCKED = 2048;     // Начинает заблокированной
    constexpr unsigned int SILENT = 4096;           // Бесшумная дверь
}

class DoorEntity {
public:
    DoorEntity();
    ~DoorEntity();

    void initFromEntity(const struct BSPEntity& entity, const class BSPLoader& bsp);
    bool intersectsCapsule(const Capsule& capsule) const;

    AABB getBounds() const;

    // === ������������� ��� ���������� ===
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

    // Получить скорость двери
    float getSpeed() const { return speed; }
    // Получить расстояние движения
    float getMoveDistance() const { return moveDistance; }
    // Получить направление движения
    glm::vec3 getMoveDir() const { return moveDir; }

private:
    DoorType type;
    DoorState state;

    glm::vec3 origin;        // ������������ ������� (�������)
    glm::vec3 angles;        // ������������ ����
    glm::vec3 mins, maxs;    // ������������ bounds (������������ origin)
    glm::vec3 currentMins, currentMaxs;  // ������� ������� bounds

    glm::vec3 pos1;          // �������� ������� origin
    glm::vec3 pos2;          // �������� ������� origin  
    glm::vec3 currentPos;    // ������� ������� origin
    glm::vec3 moveDir;       // ����������� �������� (��� sliding)
    float moveDistance;      // ���������� ��������

    float speed;             // �������� (������ � �������)
    float wait;              // ����� �� ������������ (-1 = �� ���������)
    float lip;               // ������� ������� �������
    float dmg;               // ���� ��� ����������
    int health;              // �������� (0 = �� ��������)
    unsigned int spawnFlags; // ����� ������
    bool locked;             // ������� �� �����

    float moveProgress;      // 0.0 = �������, 1.0 = �������
    float nextCloseTime;     // ����� ����� �����������
    bool touchLogged;        // ���� ��� �����������
    bool touched;            // �������� �� � ���� �����

    std::string targetName;  // ��� ��� ��������� ����������
    std::string className;   // ����� ������

    float currentAngle;      // ������� ���� �������� (��� rotating)

    void calculateBoundsFromModel(int modelIndex, const class BSPLoader& bsp);
    void updateRotatedBounds();
    void open();
    void close();
    void stop();
    void updatePosition(float progress);
    void setupSlidingMovement(const struct BSPEntity& entity);
};
