#pragma once
#include "gl_includes.h"
#include "Player.h"
#include "HUD.h"
#include "BSPLoader.h"
#include "TriangleCollider.h"

class GameManager {
public:
    static GameManager& instance();
    bool initialize();
    void run();
    void shutdown();

    Player& getPlayer() { return player; }
    HUD& getHUD() { return hud; }
    BSPLoader& getBSPLoader() { return bspLoader; }
    MeshCollider& getCollider() { return meshCollider; }
    GLFWwindow* getWindow() const { return window; }

    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    void setYaw(float y) { yaw = y; }
    void setPitch(float p) { pitch = p; }
    bool isHitboxVisible() const { return showPlayerHitbox; }
    void toggleHitbox() { showPlayerHitbox = !showPlayerHitbox; }
    float getDeltaTime() const { return deltaTime; }

private:
    GameManager() = default;
    ~GameManager() = default;
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    bool initOpenGL();
    bool loadGameContent();
    void setupPlayerSpawn();
    void update(float dt);  // ✅ Было пропущено
    void render();          // ✅ Было пропущено

    GLFWwindow* window = nullptr;
    Player player;
    HUD hud;
    BSPLoader bspLoader;
    MeshCollider meshCollider;

    float yaw = -90.0f;
    float pitch = 0.0f;
    bool showPlayerHitbox = true;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
};