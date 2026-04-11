#include "Camera.h"
#include "pch.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstring>

Camera::Camera(float startX, float startY, float startZ) {
    posX = startX;
    posY = startY;
    posZ = startZ;

    worldUpX = 0.0f;
    worldUpY = 1.0f;
    worldUpZ = 0.0f;

    yaw = -90.0f;
    pitch = 0.0f;
    speed = 5.0f;
    sensitivity = 0.1f;

    updateVectors();
}

void Camera::updateVectors() {
    // ¬ычисл€ем направление взгл€да
    frontX = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    frontY = sin(glm::radians(pitch));
    frontZ = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    // Ќормализуем с проверкой на деление на ноль
    float len = sqrt(frontX * frontX + frontY * frontY + frontZ * frontZ);
    if (len > 0.0001f) {
        frontX /= len;
        frontY /= len;
        frontZ /= len;
    } else {
        // Fallback: смотрим вперед по умолчанию
        frontX = 0.0f;
        frontY = 0.0f;
        frontZ = 1.0f;
    }

    // ¬ычисл€ем правый вектор
    rightX = frontY * worldUpZ - frontZ * worldUpY;
    rightY = frontZ * worldUpX - frontX * worldUpZ;
    rightZ = frontX * worldUpY - frontY * worldUpX;

    // Ќормализуем правый вектор с проверкой
    len = sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
    if (len > 0.0001f) {
        rightX /= len;
        rightY /= len;
        rightZ /= len;
    }

    // ¬ычисл€ем верхний вектор (уже нормализован если front и right нормализованы)
    upX = rightY * frontZ - rightZ * frontY;
    upY = rightZ * frontX - rightX * frontZ;
    upZ = rightX * frontY - rightY * frontX;
}

void Camera::processKeyboard(int key, float deltaTime) {
    float velocity = speed * deltaTime;

    if (key == GLFW_KEY_W) {
        posX += frontX * velocity;
        posY += frontY * velocity;
        posZ += frontZ * velocity;
    }
    if (key == GLFW_KEY_S) {
        posX -= frontX * velocity;
        posY -= frontY * velocity;
        posZ -= frontZ * velocity;
    }
    if (key == GLFW_KEY_A) {
        posX -= rightX * velocity;
        posY -= rightY * velocity;
        posZ -= rightZ * velocity;
    }
    if (key == GLFW_KEY_D) {
        posX += rightX * velocity;
        posY += rightY * velocity;
        posZ += rightZ * velocity;
    }
}

void Camera::processMouse(float xoffset, float yoffset) {
    yaw += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateVectors();
}

void Camera::getViewMatrix(float* matrix) const {
    glm::mat4 view = glm::lookAt(
        glm::vec3(posX, posY, posZ),
        glm::vec3(posX + frontX, posY + frontY, posZ + frontZ),
        glm::vec3(upX, upY, upZ)
    );
    memcpy(matrix, glm::value_ptr(view), sizeof(glm::mat4));
}

void Camera::getPosition(float* x, float* y, float* z) const {
    *x = posX;
    *y = posY;
    *z = posZ;
}