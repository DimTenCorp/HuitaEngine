#define GLM_ENABLE_EXPERIMENTAL
#include "Player.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/normalize_dot.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "TriangleCollider.h"

// Trace result structure for collision detection
struct TraceResult {
    float fraction;
    glm::vec3 endpos;
    glm::vec3 normal;
    int entnum;
    bool startsolid;
    bool allsolid;
    bool inwater;
    
    TraceResult() : fraction(1.0f), endpos(0.0f), normal(0.0f), entnum(0), 
                    startsolid(false), allsolid(false), inwater(false) {}
};

Player::Player() {
    position = glm::vec3(0.0f, 2.0f, 0.0f);
    velocity = glm::vec3(0.0f);
    size = glm::vec3(0.3f, 0.9f, 0.3f);
    
    // Quake-style player bounding box (16x16x36 units, halfed for mins/maxs from center)
    mins = glm::vec3(-16.0f * 0.0254f, -16.0f * 0.0254f, 0.0f);  // Convert from inches to meters
    maxs = glm::vec3(16.0f * 0.0254f, 16.0f * 0.0254f, 36.0f * 0.0254f);

    onGround = false;
    onLadder = false;
    waterLevel = 0;
    
    pmType = PM_NORMAL;
    noclipMode = false;

    yaw = -90.0f;
    pitch = 0.0f;
    
    jumpKeyHeld = false;
    waterJumpTime = 0.0f;
    jumpSecs = 0.0f;
    
    meshCollider = nullptr;
    groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    groundKnown = false;
    
    numTouch = 0;
}

// Utility function to get vector length
static float VectorLength(const glm::vec3& v) {
    return glm::length(v);
}

// Utility function to normalize vector and return original length
static float VectorNormalize(glm::vec3& v) {
    float len = glm::length(v);
    if (len > 0.0f) {
        v = glm::normalize(v);
    }
    return len;
}

// Utility function for dot product
static float DotProduct(const glm::vec3& a, const glm::vec3& b) {
    return glm::dot(a, b);
}

// Utility function to scale vector
static void VectorScale(glm::vec3& out, const glm::vec3& in, float scale) {
    out = in * scale;
}

// Add entity to touch list, discarding duplicates
void Player::AddTouchedEnt(int num) {
    if (numTouch >= 64)
        return;
    
    if (numTouch > 0) {
        if (touchIndex[numTouch - 1] == num)
            return;  // already added
    }
    
    touchIndex[numTouch] = num;
    touchVel[numTouch] = velocity;
    numTouch++;
}

// Clip velocity when hitting a surface
void Player::ClipVelocity(glm::vec3& in, const glm::vec3& normal, glm::vec3& out, float overbounce) {
    float backoff = DotProduct(in, normal) * overbounce;
    
    for (int i = 0; i < 3; i++) {
        float change = normal[i] * backoff;
        out[i] = in[i] - change;
        if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
            out[i] = 0.0f;
    }
}

// Get point contents using hull testing
int Player::HullPointContents(const glm::vec3& p) {
    if (!meshCollider)
        return 0;  // CONTENTS_EMPTY
    
    AABB testBox;
    testBox.min = p + mins;
    testBox.max = p + maxs;
    
    if (meshCollider->intersectAABB(testBox))
        return 1;  // CONTENTS_SOLID
    
    return 0;  // CONTENTS_EMPTY
}

// Test if player position is valid (not in solid)
bool Player::TestPlayerPosition(const glm::vec3& point) {
    return !checkCollisionMesh(point);
}

// Perform a player trace (box trace)
TraceResult Player::PM_PlayerTrace(const glm::vec3& start, const glm::vec3& end, unsigned int solidMask) {
    TraceResult result;
    
    if (!meshCollider) {
        result.fraction = 1.0f;
        result.endpos = end;
        return result;
    }
    
    // Simple box trace implementation
    glm::vec3 dir = end - start;
    float dist = VectorLength(dir);
    
    if (dist < 0.001f) {
        result.startsolid = checkCollisionMesh(start);
        result.fraction = result.startsolid ? 0.0f : 1.0f;
        result.endpos = start;
        return result;
    }
    
    glm::vec3 normalizedDir = glm::normalize(dir);
    
    // Step through the trace in small increments
    int steps = static_cast<int>(dist * 100.0f) + 1;
    float stepSize = dist / steps;
    
    glm::vec3 currentPos = start;
    glm::vec3 prevPos = start;
    
    for (int i = 0; i <= steps; i++) {
        currentPos = start + normalizedDir * (i * stepSize);
        
        if (checkCollisionMesh(currentPos)) {
            // Found collision, interpolate to find exact hit point
            float t = 0.5f;
            for (int j = 0; j < 8; j++) {
                glm::vec3 testPos = prevPos + (currentPos - prevPos) * t;
                if (checkCollisionMesh(testPos)) {
                    currentPos = testPos;
                    t *= 0.5f;
                } else {
                    prevPos = testPos;
                    t += t * 0.5f;
                }
            }
            
            result.fraction = glm::distance(start, prevPos) / dist;
            result.endpos = prevPos;
            result.normal = normalizedDir;  // Simplified normal
            result.startsolid = (i == 0);
            return result;
        }
        
        prevPos = currentPos;
    }
    
    result.fraction = 1.0f;
    result.endpos = end;
    return result;
}

