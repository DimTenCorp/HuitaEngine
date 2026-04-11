#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include "Menu.h"

class Game;
class BSPLoader;
class WADLoader;
class Renderer;
class LightmappedRenderer;
class LightmapManager;
class MeshCollider;
class Menu;

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

    bool useLightmappedRenderer() const { return useLightmapped; }
    void toggleLightmappedRenderer() { useLightmapped = !useLightmapped; }
    void setShowLightmapsOnly(bool show);
    void setLightmapIntensity(float intensity);

    void showMenu();
    void hideMenu();
    bool isMenuActive() const { return menuActive; }

    float getDeltaTime() const { return deltaTime; }

    void loadMap(const std::string& mapPath);
    void unloadCurrentMap();  // <-- мнбши лернд

private:
    static Engine* instance;

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

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    bool useLightmapped = false;
    bool showLightmapsOnly = false;
    float lightmapIntensity = 1.0f;
    bool menuActive = true;

    bool initGLFW();
    bool initGLAD();
    bool initImGui();
    bool initSystems();

    void updateTime();
    void render();

    void handleMenuReturn();
};