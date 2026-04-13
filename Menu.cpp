#include "pch.h"
#include "Menu.h"
#include "imgui.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include "Localization.h"

namespace fs = std::filesystem;

static const float MENU_FONT_SIZE = 20.0f;
static const float TAB_BAR_HEIGHT = 50.0f;

Menu::Menu() = default;
Menu::~Menu() = default;

void Menu::init(GLFWwindow* win, int w, int h) {
    if (initialized) {
        window = win;
        width = w;
        height = h;
        reset();
        return;
    }

    window = win;
    width = w;
    height = h;

    loadSettings();
    refreshDisplayModes();
    settingsSensitivity = settings.mouseSensitivity;
    settingsTab = 0;
    pendingApplySettings = false;

    // Инициализация локализации - загружаем русский язык
    Localization::getInstance().loadLanguage("res/dtc_russian.txt");

    reset();

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = nullptr;

    ImFont* customFont = nullptr;
    bool fontLoaded = false;

    if (!fontPath.empty() && fs::exists(fontPath)) {
        customFont = io.Fonts->AddFontFromFileTTF(
            fontPath.c_str(),
            MENU_FONT_SIZE,
            nullptr,
            io.Fonts->GetGlyphRangesCyrillic()
        );

        if (customFont) {
            std::cout << "[MENU] Loaded custom font: " << fontPath << std::endl;
            fontLoaded = true;
        }
        else {
            std::cerr << "[MENU] Failed to load custom font: " << fontPath << std::endl;
        }
    }

    if (!fontLoaded) {
        const char* systemFont = "C:\\Windows\\Fonts\\Arial.ttf";
        if (fs::exists(systemFont)) {
            customFont = io.Fonts->AddFontFromFileTTF(
                systemFont,
                MENU_FONT_SIZE,
                nullptr,
                io.Fonts->GetGlyphRangesCyrillic()
            );
            if (customFont) {
                std::cout << "[MENU] Loaded system Arial" << std::endl;
                fontLoaded = true;
            }
        }
    }

    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
        std::cerr << "[MENU] Using default ImGui font (no Cyrillic support!)" << std::endl;
    }
    else {
        io.FontDefault = customFont;
    }

    io.Fonts->Build();

    std::cout << "[MENU] Fonts count: " << io.Fonts->Fonts.size() << std::endl;
    std::cout << "[MENU] Default font: " << (io.FontDefault ? "set" : "null") << std::endl;

    initialized = true;
}

void Menu::reset() {
    currentState = State::MAIN_MENU;
    selectedMapIndex = -1;
    lastClickTime = 0.0;
    lastClickedMap = -1;
    returnToMenu = false;
    mousePressed = false;
    loadingMapName.clear();
    settingsTab = 0;
    pendingApplySettings = false;
}

void Menu::setActive(bool act) {
    active = act;
    if (active) {
        reset();
    }
}

void Menu::scanMaps(const std::string& directory) {
    maps.clear();
    selectedMapIndex = -1;

    if (!fs::exists(directory)) {
        std::cerr << "[MENU] Maps directory not found: " << directory << std::endl;
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".bsp") {
                    maps.push_back(entry.path().filename().string());
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[MENU] Error scanning maps: " << e.what() << std::endl;
        return;
    }

    std::sort(maps.begin(), maps.end());
    std::cout << "[MENU] Found " << maps.size() << " maps" << std::endl;
}

void Menu::update(float deltaTime) {
    (void)deltaTime;
}

void Menu::handleMouseMove(double x, double y) {
    mouseX = x;
    mouseY = y;
}

void Menu::handleMouseButton(int button, int action, double x, double y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    mouseX = x;
    mouseY = y;
    mousePressed = (action == GLFW_PRESS);
}