// Get ground height at position
glm::vec3 Player::GetGroundHeight(const glm::vec3& pos) {
    glm::vec3 testPos = pos;
    testPos.y -= 1.0f;  // Test below player
    
    TraceResult tr = PM_PlayerTrace(pos, testPos, 1);
    
    if (tr.fraction < 1.0f && !tr.startsolid) {
        return tr.endpos;
    }
    
    return pos;
}

// Categorize player position (on ground, in water, etc.)
void Player::CategorizePosition() {
    glm::vec3 testPoint = position;
    testPoint.y += mins.y - 2.0f * 0.0254f;  // Test below feet
    
    int pointContents = HullPointContents(testPoint);
    
    // Check if on ground
    if (pmType != PM_FLY && pmType != PM_SPECTATOR && pmType != PM_NONE) {
        glm::vec3 downTest = position;
        downTest.y -= 2.0f * 0.0254f;  // Small offset
        
        if (!TestPlayerPosition(downTest)) {
            onGround = true;
            groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            groundKnown = true;
        } else {
            onGround = false;
            groundKnown = false;
        }
    }
    
    // Water level detection (simplified)
    waterLevel = 0;
    onLadder = false;
}

void Player::handleInput(float deltaTime) {
    if (noclipMode) return;

    glm::vec3 wishDir(0.0f);

    float yawRad = glm::radians(yaw);
    glm::vec3 forward(cos(yawRad), 0.0f, sin(yawRad));
    glm::vec3 right(sin(yawRad), 0.0f, -cos(yawRad));

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir += right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir -= right;

    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
        velocity.x = wishDir.x * speed;
        velocity.z = wishDir.z * speed;
    }
    else {
        velocity.x = 0.0f;
        velocity.z = 0.0f;
    }

    bool spacePressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

    if (spacePressed) {
        if (!jumpKeyWasHeld && onGround && velocity.y <= 0.01f) {
            velocity.y = jumpForce;
            onGround = false;
            position.y += 0.01f;
        }
        jumpKeyWasHeld = true;
    }
    else {
        jumpKeyWasHeld = false;
    }
}

void Player::moveNoclip(float deltaTime) {
    glm::vec3 wishDir(0.0f);

    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);

    glm::vec3 forward(cos(yawRad) * cos(pitchRad), sin(pitchRad), sin(yawRad) * cos(pitchRad));
    glm::vec3 right(sin(yawRad), 0.0f, -cos(yawRad));
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) wishDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) wishDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) wishDir += right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) wishDir -= right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) wishDir += up;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) wishDir -= up;

    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
        position += wishDir * noclipSpeed * deltaTime;
    }

    velocity = glm::vec3(0.0f);
}

AABB Player::getPlayerAABB(const glm::vec3& pos) const {
    AABB box;
    box.min = pos - size;
    box.max = pos + size;
    return box;
}

bool Player::checkCollision(const glm::vec3& pos, const AABB& box) {
    glm::vec3 pMin = getMin(pos);
    glm::vec3 pMax = getMax(pos);

    return (pMin.x < box.max.x && pMax.x > box.min.x &&
        pMin.y < box.max.y && pMax.y > box.min.y &&
        pMin.z < box.max.z && pMax.z > box.min.z);
}

bool Player::checkCollisionMesh(const glm::vec3& pos) const {
    if (!meshCollider) return false;
    AABB box = getPlayerAABB(pos);
    return meshCollider->intersectAABB(box);
}

bool Player::checkOnGround() const {
    if (!meshCollider) return false;
    glm::vec3 testPos = position;
    testPos.y -= 0.01f;
    return checkCollisionMesh(testPos);
}

