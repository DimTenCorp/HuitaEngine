#pragma once
#include <string>
#include <vector>
#include <functional>
#include <GLFW/glfw3.h>
#include "Settings.h"

class Menu {
public:
    enum class State {
        MAIN_MENU,
        MAP_SELECT,
        CONFIRM_EXIT,   // Диалог подтверждения выхода в главное меню из игры
        CONFIRM_QUIT,   // Диалог подтверждения полного выхода из игры
        LOADING,
        SETTINGS,
        PAUSE           // <-- НОВОЕ: меню паузы во время игры
    };

    Menu();
    ~Menu();

    void init(GLFWwindow* window, int width, int height);
    void reset();
    void update(float deltaTime);
    void render();

    void setActive(bool active);
    bool isActive() const { return active; }

    void setState(State state) { currentState = state; }
    State getState() const { return currentState; }

    void setOnMapSelected(std::function<void(const std::string&)> callback);
    void setOnExitGame(std::function<void()> callback);
    void setOnReturnToGame(std::function<void()> callback);
    void setOnQuitGame(std::function<void()> callback);

    void setOnSettingsChanged(std::function<void(const SettingsData&)> callback) {
        onSettingsChanged = callback;
    }

    void handleMouseButton(int button, int action, double x, double y);
    void handleMouseMove(double x, double y);
    void handleKey(int key, int action);
    void handleScroll(double xoffset, double yoffset);

    void scanMaps(const std::string& directory);
    const std::vector<std::string>& getMaps() const { return maps; }
    void selectMap(int index);

    bool shouldReturnToMenu() const { return returnToMenu; }
    void clearReturnToMenuFlag() { returnToMenu = false; }

    void showLoading(const std::string& mapName);
    void hideLoading();
    bool isLoading() const { return currentState == State::LOADING; }

    void setFontPath(const std::string& path) { fontPath = path; }

    const SettingsData& getSettings() const { return settings; }

    // <-- НОВОЕ: проверяем, находимся ли мы в режиме паузы (игра загружена, меню активно)
    bool isPauseMenu() const { return currentState == State::PAUSE; }

private:
    GLFWwindow* window = nullptr;
    int width = 1280;
    int height = 720;
    bool active = true;
    State currentState = State::MAIN_MENU;
    std::string fontPath;
    bool initialized = false;

    SettingsData settings;
    std::vector<DisplayMode> availableModes;
    int selectedModeIndex = -1;
    float settingsSensitivity = 0.5f;
    bool pendingApplySettings = false;

    int settingsTab = 0;

    std::vector<std::string> maps;
    int selectedMapIndex = -1;
    double lastClickTime = 0.0;
    int lastClickedMap = -1;
    static constexpr double DOUBLE_CLICK_TIME = 0.3;

    std::function<void(const std::string&)> onMapSelected;
    std::function<void()> onExitGame;
    std::function<void()> onReturnToGame;
    std::function<void()> onQuitGame;

    std::function<void(const SettingsData&)> onSettingsChanged;

    bool returnToMenu = false;

    double mouseX = 0, mouseY = 0;
    bool mousePressed = false;
    std::string loadingMapName;

    void renderMainMenu();
    void renderMapSelect();
    void renderConfirmExit();
    void renderConfirmQuit();
    void renderLoading();
    void renderSettings();
    void renderPause();     // <-- НОВОЕ: отрисовка меню паузы

    State previousState = State::MAIN_MENU;

    void applySettings();
    void refreshDisplayModes();
    void saveSettings();
    void loadSettings();
    
    // Локализация
    std::string tr(const std::string& key, const std::string& fallback = "") const;
};