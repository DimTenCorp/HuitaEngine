#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "TriangleCollider.h"
#include "AABB.h"
#include <GLFW/glfw3.h>

class MeshCollider;
class CFuncWater;

// HL Physics flags
#define PFLAG_ONLADDER      (1<<0)
#define PFLAG_ONTRAIN       (1<<1)
#define PFLAG_ONBARNACLE    (1<<2)
#define PFLAG_DUCKING       (1<<3)
#define PFLAG_USING         (1<<4)
#define PFLAG_OBSERVER      (1<<5)
#define PFLAG_INWATER       (1<<6)  // Игрок в воде

// HL Fall damage
#define PLAYER_FATAL_FALL_SPEED     1024.0f
#define PLAYER_MAX_SAFE_FALL_SPEED  580.0f
#define DAMAGE_FOR_FALL_SPEED       (100.0f / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED))
#define PLAYER_MIN_BOUNCE_SPEED     200.0f
#define PLAYER_FALL_PUNCH_THRESHOLD 350.0f

// HL Movement
#define PLAYER_WALLJUMP_SPEED       300.0f
#define PLAYER_LONGJUMP_SPEED       350.0f
#define TIME_TO_DUCK                0.25f    // Ускоренный присяд
#define MAX_CLIMB_SPEED             200.0f

// Hull sizes как в HL1
#define VEC_HULL_MIN        glm::vec3(-16.0f, -36.0f, -16.0f)
#define VEC_HULL_MAX        glm::vec3(16.0f, 36.0f, 16.0f)
#define VEC_DUCK_HULL_MIN   glm::vec3(-16.0f, -18.0f, -16.0f)
#define VEC_DUCK_HULL_MAX   glm::vec3(16.0f, 18.0f, 16.0f)
#define VEC_VIEW            glm::vec3(0.0f, 28.0f, 0.0f)
#define VEC_DUCK_VIEW       glm::vec3(0.0f, 12.0f, 0.0f)

#define VEC_HULL_RADIUS     16.0f
#define VEC_HULL_HEIGHT     72.0f  // Полная высота = 72 (от -36 до +36)
#define VEC_DUCK_HULL_HEIGHT 36.0f

class Player {
private:
    glm::vec3 position;
    glm::vec3 velocity;

    bool onGround;
    float speed;
    float jumpForce;
    float gravity;
    float friction;

    float yaw;
    float pitch;

    bool noclipMode;
    float noclipSpeed;

    const MeshCollider* meshCollider = nullptr;

    float stepHeight = 18.0f;

    // Текущие hull (меняются ТОЛЬКО когда полностью сел/встал)
    glm::vec3 m_vecHullMin;
    glm::vec3 m_vecHullMax;

    // HL флаги
    unsigned int m_afPhysicsFlags;
    float m_flFallVelocity;
    float m_flDuckTime;
    float m_flTimeStepSound;
    int m_iStepLeft;
    bool m_fLongJump;
    float m_flWallJumpTime;
    int m_afButtonLast;
    int m_afButtonPressed;
    int m_afButtonReleased;
    char m_chTextureType;

    bool m_bAutoJump = false;
    bool m_bDidAutoJump = false;
    int m_nBunnyHopFrames = 0;

    float m_fHullRadius;      // Радиус капсулы (обычно 16)
    float m_fHullHeight;      // Текущая полная высота капсулы
    float m_fDuckHullHeight;

    // Вода
    float m_flWaterLevel;     // Уровень воды (0-3, как в HL)
    float m_flSwimTime;       // Время плавания
    bool m_bWasInWater;       // Было ли состояние "в воде" в предыдущем кадре

public:
    Player();

    void update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider);
    void handleInput(float deltaTime);
    void moveWithCollision(float deltaTime);
    void moveNoclip(float deltaTime);

    void PreThink(float deltaTime);
    void PostThink(float deltaTime);
    void Jump();
    void Duck(float deltaTime);
    void WalkMove(float deltaTime);
    void FlyMove(float deltaTime);
    void CheckFalling(float deltaTime);

    Capsule getPlayerCapsule(const glm::vec3& pos) const;

    glm::vec3 ClipVelocity(const glm::vec3& in, const glm::vec3& normal, float overbounce);
    bool TryMove(const glm::vec3& start, const glm::vec3& end, glm::vec3& outPos);
    void WalkMove_Sliding(float deltaTime);

    bool checkCollision(const glm::vec3& pos, const AABB& box);
    bool checkCollisionMesh(const glm::vec3& pos) const;
    bool checkOnGround() const;

    void EnableAutoJump(bool enable) { m_bAutoJump = enable; }
    bool IsAutoJumpEnabled() const { return m_bAutoJump; }

    bool stepSlideMove(float deltaTime, int axis);
    void resolveCollisionAxis(float deltaTime, int axis);

    void CategorizePosition();

    // Присяд система
    void StartDuck();
    void StopDuck();
    bool IsDucking() const;
    bool IsFullyDucked() const;

    // Нелинейная интерполяция как в HL1
    float UTIL_SplineFraction(float value, float scale) const;
    float GetDuckFraction() const;
    glm::vec3 GetCurrentViewOffset() const;

    void toggleNoclip() { noclipMode = !noclipMode; }
    bool isNoclip() const { return noclipMode; }

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getMin(const glm::vec3& pos) const { return pos + m_vecHullMin; }
    glm::vec3 getMax(const glm::vec3& pos) const { return pos + m_vecHullMax; }
    glm::vec3 getMin() const { return getMin(position); }
    glm::vec3 getMax() const { return getMax(position); }

    AABB getPlayerAABB(const glm::vec3& pos) const;

    void setPosition(const glm::vec3& pos) { position = pos; }
    bool isOnGround() const { return onGround; }

    void getViewMatrix(float* matrix) const;
    glm::vec3 getEyePosition() const;

    void setYaw(float y) { yaw = y; }
    void setPitch(float p) { pitch = p; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }

    float GetFallVelocity() const { return m_flFallVelocity; }
    bool IsLongJumpEnabled() const { return m_fLongJump; }
    void EnableLongJump(bool enable) { m_fLongJump = enable; }

    // Вода
    void SetInWater(bool inWater);
    bool IsInWater() const { return (m_afPhysicsFlags & PFLAG_INWATER) != 0; }
    void CheckWater(const std::vector<CFuncWater*>& waterZones);
    void ApplyWaterPhysics(float deltaTime);
};