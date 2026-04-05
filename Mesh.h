#pragma once

class Mesh {
private:
    unsigned int VAO, VBO, EBO;
    int indexCount;

public:
    Mesh(float* vertices, int vertexCount, unsigned int* indices, int indexCount);
    ~Mesh();

    void draw();
};