void Menu::handleKey(int key, int action) {
    if (action != GLFW_PRESS) return;

    if (currentState == State::MAP_SELECT) {
        if (key == GLFW_KEY_ESCAPE) {
            currentState = State::MAIN_MENU;
            selectedMapIndex = -1;
        }
    }
    else if (currentState == State::CONFIRM_EXIT) {
        if (key == GLFW_KEY_ESCAPE) {
            if (onReturnToGame) onReturnToGame();
        }
    }
    else if (currentState == State::CONFIRM_QUIT) {
        if (key == GLFW_KEY_ESCAPE) {
            currentState = State::MAIN_MENU;
        }
    }
    else if (currentState == State::SETTINGS) {
        if (key == GLFW_KEY_ESCAPE) {
            loadSettings();
            settingsSensitivity = settings.mouseSensitivity;
            currentState = State::MAIN_MENU;
        }
    }
    // <-- НОВОЕ: ESC в паузе = вернуться в игру
    else if (currentState == State::PAUSE) {
        if (key == GLFW_KEY_ESCAPE) {
            if (onReturnToGame) onReturnToGame();
        }
    }

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (currentState == State::PAUSE) {
            // Закрываем паузу
            setActive(false);
            return;
        }
    }
}

void Menu::handleScroll(double xoffset, double yoffset) {
    (void)xoffset;
    (void)yoffset;
}

void Menu::showLoading(const std::string& mapName) {
    loadingMapName = mapName;
    currentState = State::LOADING;
}

void Menu::hideLoading() {
    currentState = State::MAIN_MENU;
    loadingMapName.clear();
}

void Menu::refreshDisplayModes() {
    availableModes = SettingsData::getAvailableDisplayModes();

    selectedModeIndex = -1;
    for (size_t i = 0; i < availableModes.size(); i++) {
        if (availableModes[i].width == settings.screenWidth &&
            availableModes[i].height == settings.screenHeight) {
            selectedModeIndex = (int)i;
            break;
        }
    }

    if (selectedModeIndex == -1 && settings.screenWidth > 0 && settings.screenHeight > 0) {
        availableModes.insert(availableModes.begin(),
            DisplayMode(settings.screenWidth, settings.screenHeight));
        selectedModeIndex = 0;
    }
}

void Menu::applySettings() {
    if (window) {
        GLFWmonitor* monitor = settings.fullscreen ? glfwGetPrimaryMonitor() : nullptr;

        if (settings.fullscreen && monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0,
                settings.screenWidth, settings.screenHeight, mode->refreshRate);
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
        }
        else {
            // Декорируем окно (рамка для перетаскивания)
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);

            // Получаем размеры монитора для центрирования
            GLFWmonitor* primary = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(primary);
            int monitorWidth = mode->width;
            int monitorHeight = mode->height;

            int windowX = (monitorWidth - settings.screenWidth) / 2;
            int windowY = (monitorHeight - settings.screenHeight) / 2;

            glfwSetWindowMonitor(window, nullptr, windowX, windowY,
                settings.screenWidth, settings.screenHeight, 0);
        }

        glfwGetFramebufferSize(window, &width, &height);
    }

    if (onSettingsChanged) {
        onSettingsChanged(settings);
    }

    std::cout << "[MENU] Applied settings: "
        << settings.screenWidth << "x" << settings.screenHeight
        << " fullscreen=" << settings.fullscreen
        << " sensitivity=" << settings.mouseSensitivity << std::endl;
}

void Menu::saveSettings() {
    settings.mouseSensitivity = settingsSensitivity;
    settings.save();
    std::cout << "[MENU] Settings saved" << std::endl;
}

void Menu::loadSettings() {
    settings.load();
    settingsSensitivity = settings.mouseSensitivity;
    std::cout << "[MENU] Settings loaded: sensitivity=" << settingsSensitivity << std::endl;
}

