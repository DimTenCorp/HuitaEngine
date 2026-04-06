#include "Camera.h"
#include <cmath>

Camera::Camera(glm::vec3 startPosition, glm::vec3 startUp)
    : position(startPosition), worldUp(startUp), yaw(-90.0f), pitch(0.0f) {
    updateVectors();
}

void Camera::updateVectors() {
    // Вычисляем направление взгляда из углов Эйлера
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);
    
    // Вычисляем правый и верхний векторы
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= settings.sensitivity;
    yoffset *= settings.sensitivity;
    
    if (settings.invertY) {
        yoffset = -yoffset;
    }
    
    yaw += xoffset;
    pitch += yoffset;
    
    // Ограничиваем угол обзора по вертикали
    if (pitch > settings.maxPitch) pitch = settings.maxPitch;
    if (pitch < settings.minPitch) pitch = settings.minPitch;
    
    updateVectors();
}

void Camera::processKeyboard(int key, float deltaTime) {
    float velocity = settings.moveSpeed * deltaTime;
    
    // Используем GLAD вместо GLFW для независимости
    #ifdef GLFW_INCLUDE_NONE
    // Если GLFW подключен
    #endif
    
    // Обработка клавиш будет через InputHandler
    // Этот метод можно расширить при необходимости
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

void Camera::getViewMatrix(float* matrix) const {
    glm::mat4 view = getViewMatrix();
    memcpy(matrix, glm::value_ptr(view), sizeof(glm::mat4));
}
