#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "TriangleCollider.h"

// Quake-style player movement constants
#define BUTTON_ATTACK 1
#define BUTTON_JUMP 2

#define STOP_EPSILON 0.1f
#define MIN_STEP_NORMAL 0.7f  // roughly 45 degrees
#define MAX_CLIP_PLANES 5
#define NUM_BUMPS 4

class MeshCollider;

enum PMoveType {
    PM_NORMAL = 0,        // normal ground movement
    PM_SPECTATOR,         // fly, no clip to world
    PM_DEAD,              // no acceleration
    PM_FLY,               // fly, bump into walls
    PM_NONE,              // can't move
    PM_FREEZE,            // can't move or look around
    PM_LADDER             // on ladder
};

struct MoveVars {
    float gravity;
    float stopspeed;
    float maxspeed;
    float spectatormaxspeed;
    float maxairspeed;
    float accelerate;
    float airaccelerate;
    float wateraccelerate;
    float friction;
    float waterfriction;
    float flyfriction;
    float edgefriction;
    float jumpspeed;
    int stepheight;
    
    // Extended movement flags
    bool airstep;
    bool slidefix;
    bool autobunny;
    bool bunnyfriction;
    
    MoveVars() {
        gravity = 800.0f;
        stopspeed = 100.0f;
        maxspeed = 320.0f;
        spectatormaxspeed = 500.0f;
        maxairspeed = 30.0f;
        accelerate = 10.0f;
        airaccelerate = 1.0f;
        wateraccelerate = 8.0f;
        friction = 6.0f;
        waterfriction = 1.0f;
        flyfriction = 3.0f;
        edgefriction = 2.0f;
        jumpspeed = 270.0f;
        stepheight = 18;  // 18 units
        
        airstep = false;
        slidefix = true;
        autobunny = false;
        bunnyfriction = false;
    }
};

class Player {
private:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 size;  // player mins/maxs
    glm::vec3 mins;
    glm::vec3 maxs;

    bool onGround;
    bool onLadder;
    int waterLevel;
    
    float yaw;
    float pitch;

    PMoveType pmType;
    bool noclipMode;
    
    // Movement state
    bool jumpKeyHeld;
    float waterJumpTime;
    float jumpSecs;
    
    // Legacy movement variables for compatibility
    float speed;
    float noclipSpeed;
    float jumpForce;
    float stepUpSpeed;
    float stepHeight;
    bool jumpKeyWasHeld;  // For jump key state tracking
    float gravity;        // Gravity constant
    
    const MeshCollider* meshCollider;
    
    // Quake-style movement vars
    MoveVars movevars;
    glm::vec3 groundNormal;
    bool groundKnown;
    
    // Collision results
    int numTouch;
    int touchIndex[64];
    glm::vec3 touchVel[64];

    // Helper functions for Quake-style movement
    void CategorizePosition();
    bool TestPlayerPosition(const glm::vec3& point);
    struct TraceResult PM_PlayerTrace(const glm::vec3& start, const glm::vec3& end, unsigned int solidMask);
    int HullPointContents(const glm::vec3& p);
    
    void ClipVelocity(glm::vec3& in, const glm::vec3& normal, glm::vec3& out, float overbounce);
    int SlideMove();
    int StepSlideMove(bool inAir);
    void Friction();
    void Accelerate(const glm::vec3& wishdir, float wishspeed, float accel);
    void AirAccelerate(const glm::vec3& wishdir, float wishspeed, float accel);
    void WaterMove();
    void FlyMove();
    void AirMove();
    void CheckWaterJump();
    void CheckJump();
    void NudgePosition();
    
    void AddTouchedEnt(int num);
    glm::vec3 GetGroundHeight(const glm::vec3& pos);

public:
    Player();

    void update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider);

    void handleInput(float deltaTime);
    void moveWithCollision(float deltaTime);
    void moveNoclip(float deltaTime);
    
    // Quake-style movement function
    void PlayerMove(float gamespeed);

    bool checkCollision(const glm::vec3& pos, const AABB& box);
    bool checkCollisionMesh(const glm::vec3& pos) const;
    bool checkOnGround() const;
    bool tryStepUp(const glm::vec3& fromPos, const glm::vec3& moveDir, float maxStepHeight, float deltaTime);

    void resolveCollisionAxis(float deltaTime, int axis);

    void toggleNoclip() { noclipMode = !noclipMode; }
    bool isNoclip() const { return noclipMode; }
    
    void setPMType(PMoveType type) { pmType = type; }
    PMoveType getPMType() const { return pmType; }
    MoveVars& getMoveVars() { return movevars; }

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getMin(const glm::vec3& pos) const { return pos + mins; }
    glm::vec3 getMax(const glm::vec3& pos) const { return pos + maxs; }
    glm::vec3 getMin() const { return position + mins; }
    glm::vec3 getMax() const { return position + maxs; }

    AABB getPlayerAABB(const glm::vec3& pos) const;

    void setPosition(const glm::vec3& pos) { position = pos; }
    bool isOnGround() const { return onGround; }
    bool isOnLadder() const { return onLadder; }
    int getWaterLevel() const { return waterLevel; }

    void getViewMatrix(float* matrix) const;
    glm::vec3 getEyePosition() const;

    void setYaw(float y) { yaw = y; }
    void setPitch(float p) { pitch = p; }
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    
    glm::vec3 getVelocity() const { return velocity; }
    void setVelocity(const glm::vec3& vel) { velocity = vel; }
};