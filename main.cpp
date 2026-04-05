#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
// #include "Mesh.h" // Удалено, если не используется
#include "Shader.h"
#include "Player.h"
#include "HUD.h"
#include "BSPLoader.h"
#include "TriangleCollider.h"

// ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "WADLoader.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Player player;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float yaw = -90.0f;
float pitch = 0.0f;
bool f1Pressed = false, f2Pressed = false, f3Pressed = false, f4Pressed = true, noclipPressed = false;
bool showPlayerHitbox = true;

BSPLoader bspLoader;
MeshCollider meshCollider;
unsigned int bspVAO = 0, bspVBO = 0, bspEBO = 0;
size_t bspIndexCount = 0;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float xoffset = (float)(xpos - lastX);
    float yoffset = (float)(lastY - ypos);
    lastX = (float)xpos; lastY = (float)ypos;
    yaw += xoffset * 0.1f;
    pitch += yoffset * 0.1f;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void processHUDInput(GLFWwindow* window, HUD& hud) {
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && !f1Pressed) { hud.toggleCrosshair(); f1Pressed = true; }
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) f1Pressed = false;
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS && !f2Pressed) { hud.toggleFPS(); f2Pressed = true; }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_RELEASE) f2Pressed = false;
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS && !f3Pressed) { hud.togglePosition(); f3Pressed = true; }
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_RELEASE) f3Pressed = false;
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && !noclipPressed) {
        player.toggleNoclip(); std::cout << "Noclip: " << (player.isNoclip() ? "ON" : "OFF") << "\n"; noclipPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_RELEASE) noclipPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS && !f4Pressed) { showPlayerHitbox = !showPlayerHitbox; f4Pressed = true; }
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_RELEASE) f4Pressed = false;
}

bool initBSPRenderer() {
    if (!bspLoader.isLoaded()) return false;
    const auto& vertices = bspLoader.getMeshVertices();
    const auto& indices = bspLoader.getMeshIndices();
    if (vertices.empty() || indices.empty()) return false;

    meshCollider.buildFromBSP(vertices, bspLoader.getMeshIndices());
    std::cout << "Mesh collider built with " << meshCollider.getTriangleCount() << " triangles\n";

    glGenVertexArrays(1, &bspVAO);
    glGenBuffers(1, &bspVBO);
    glGenBuffers(1, &bspEBO);
    glBindVertexArray(bspVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bspVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bspEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), (void*)offsetof(BSPVertex, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), (void*)offsetof(BSPVertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), (void*)offsetof(BSPVertex, texCoord));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    bspIndexCount = indices.size();
    return true;
}

