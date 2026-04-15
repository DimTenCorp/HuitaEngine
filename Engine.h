#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include "Menu.h"
#include "SkyboxRenderer.h"
#include "DoorEntity.h"

constexpr float DEFAULT_LIGHTMAP_INTENSITY = 2.0f;

class Game;
class BSPLoader;
class WADLoader;
class Renderer;
class LightmappedRenderer;
class LightmapManager;
class MeshCollider;
class Menu;
class CFuncWater;
class Player;

class Engine {
public:
    Engine();
    ~Engine();

    bool init();
    void run();
    void shutdown();

    GLFWwindow* getWindow() const { return window; }
    static Engine* getInstance() { return instance; }

    Game* getGame() const { return game.get(); }
    BSPLoader* getBSP() const { return bspLoader.get(); }
    Renderer* getRenderer() const { return renderer.get(); }
    LightmappedRenderer* getLMRenderer() const { return lmRenderer.get(); }
    LightmapManager* getLightmapManager() const { return lightmapManager.get(); }
    MeshCollider* getCollider() const { return meshCollider.get(); }
    Menu* getMenu() const { return menu.get(); }
    const std::vector<CFuncWater*>& getWaterZones() const { return waterZones; }

    bool useLightmappedRenderer() const { return useLightmapped; }
    void toggleLightmappedRenderer();
    void setShowLightmapsOnly(bool show);
    void setLightmapIntensity(float intensity);
    float getLightmapIntensity() const { return lightmapIntensity; }

    void showMenu();
    void hideMenu();
    bool isMenuActive() const { return menuActive; }

    float getDeltaTime() const { return deltaTime; }

    void loadMap(const std::string& mapPath);
    void unloadCurrentMap();

    int getWidth() const { return width; }
    int getHeight() const { return height; }

    void onFramebufferSize(int width, int height);

    void setLightingEnabled(bool enabled);

    void applySettings(const SettingsData& settings);

    void updateDoors(float deltaTime);
    void checkDoorInteractions(Player* player);
    const std::vector<std::unique_ptr<DoorEntity>>& getDoors() const { return doors; }
    void renderDoors(const glm::mat4& view, const glm::mat4& projection);
    void cleanupDoors();
    
    // Буфер для трансформированных треугольников дверей (переиспользуется каждый кадр)
    std::vector<Triangle> doorTrianglesBuffer;

private:
    static Engine* instance;

    std::vector<std::unique_ptr<DoorEntity>> doors;

    GLFWwindow* window = nullptr;
    int width = 1280;
    int height = 720;

    std::unique_ptr<Game> game;
    std::unique_ptr<BSPLoader> bspLoader;
    std::unique_ptr<WADLoader> wadLoader;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<LightmappedRenderer> lmRenderer;
    std::unique_ptr<LightmapManager> lightmapManager;
    std::unique_ptr<MeshCollider> meshCollider;
    std::unique_ptr<Menu> menu;
    std::unique_ptr<SkyboxRenderer> skyboxRenderer;
    std::vector<CFuncWater*> waterZones;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    bool useLightmapped = false;
    bool showLightmapsOnly = false;
    float lightmapIntensity = DEFAULT_LIGHTMAP_INTENSITY;
    bool menuActive = true;

    bool pendingLoad = false;
    std::string pendingMapPath;
    bool mapLoadInProgress = false;

    SettingsData currentSettings;

    bool initGLFW();
    bool initGLAD();
    bool initImGui();
    bool initSystems();

    void updateTime();
    void render();
    void processPendingLoad();
    void doLoadMap(const std::string& mapPath);
};