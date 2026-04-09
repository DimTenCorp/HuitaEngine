#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "TriangleCollider.h"
#include "AABB.h"
#include <GLFW/glfw3.h>

class MeshCollider;

// HL: Physics flags
#define PFLAG_ONLADDER		(1<<0)
#define PFLAG_ONTRAIN		(1<<1)
#define PFLAG_ONBARNACLE	(1<<2)
#define PFLAG_DUCKING		(1<<3)
#define PFLAG_USING			(1<<4)
#define PFLAG_OBSERVER		(1<<5)

// HL: Fall damage constants
#define PLAYER_FATAL_FALL_SPEED		1024.0f
#define PLAYER_MAX_SAFE_FALL_SPEED	580.0f
#define DAMAGE_FOR_FALL_SPEED		(100.0f / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED))
#define PLAYER_MIN_BOUNCE_SPEED		200.0f
#define PLAYER_FALL_PUNCH_THRESHOLD	350.0f

// HL: Movement constants
#define PLAYER_WALLJUMP_SPEED		300.0f
#define PLAYER_LONGJUMP_SPEED		350.0f
#define TIME_TO_DUCK				0.4f
#define MAX_CLIMB_SPEED				200.0f

class Player {
private:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 size;			// HL: vec3 for variable hull sizes

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
    bool jumpKeyWasHeld = false;

    float stepHeight = 0.45f;

    // ========== HL ADDITIONS ==========
    unsigned int m_afPhysicsFlags;	// HL: physics flags
    float m_flFallVelocity;			// HL: track fall speed for damage
    float m_flDuckTime;				// HL: when ducking started
    float m_flTimeStepSound;		// HL: footstep timing
    int m_iStepLeft;				// HL: alternate left/right foot
    bool m_fLongJump;				// HL: has longjump module
    float m_flWallJumpTime;			// HL: wall jump cooldown

    // HL: Hull sizes (like HL's VEC_HULL_MIN/MAX)
    glm::vec3 m_vecStandMin;
    glm::vec3 m_vecStandMax;
    glm::vec3 m_vecDuckMin;
    glm::vec3 m_vecDuckMax;
    glm::vec3 m_vecViewStand;
    glm::vec3 m_vecViewDuck;

    // HL: Button tracking
    int m_afButtonLast;
    int m_afButtonPressed;
    int m_afButtonReleased;

    // HL: Texture type for footsteps (simplified - just track surface)
    char m_chTextureType;

    // BHOP
    bool m_bAutoJump = false;      // Для зажатого пробела
    bool m_bDidAutoJump = false; // Флаг что уже прыгнули
    int m_nBunnyHopFrames = 0;     // Счетчик кадров для perfect bhop

public:
    Player();

    void update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider);

    void handleInput(float deltaTime);
    void moveWithCollision(float deltaTime);
    void moveNoclip(float deltaTime);

    // HL-style movement
    void PreThink(float deltaTime);		// HL: called before physics
    void PostThink(float deltaTime);	// HL: called after physics
    void Jump();						// HL: jump with longjump support
    void Duck(float deltaTime);			// HL: proper ducking
    void WalkMove(float deltaTime);
    void FlyMove(float deltaTime);
    void CheckFalling(float deltaTime);	// HL: fall damage

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

    // HL: Categorize position (ground, air, water)
    void CategorizePosition();

    // HL: Get current hull size based on duck state
    void GetHullSize(glm::vec3& outMin, glm::vec3& outMax) const;
    float GetViewOffset() const;

    void toggleNoclip() { noclipMode = !noclipMode; }
    bool isNoclip() const { return noclipMode; }

    // HL: Duck toggle
    void StartDuck() { m_afPhysicsFlags |= PFLAG_DUCKING; m_flDuckTime = glfwGetTime(); }
    void StopDuck() { m_afPhysicsFlags &= ~PFLAG_DUCKING; }
    bool IsDucking() const { return (m_afPhysicsFlags & PFLAG_DUCKING) != 0; }
    bool IsFullyDucked() const;

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getMin(const glm::vec3& pos) const;
    glm::vec3 getMax(const glm::vec3& pos) const;
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

    // HL: Getters for physics state
    float GetFallVelocity() const { return m_flFallVelocity; }
    bool IsLongJumpEnabled() const { return m_fLongJump; }
    void EnableLongJump(bool enable) { m_fLongJump = enable; }
};