void cleanupBSP() {
    if (bspVAO) glDeleteVertexArrays(1, &bspVAO);
    if (bspVBO) glDeleteBuffers(1, &bspVBO);
    if (bspEBO) glDeleteBuffers(1, &bspEBO);
    bspLoader.cleanupTextures();
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "HuitaEngine BSP", NULL, NULL);
    if (!window) { std::cerr << "Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to init GLAD\n"; return -1; }

    // List available WAD files (Windows)
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("*.wad", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        std::cout << "\n=== Available WAD files ===\n";
        do {
            std::cout << "  " << findData.cFileName << std::endl;
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
        std::cout << "===========================\n\n";
    }
    else {
        std::cout << "No WAD files found in current directory\n";
    }
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    HUD hud;

    WADLoader wadLoader;  // Создаём WADLoader до BSP

    if (wadLoader.loadQuakePalette("palette.lmp")) {
        std::cout << "Quake palette loaded successfully!" << std::endl;
    }
    else {
        std::cout << "No palette.lmp found, using fallback colors" << std::endl;
    }

    if (bspLoader.load("maps/crossfire.bsp", wadLoader)) {
        initBSPRenderer();

        // Находим спавн точку игрока
        glm::vec3 spawnPos;
        glm::vec3 spawnAngles;

        if (bspLoader.findPlayerStart(spawnPos, spawnAngles)) {
            player.setPosition(spawnPos);

            // Устанавливаем углы камеры (yaw, pitch)
            // В Quake углы: pitch, yaw, roll
            yaw = spawnAngles.y;  // Yaw - горизонтальный поворот
            pitch = spawnAngles.x; // Pitch - вертикальный наклон

            // Обновляем игрока с новыми углами
            player.setYaw(yaw);
            player.setPitch(pitch);

            std::cout << "Player spawned at: "
                << spawnPos.x << ", " << spawnPos.y << ", " << spawnPos.z
                << " with yaw: " << yaw << ", pitch: " << pitch << std::endl;
        }
        else {
            // Fallback: центр карты + немного вверх
            auto bounds = bspLoader.getWorldBounds();
            float centerY = bounds.min.y + (bounds.max.y - bounds.min.y) * 0.5f;
            player.setPosition(glm::vec3(0.0f, centerY + 10.0f, 0.0f));
        }
    }
    else {
        std::cerr << "Failed to load BSP, using fallback\n";
        player.setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    }

    // Shader program with texture support
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoord;
        uniform mat4 mvp;
        uniform mat4 model;
        out vec3 vNormal;
        out vec3 vPos;
        out vec2 vTexCoord;
        void main() {
            vPos = (model * vec4(aPos, 1.0)).xyz;
            vNormal = mat3(transpose(inverse(model))) * aNormal;
            vTexCoord = aTexCoord;
            gl_Position = mvp * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 vNormal;
        in vec3 vPos;
        in vec2 vTexCoord;
        out vec4 FragColor;
        uniform sampler2D uTexture;
        uniform vec3 color;
        uniform vec3 lightDir;
        uniform vec3 viewPos;
        void main() {
            vec4 texColor = texture(uTexture, vTexCoord);
            if (texColor.a < 0.5) discard;
            vec3 norm = normalize(vNormal);
            float diff = max(dot(norm, -lightDir), 0.0);
            vec3 ambient = vec3(0.3);
            vec3 diffuse = texColor.rgb * diff * 0.7;
            float dist = length(viewPos - vPos);
            float fog = exp(-dist * 0.02);
            fog = clamp(fog, 0.0, 1.0);
            vec3 fogColor = vec3(0.1, 0.15, 0.2);
            vec3 result = mix(fogColor, texColor.rgb * (ambient + diffuse), fog);
            FragColor = vec4(result, texColor.a);
        }
    )";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Debug cube for hitbox
    float cubeVertices[] = {
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f
    };
    unsigned int cubeIndices[] = {
        0,1,2,2,3,0, 4,5,6,6,7,4, 4,0,3,3,7,4, 1,5,6,6,2,1, 3,2,6,6,7,3, 4,5,1,1,0,4
    };
    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glm::mat4 projection = glm::perspective(glm::radians(75.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 1000.0f);
    glUseProgram(shaderProgram);
    int mvpLoc = glGetUniformLocation(shaderProgram, "mvp");
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
    int viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    glUniform3f(lightDirLoc, 0.3f, -0.7f, 0.5f);

    std::vector<FaceDrawCall> sortedDrawCalls = bspLoader.getDrawCalls();
    std::sort(sortedDrawCalls.begin(), sortedDrawCalls.end(),
        [](const FaceDrawCall& a, const FaceDrawCall& b) { return a.texID < b.texID; });

    GLuint defaultTex = bspLoader.getDefaultTextureID();

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        player.update(deltaTime, yaw, pitch, &meshCollider);
        hud.update(deltaTime, player.getPosition());
        processHUDInput(window, hud);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        glm::mat4 view;
        player.getViewMatrix(glm::value_ptr(view));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(player.getEyePosition()));

        // Render BSP
        if (bspVAO != 0) {
            glBindVertexArray(bspVAO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bspEBO);

            GLuint lastTex = 0;
            if (!sortedDrawCalls.empty()) {
                for (const auto& dc : sortedDrawCalls) {
                    if (dc.texID != lastTex) {
                        glBindTexture(GL_TEXTURE_2D, dc.texID);
                        lastTex = dc.texID;
                    }
                    glm::mat4 model = glm::mat4(1.0f);
                    glm::mat4 mvp = projection * view * model;
                    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
                    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                    glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 1.0f, 1.0f);
                    glDrawElements(GL_TRIANGLES, (GLsizei)dc.indexCount, GL_UNSIGNED_INT,
                        (void*)(dc.indexOffset * sizeof(unsigned int)));
                }
            }
            else {
                glBindTexture(GL_TEXTURE_2D, defaultTex);
                glm::mat4 model = glm::mat4(1.0f);
                glm::mat4 mvp = projection * view * model;
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glUniform3f(glGetUniformLocation(shaderProgram, "color"), 0.8f, 0.8f, 0.8f);
                glDrawElements(GL_TRIANGLES, (GLsizei)bspIndexCount, GL_UNSIGNED_INT, 0);
            }
            glBindVertexArray(0);
        }

        // Render player hitbox
        if (showPlayerHitbox) {
            glDisable(GL_CULL_FACE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glm::mat4 model = glm::translate(glm::mat4(1.0f), player.getPosition());
            model = glm::scale(model, glm::vec3(0.6f, 1.8f, 0.6f));
            glm::mat4 mvp = projection * view * model;
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3f(glGetUniformLocation(shaderProgram, "color"), 1.0f, 1.0f, 0.0f);
            glBindVertexArray(cubeVAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
        }

        hud.render(SCR_WIDTH, SCR_HEIGHT);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanupBSP();
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteProgram(shaderProgram);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}