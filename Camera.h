#pragma once

class Camera {
private:
    float posX, posY, posZ;
    float frontX, frontY, frontZ;
    float upX, upY, upZ;
    float rightX, rightY, rightZ;
    float worldUpX, worldUpY, worldUpZ;

    float yaw;
    float pitch;
    float speed;
    float sensitivity;

    void updateVectors();

public:
    Camera(float startX, float startY, float startZ);

    void processKeyboard(int key, float deltaTime);
    void processMouse(float xoffset, float yoffset);
    void getViewMatrix(float* matrix) const;
    void getPosition(float* x, float* y, float* z) const;
};