void Menu::render() {
    if (!active) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    bool open = true;
    ImGui::Begin("Menu", &open,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Фон ТОЛЬКО для главного меню и настроек (когда НЕ пауза)
    if (currentState != State::PAUSE && currentState != State::LOADING) {
        drawList->AddRectFilled(
            ImVec2(0, 0),
            ImVec2((float)width, (float)height),
            IM_COL32(0, 0, 0, 180)
        );
    }
    // Для PAUSE — полупрозрачный чёрный фон (темнее, чем в меню)
    else if (currentState == State::PAUSE) {
        drawList->AddRectFilled(
            ImVec2(0, 0),
            ImVec2((float)width, (float)height),
            IM_COL32(0, 0, 0, 120)  // Полупрозрачный чёрный, менее затемнённый чем в меню
        );
    }

    switch (currentState) {
    case State::MAIN_MENU: renderMainMenu(); break;
    case State::MAP_SELECT: renderMapSelect(); break;
    case State::CONFIRM_EXIT: renderConfirmExit(); break;
    case State::CONFIRM_QUIT: renderConfirmQuit(); break;
    case State::LOADING: renderLoading(); break;
    case State::SETTINGS: renderSettings(); break;
    case State::PAUSE: renderPause(); break;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void Menu::renderPause() {
    float buttonX = 50.0f;
    float startY = height - 200.0f;
    float buttonHeight = 35.0f;
    float spacing = 8.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, spacing));

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 100, 80));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(150, 150, 150, 100));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 200));

    ImGui::SetCursorPos(ImVec2(buttonX, startY));
    if (ImGui::Button(tr("#dtc_resume").c_str(), ImVec2(0, buttonHeight))) {
        if (onReturnToGame) onReturnToGame();
    }

    ImGui::SetCursorPos(ImVec2(buttonX, startY + buttonHeight + spacing));
    if (ImGui::Button(tr("#dtc_settings").c_str(), ImVec2(0, buttonHeight))) {
        previousState = State::PAUSE;  // <-- ЗАПОМИНАЕМ что мы были в паузе
        currentState = State::SETTINGS;
        settingsSensitivity = settings.mouseSensitivity;
        refreshDisplayModes();
    }

    ImGui::SetCursorPos(ImVec2(buttonX, startY + (buttonHeight + spacing) * 2));
    if (ImGui::Button(tr("#dtc_mainmenu").c_str(), ImVec2(0, buttonHeight))) {
        currentState = State::CONFIRM_EXIT;
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}

void Menu::renderMainMenu() {
    float buttonX = 50.0f;
    float startY = height - 200.0f;
    float buttonHeight = 35.0f;
    float spacing = 8.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, spacing));

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 100, 80));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(150, 150, 150, 100));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 200));

    ImGui::SetCursorPos(ImVec2(buttonX, startY));
    if (ImGui::Button(tr("#dtc_newgame").c_str(), ImVec2(0, buttonHeight))) {
        currentState = State::MAP_SELECT;
    }

    ImGui::SetCursorPos(ImVec2(buttonX, startY + buttonHeight + spacing));
    if (ImGui::Button(tr("#dtc_settings").c_str(), ImVec2(0, buttonHeight))) {
        previousState = State::MAIN_MENU;  // <-- ЗАПОМИНАЕМ что мы были в главном меню
        currentState = State::SETTINGS;
        settingsSensitivity = settings.mouseSensitivity;
        refreshDisplayModes();
    }

    ImGui::SetCursorPos(ImVec2(buttonX, startY + (buttonHeight + spacing) * 2));
    if (ImGui::Button(tr("#dtc_exit").c_str(), ImVec2(0, buttonHeight))) {
        currentState = State::CONFIRM_QUIT;
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}