// ИСПРАВЛЕНО: добавлен deltaTime параметр
bool Player::tryStepUp(const glm::vec3& fromPos, const glm::vec3& moveDir, float maxStepHeight, float deltaTime) {
    if (!meshCollider) return false;

    glm::vec3 horizontalTestPos = fromPos;
    horizontalTestPos.x += moveDir.x;
    horizontalTestPos.z += moveDir.z;

    if (!checkCollisionMesh(horizontalTestPos)) {
        return false;
    }

    float low = 0.0f, high = maxStepHeight;
    float foundHeight = -1.0f;

    for (int i = 0; i < 8; i++) {
        float mid = (low + high) * 0.5f;
        glm::vec3 testPos = fromPos + glm::vec3(moveDir.x, mid, moveDir.z);

        if (checkCollisionMesh(testPos)) {
            low = mid;
        }
        else {
            foundHeight = fromPos.y + mid;
            high = mid;
        }
    }

    if (foundHeight < 0.0f || (foundHeight - fromPos.y) < 0.001f) {
        return false;
    }

    float heightDiff = foundHeight - fromPos.y;
    if (heightDiff > maxStepHeight) return false;

    glm::vec3 headTestPos = fromPos;
    headTestPos.x += moveDir.x;
    headTestPos.z += moveDir.z;
    headTestPos.y = foundHeight + size.y * 2.0f - 0.01f;

    if (checkCollisionMesh(headTestPos)) {
        return false;
    }

    float riseThisFrame = stepUpSpeed * deltaTime;
    float actualRise = std::min(heightDiff, riseThisFrame);

    position.y += actualRise;
    position.x = fromPos.x + moveDir.x;
    position.z = fromPos.z + moveDir.z;

    if (actualRise >= heightDiff - 0.001f) {
        position.y = foundHeight;
        onGround = true;
        velocity.y = 0;
    }
    else {
        onGround = false;
    }

    return true;
}

void Player::resolveCollisionAxis(float deltaTime, int axis) {
    glm::vec3 newPos = position;

    if (axis == 0) newPos.x += velocity.x * deltaTime;
    else if (axis == 1) newPos.y += velocity.y * deltaTime;
    else newPos.z += velocity.z * deltaTime;

    if (checkCollisionMesh(newPos)) {
        if (axis == 0) {
            glm::vec3 moveDir(velocity.x * deltaTime, 0.0f, 0.0f);
            // ИСПРАВЛЕНО: передаем deltaTime
            if (!tryStepUp(position, moveDir, stepHeight, deltaTime)) {
                velocity.x = 0;
            }
            else {
                velocity.x *= 0.5f;
            }
        }
        else if (axis == 1) {
            if (velocity.y > 0) {
                velocity.y = 0;
            }
            else if (velocity.y < 0) {
                onGround = true;
                velocity.y = 0;
            }
        }
        else {
            glm::vec3 moveDir(0.0f, 0.0f, velocity.z * deltaTime);
            // ИСПРАВЛЕНО: передаем deltaTime
            if (!tryStepUp(position, moveDir, stepHeight, deltaTime)) {
                velocity.z = 0;
            }
            else {
                velocity.z *= 0.5f;
            }
        }
        return;
    }

    if (axis == 0) position.x = newPos.x;
    else if (axis == 1) position.y = newPos.y;
    else position.z = newPos.z;
}

void Player::moveWithCollision(float deltaTime) {
    bool groundBelow = checkOnGround();

    if (onGround && !groundBelow && velocity.y >= 0) {
        onGround = false;
    }

    resolveCollisionAxis(deltaTime, 0);
    resolveCollisionAxis(deltaTime, 2);

    resolveCollisionAxis(deltaTime, 1);

    if (!onGround && velocity.y < 0) {
        if (checkOnGround()) {
            onGround = true;
            velocity.y = 0;
        }
    }

    if (onGround && velocity.y == 0) {
        glm::vec3 testPos = position;
        float groundHeight = -9999.0f;
        
        // Используем именованные константы вместо магических чисел
        constexpr int GROUND_SAMPLES = 60;
        constexpr float SAMPLE_STEP = 0.005f;
        constexpr float GROUND_EPSILON = 0.001f;
        constexpr float MAX_GROUND_DIFF = 0.1f;

        for (int i = 0; i < GROUND_SAMPLES; i++) {
            testPos.y -= SAMPLE_STEP;
            if (checkCollisionMesh(testPos)) {
                groundHeight = testPos.y + SAMPLE_STEP + GROUND_EPSILON;
                break;
            }
        }

        if (groundHeight > -9000.0f) {
            float diff = position.y - groundHeight;
            if (diff > GROUND_EPSILON && diff < MAX_GROUND_DIFF) {
                position.y = groundHeight;
            }
        }
    }
}

void Player::update(float deltaTime, float cameraYaw, float cameraPitch, const MeshCollider* collider) {
    yaw = cameraYaw;
    pitch = cameraPitch;
    meshCollider = collider;

    handleInput(deltaTime);

    if (noclipMode) {
        moveNoclip(deltaTime);
    }
    else {
        if (!onGround) {
            velocity.y -= gravity * deltaTime;
        }
        moveWithCollision(deltaTime);
    }
}

glm::vec3 Player::getEyePosition() const {
    return glm::vec3(position.x, position.y + 0.8f, position.z);
}

void Player::getViewMatrix(float* matrix) const {
    glm::vec3 eye = getEyePosition();
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    glm::mat4 view = glm::lookAt(eye, eye + front, glm::vec3(0.0f, 1.0f, 0.0f));
    memcpy(matrix, glm::value_ptr(view), sizeof(glm::mat4));
}