#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Структура настроек камеры
struct CameraSettings {
    float sensitivity = 0.1f;
    float moveSpeed = 5.0f;
    float sprintMultiplier = 2.0f;
    bool invertY = false;
    
    // Ограничения углов
    float minPitch = -89.0f;
    float maxPitch = 89.0f;
};

class Camera {
private:
    // Позиция камеры
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    
    // Углы Эйлера
    float yaw;
    float pitch;
    
    // Настройки
    CameraSettings settings;
    
    // Обновление векторов направления
    void updateVectors();
    
public:
    // Конструкторы
    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3 startUp = glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Обработка ввода
    void processMouseMovement(float xoffset, float yoffset);
    void processKeyboard(int key, float deltaTime);
    
    // Получение матрицы вида
    glm::mat4 getViewMatrix() const;
    void getViewMatrix(float* matrix) const;
    
    // Геттеры и сеттеры
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    glm::vec3 getUp() const { return up; }
    glm::vec3 getRight() const { return right; }
    
    float getYaw() const { return yaw; }
    float getPitch() const { return pitch; }
    
    void setYaw(float y) { yaw = y; updateVectors(); }
    void setPitch(float p) { pitch = p; updateVectors(); }
    void setPosition(const glm::vec3& pos) { position = pos; }
    
    void setSensitivity(float sens) { settings.sensitivity = sens; }
    void setMoveSpeed(float speed) { settings.moveSpeed = speed; }
    
    const CameraSettings& getSettings() const { return settings; }
    CameraSettings& getSettings() { return settings; }
};