void Menu::renderSettings() {
    float panelX = width * 0.2f;
    float panelY = 80.0f;
    float panelWidth = width * 0.6f;
    float panelHeight = height - 160.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelWidth, panelY + panelHeight),
        IM_COL32(30, 30, 40, 220),
        10.0f
    );
    drawList->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelWidth, panelY + panelHeight),
        IM_COL32(80, 80, 100, 255),
        10.0f, 0, 2.0f
    );

    ImGui::SetCursorPos(ImVec2(panelX + 20, panelY + 15));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::Text(tr("#dtc_settings").c_str());
    ImGui::PopStyleColor();

    float tabWidth = 120.0f;
    float tabY = panelY + 50.0f;

    ImGui::SetCursorPos(ImVec2(panelX + 20, tabY));
    ImGui::PushStyleColor(ImGuiCol_Button, settingsTab == 0 ? IM_COL32(80, 80, 120, 255) : IM_COL32(50, 50, 70, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 140, 255));
    if (ImGui::Button(tr("#dtc_game").c_str(), ImVec2(tabWidth, 35))) {
        settingsTab = 0;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 5);
    ImGui::SetCursorPosX(panelX + 20 + tabWidth + 5);
    ImGui::PushStyleColor(ImGuiCol_Button, settingsTab == 1 ? IM_COL32(80, 80, 120, 255) : IM_COL32(50, 50, 70, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 140, 255));
    if (ImGui::Button(tr("#dtc_graphics").c_str(), ImVec2(tabWidth, 35))) {
        settingsTab = 1;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 5);
    ImGui::SetCursorPosX(panelX + 20 + tabWidth * 2 + 10);
    ImGui::PushStyleColor(ImGuiCol_Button, settingsTab == 2 ? IM_COL32(80, 80, 120, 255) : IM_COL32(50, 50, 70, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 140, 255));
    if (ImGui::Button(tr("#dtc_sound").c_str(), ImVec2(tabWidth, 35))) {
        settingsTab = 2;
    }
    ImGui::PopStyleColor(2);

    float contentY = tabY + 50.0f;
    float contentHeight = panelHeight - 140.0f;

    ImGui::SetCursorPos(ImVec2(panelX + 20, contentY));
    ImGui::BeginChild("SettingsContent", ImVec2(panelWidth - 40, contentHeight), false);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 220, 220, 255));

    if (settingsTab == 0) {
        ImGui::Text(tr("#dtc_controls").c_str());
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text(tr("#dtc_mouse_sensitivity").c_str());
        float oldSens = settingsSensitivity;
        ImGui::SliderFloat("##sensitivity", &settingsSensitivity, 0.1f, 2.0f, "%.2f");

        ImGui::SameLine();
        ImGui::Text(" (%.2f)", settingsSensitivity);

        ImGui::Spacing();
        ImGui::Spacing();
    }
    else if (settingsTab == 1) {
        ImGui::Text(tr("#dtc_display").c_str());
        ImGui::Separator();
        ImGui::Spacing();

        bool fsChanged = false;
        bool currentFullscreen = settings.fullscreen;
        if (ImGui::Checkbox(tr("#dtc_fullscreen").c_str(), &currentFullscreen)) {
            settings.fullscreen = currentFullscreen;
            fsChanged = true;
        }

        ImGui::Spacing();

        if (!availableModes.empty()) {
            ImGui::Text(tr("#dtc_resolution").c_str());

            std::vector<const char*> modeNames;
            for (const auto& mode : availableModes) {
                modeNames.push_back(mode.name.c_str());
            }

            int currentMode = selectedModeIndex;
            if (ImGui::Combo(u8"##resolution", &currentMode, modeNames.data(), (int)modeNames.size())) {
                if (currentMode >= 0 && currentMode < (int)availableModes.size()) {
                    settings.screenWidth = availableModes[currentMode].width;
                    settings.screenHeight = availableModes[currentMode].height;
                    selectedModeIndex = currentMode;
                    settings.displayModeIndex = currentMode;
                    fsChanged = true;
                }
            }
        }

        ImGui::Spacing();
        //ImGui::Text(u8"Текущее разрешение: %dx%d", settings.screenWidth, settings.screenHeight);
        ImGui::Text(tr("#dtc_current_res").c_str(), settings.screenWidth, settings.screenHeight);

        if (fsChanged) {
            pendingApplySettings = true;
        }
    }
    else if (settingsTab == 2) {
        ImGui::Text(tr("#dtc_audio").c_str());
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text(tr("#dtc_volume").c_str());
        float volume = settings.masterVolume;
        if (ImGui::SliderFloat(u8"##volume", &volume, 0.0f, 1.0f, "%.0f%%")) {
            settings.masterVolume = volume;
        }

        ImGui::Spacing();

        bool mute = settings.mute;
        if (ImGui::Checkbox(tr("#dtc_nosound").c_str(), &mute)) {
            settings.mute = mute;
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text(tr("#dtc_sound_description").c_str());
        
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();

    float buttonY = panelY + panelHeight - 55.0f;
    float buttonWidth = 150.0f;

    // Кнопка СОХРАНИТЬ
    ImGui::SetCursorPos(ImVec2(panelX + panelWidth - buttonWidth - 20, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 70, 70, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 90, 90, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(110, 110, 110, 255));
    if (ImGui::Button(tr("#dtc_save").c_str(), ImVec2(buttonWidth, 40))) {
        settings.mouseSensitivity = settingsSensitivity;
        saveSettings();

        // <-- ИСПРАВЛЕНИЕ: ВСЕГДА вызываем applySettings для применения сенсы и других настроек
        if (onSettingsChanged) {
            onSettingsChanged(settings);
        }

        // Применяем настройки дисплея только если они менялись
        if (pendingApplySettings) {
            applySettings();  // Это для смены разрешения/полноэкранного режима
            pendingApplySettings = false;
        }

        currentState = previousState;
    }
    ImGui::PopStyleColor(3);

    // Кнопка ОТМЕНА
    ImGui::SetCursorPos(ImVec2(panelX + 20, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 70, 70, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 90, 90, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(110, 110, 110, 255));
    if (ImGui::Button(tr("#dtc_cancel").c_str(), ImVec2(buttonWidth, 40))) {
        loadSettings();
        settingsSensitivity = settings.mouseSensitivity;
        currentState = previousState;
    }
    ImGui::PopStyleColor(3);
}

void Menu::renderMapSelect() {
    ImVec2 center = ImVec2(width * 0.5f, 100.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y));
    ImGui::Text(tr("#dtc_select_map").c_str());
    ImGui::PopStyleColor();

    float listX = width * 0.5f - 200.0f;
    float listY = 180.0f;
    float listWidth = 400.0f;
    float listHeight = height - 300.0f;
    float itemHeight = 30.0f;

    ImGui::SetCursorPos(ImVec2(listX, listY));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));

    ImGui::BeginChild("MapList", ImVec2(listWidth, listHeight), false);

    for (int i = 0; i < (int)maps.size(); i++) {
        bool isSelected = (i == selectedMapIndex);

        std::string displayName = maps[i];
        size_t dotPos = displayName.find_last_of('.');
        if (dotPos != std::string::npos) {
            displayName = displayName.substr(0, dotPos);
        }

        ImU32 headerColor, headerHoveredColor, textColor;
        if (isSelected) {
            headerColor = IM_COL32(200, 200, 200, 100);
            headerHoveredColor = IM_COL32(220, 220, 220, 150);
            textColor = IM_COL32(255, 255, 255, 255);
        }
        else {
            headerColor = IM_COL32(0, 0, 0, 0);
            headerHoveredColor = IM_COL32(100, 100, 100, 100);
            textColor = IM_COL32(180, 180, 180, 255);
        }

        ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        ImGui::SetCursorPosX(10);
        bool clicked = ImGui::Selectable(displayName.c_str(), isSelected,
            ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(listWidth - 20, itemHeight));

        if (clicked) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                std::string fullPath = "maps/" + maps[i];
                showLoading(displayName);
                if (onMapSelected) {
                    onMapSelected(fullPath);
                }
            }
            else {
                selectedMapIndex = i;
                lastClickTime = glfwGetTime();
                lastClickedMap = i;
            }
        }

        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    float buttonY = height - 80.0f;
    float buttonWidth = 150.0f;
    float buttonHeight = 40.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    ImGui::SetCursorPos(ImVec2(width * 0.5f - 160.0f, buttonY));
    if (selectedMapIndex >= 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
        if (ImGui::Button(tr("#dtc_load").c_str(), ImVec2(buttonWidth, buttonHeight))) {
            std::string displayName = maps[selectedMapIndex];
            size_t dotPos = displayName.find_last_of('.');
            if (dotPos != std::string::npos) {
                displayName = displayName.substr(0, dotPos);
            }
            showLoading(displayName);
            if (onMapSelected) {
                onMapSelected("maps/" + maps[selectedMapIndex]);
            }
        }
        ImGui::PopStyleColor(2);
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 50, 50, 150));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50, 50, 50, 150));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 50, 50, 150));
        ImGui::Button(tr("#dtc_load").c_str(), ImVec2(buttonWidth, buttonHeight));
        ImGui::PopStyleColor(3);
    }

    ImGui::SetCursorPos(ImVec2(width * 0.5f + 10.0f, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 60, 60, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 100, 100, 200));
    if (ImGui::Button(tr("#dtc_back").c_str(), ImVec2(buttonWidth, buttonHeight))) {
        currentState = State::MAIN_MENU;
        selectedMapIndex = -1;
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();

    ImGui::SetCursorPos(ImVec2(width * 0.5f - 150, height - 30.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 120, 120, 255));
    ImGui::Text(tr("#dtc_fast_map_load").c_str());
    ImGui::PopStyleColor();
}

