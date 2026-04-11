#pragma once
#include <string>
#include <vector>
#include <functional>
#include <GLFW/glfw3.h>

class Menu {
public:
    enum class State {
        MAIN_MENU,
        MAP_SELECT,
        CONFIRM_EXIT,
        LOADING
    };

    Menu();
    ~Menu();

    // Инициализация - вызывать ОДИН РАЗ за сессию
    void init(GLFWwindow* window, int width, int height);

    // Сброс состояния при возврате в меню
    void reset();

    void update(float deltaTime);
    void render();

    void setActive(bool active);
    bool isActive() const { return active; }

    void setState(State state) { currentState = state; }
    State getState() const { return currentState; }

    // Callbacks
    void setOnMapSelected(std::function<void(const std::string&)> callback);
    void setOnExitGame(std::function<void()> callback);
    void setOnReturnToGame(std::function<void()> callback);

    // Input
    void handleMouseButton(int button, int action, double x, double y);
    void handleMouseMove(double x, double y);
    void handleKey(int key, int action);
    void handleScroll(double xoffset, double yoffset);

    // Maps
    void scanMaps(const std::string& directory);
    const std::vector<std::string>& getMaps() const { return maps; }
    void selectMap(int index);

    // Для Engine
    bool shouldReturnToMenu() const { return returnToMenu; }
    void clearReturnToMenuFlag() { returnToMenu = false; }

    // Для экрана загрузки
    void showLoading(const std::string& mapName);
    void hideLoading();
    bool isLoading() const { return currentState == State::LOADING; }

    // Шрифт - устанавливается ДО init()
    void setFontPath(const std::string& path) { fontPath = path; }

private:
    GLFWwindow* window = nullptr;
    int width = 1280;
    int height = 720;
    bool active = true;
    State currentState = State::MAIN_MENU;
    std::string fontPath;

    bool initialized = false;

    // Списки карт
    std::vector<std::string> maps;
    int selectedMapIndex = -1;
    double lastClickTime = 0.0;
    int lastClickedMap = -1;
    static constexpr double DOUBLE_CLICK_TIME = 0.3;

    // Callbacks
    std::function<void(const std::string&)> onMapSelected;
    std::function<void()> onExitGame;
    std::function<void()> onReturnToGame;

    // Флаг для Engine
    bool returnToMenu = false;

    // Рендеринг состояний
    void renderMainMenu();
    void renderMapSelect();
    void renderConfirmExit();
    void renderLoading();  // <-- НОВЫЙ МЕТОД

    // Input state
    double mouseX = 0, mouseY = 0;
    bool mousePressed = false;

    // Для экрана загрузки
    std::string loadingMapName;
};