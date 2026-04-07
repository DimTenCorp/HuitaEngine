#pragma once

class Mesh {
private:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    int indexCount = 0;

public:
    Mesh(float* vertices, int vertexCount, unsigned int* indices, int indexCount);
    ~Mesh();

    void draw();
};