// Диалог подтверждения выхода в главное меню (из игры)
void Menu::renderConfirmExit() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(
        ImVec2(0, 0),
        ImVec2((float)width, (float)height),
        IM_COL32(0, 0, 0, 200)
    );

    float dialogWidth = 400.0f;
    float dialogHeight = 150.0f;
    float dialogX = (width - dialogWidth) * 0.5f;
    float dialogY = (height - dialogHeight) * 0.5f;

    drawList->AddRectFilled(
        ImVec2(dialogX, dialogY),
        ImVec2(dialogX + dialogWidth, dialogY + dialogHeight),
        IM_COL32(50, 50, 50, 255)
    );
    drawList->AddRect(
        ImVec2(dialogX, dialogY),
        ImVec2(dialogX + dialogWidth, dialogY + dialogHeight),
        IM_COL32(120, 120, 120, 255),
        0.0f, 0, 2.0f
    );

    ImGui::SetCursorPos(ImVec2(dialogX + 20, dialogY + 30));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::Text(tr("#dtc_quit_to_menu").c_str());
    ImGui::PopStyleColor();

    float buttonY = dialogY + 90;
    float buttonWidth = 120.0f;
    float buttonHeight = 35.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    ImGui::SetCursorPos(ImVec2(dialogX + 60, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
    if (ImGui::Button(tr("#dtc_yes").c_str(), ImVec2(buttonWidth, buttonHeight))) {
        returnToMenu = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(dialogX + dialogWidth - 60 - buttonWidth, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
    if (ImGui::Button(tr("#dtc_no").c_str(), ImVec2(buttonWidth, buttonHeight))) {
        if (onReturnToGame) onReturnToGame();
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();
}

// Новый диалог подтверждения полного выхода из игры
void Menu::renderConfirmQuit() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(
        ImVec2(0, 0),
        ImVec2((float)width, (float)height),
        IM_COL32(0, 0, 0, 200)
    );

    float dialogWidth = 400.0f;
    float dialogHeight = 150.0f;
    float dialogX = (width - dialogWidth) * 0.5f;
    float dialogY = (height - dialogHeight) * 0.5f;

    drawList->AddRectFilled(
        ImVec2(dialogX, dialogY),
        ImVec2(dialogX + dialogWidth, dialogY + dialogHeight),
        IM_COL32(50, 50, 50, 255)
    );
    drawList->AddRect(
        ImVec2(dialogX, dialogY),
        ImVec2(dialogX + dialogWidth, dialogY + dialogHeight),
        IM_COL32(120, 120, 120, 255),
        0.0f, 0, 2.0f
    );

    ImGui::SetCursorPos(ImVec2(dialogX + 20, dialogY + 30));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::Text(tr("#dtc_quit_game").c_str());
    ImGui::PopStyleColor();

    float buttonY = dialogY + 90;
    float buttonWidth = 120.0f;
    float buttonHeight = 35.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    ImGui::SetCursorPos(ImVec2(dialogX + 60, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
    if (ImGui::Button(tr("#dtc_yes", "ДА").c_str(), ImVec2(buttonWidth, buttonHeight))) {
        if (onQuitGame) onQuitGame();
    }
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(dialogX + dialogWidth - 60 - buttonWidth, buttonY));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 80, 80, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 120, 120, 200));
    if (ImGui::Button(tr("#dtc_no", "НЕТ").c_str(), ImVec2(buttonWidth, buttonHeight))) {
        currentState = State::MAIN_MENU;
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();
}

void Menu::renderLoading() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(
        ImVec2(0, 0),
        ImVec2((float)width, (float)height),
        IM_COL32(10, 10, 15, 255)
    );

    float boxWidth = 200.0f;
    float boxHeight = 60.0f;
    float boxX = width - boxWidth - 30.0f;
    float boxY = height - boxHeight - 30.0f;
    float rounding = 10.0f;

    drawList->AddRectFilled(
        ImVec2(boxX + 2, boxY + 2),
        ImVec2(boxX + boxWidth + 2, boxY + boxHeight + 2),
        IM_COL32(0, 0, 0, 100),
        rounding
    );

    drawList->AddRectFilled(
        ImVec2(boxX, boxY),
        ImVec2(boxX + boxWidth, boxY + boxHeight),
        IM_COL32(30, 30, 40, 220),
        rounding
    );

    drawList->AddRect(
        ImVec2(boxX, boxY),
        ImVec2(boxX + boxWidth, boxY + boxHeight),
        IM_COL32(80, 80, 100, 255),
        rounding, 0, 1.5f
    );

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

    static float timer = 0.0f;
    timer += 0.016f;
    int dots = ((int)(timer * 2.0f) % 4);
    std::string loadingText = (tr("#dtc_loading").c_str());
    for (int i = 0; i < dots; i++) loadingText += ".";
    for (int i = dots; i < 3; i++) loadingText += " ";

    ImVec2 textSize = ImGui::CalcTextSize(loadingText.c_str());
    ImGui::SetCursorPos(ImVec2(
        boxX + (boxWidth - textSize.x) * 0.5f,
        boxY + (boxHeight - textSize.y) * 0.5f
    ));
    ImGui::Text("%s", loadingText.c_str());

    if (!loadingMapName.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 180, 200, 255));
        std::string mapText = loadingMapName;
        ImVec2 mapTextSize = ImGui::CalcTextSize(mapText.c_str());
        ImGui::SetCursorPos(ImVec2(
            boxX + (boxWidth - mapTextSize.x) * 0.5f,
            boxY + boxHeight - 22.0f
        ));
        ImGui::Text("%s", mapText.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleColor();
}

void Menu::setOnMapSelected(std::function<void(const std::string&)> callback) {
    onMapSelected = callback;
}

void Menu::setOnExitGame(std::function<void()> callback) {
    onExitGame = callback;
}

void Menu::setOnReturnToGame(std::function<void()> callback) {
    onReturnToGame = callback;
}

void Menu::setOnQuitGame(std::function<void()> callback) {
    onQuitGame = callback;
}

void Menu::selectMap(int index) {
    if (index >= 0 && index < (int)maps.size()) {
        selectedMapIndex = index;
    }
}

std::string Menu::tr(const std::string& key, const std::string& fallback) const {
    return Localization::getInstance().getOrFallback(key, fallback.empty() ? key : fallback);
}