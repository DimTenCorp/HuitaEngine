#include "WorldRenderer.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <algorithm>

// Константы из GoldSrc/Half-Life
#define CONTENTS_EMPTY -1
#define SURF_DRAWSKY 0x0004
#define MAX_MAP_LEAFS 32767

WorldRenderer::WorldRenderer() = default;

WorldRenderer::~WorldRenderer() {
    cleanup();
}

bool WorldRenderer::initialize(const BSPLoader& bspLoader) {
    if (initialized) {
        return true;
    }

    // Получаем данные из BSP загрузчика
    const auto& faces = bspLoader.getMeshIndices(); // Используем как временное решение
    const auto& planes = /* нужно добавить getter в BSPLoader */ std::vector<BSPPlane>();
    const auto& leaves_data = /* нужно добавить getter */ std::vector<int>();
    const auto& nodes_data = /* нужно добавить getter */ std::vector<int>();

    // Создаем листья и узлы
    createLeaves(bspLoader);
    createNodes(bspLoader);
    
    if (!nodes.empty()) {
        updateNodeParents(0, 0);
    }

    // Создаем буферы
    createBuffers();

    // Инициализируем список поверхностей
    // Примечание: здесь нужна интеграция с данными о поверхностях из BSP
    // В оригинальном коде поверхности уже подготовлены SceneRenderer'ом
    
    nextMaterialIndex = 1; // Минимум одна текстура
    surfaces.reserve(1000); // Резервируем место

    initialized = true;
    return true;
}

void WorldRenderer::cleanup() {
    if (worldEBO != 0) {
        glDeleteBuffers(1, &worldEBO);
        worldEBO = 0;
    }
    worldEBOBuf.clear();
    nodes.clear();
    leaves.clear();
    surfaces.clear();
    initialized = false;
}

void WorldRenderer::getVisibleSurfaces(const Camera& camera, WorldSurfaceList& surfList) {
    if (!initialized) {
        return;
    }

    // Инициализация visFrame при первом вызове
    if (surfList.visFrame == 0) {
        surfList.nodeVisFrame.resize(nodes.size());
        surfList.leafVisFrame.resize(leaves.size());
        surfList.visFrame = 1;
    }

    // Подготовка списка поверхностей
    surfList.textureChainFrame++;
    surfList.textureChain.resize(nextMaterialIndex);
    surfList.textureChainFrames.resize(nextMaterialIndex);
    surfList.skySurfaces.clear();

    // Отмечаем видимые листья
    markLeaves(camera, surfList);

    // Обход BSP дерева для сбора видимых поверхностей
    recursiveWorldNodesTextured(camera, surfList, 0);
}

void WorldRenderer::drawTexturedWorld(WorldSurfaceList& surfList, const Shader& shader) {
    auto& textureChain = surfList.textureChain;
    auto& textureChainFrames = surfList.textureChainFrames;
    unsigned frame = surfList.textureChainFrame;
    unsigned drawnSurfs = 0;

    if (!initialized || textureChain.empty()) {
        return;
    }

    // Настраиваем GL
    glBindVertexArray(/* нужен VAO */ 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);

    // Рисуем цепочки текстур
    unsigned eboIdx = 0;

    for (size_t i = 0; i < textureChain.size(); i++) {
        if (textureChainFrames[i] != frame || textureChain[i].empty()) {
            continue;
        }

        // Биндим материал/текстуру
        // В оригинальном коде: mat->activateTextures()
        // Здесь нужна интеграция с вашей системой материалов
        
        shader.use();
        // shader.setUniform(...); // Настройка шейдера

        // Заполняем EBO
        unsigned eboOffset = eboIdx;
        unsigned vertexCount = 0;

        for (unsigned surfIdx : textureChain[i]) {
            if (surfIdx >= surfaces.size()) continue;
            
            RenderSurface& surf = surfaces[surfIdx];
            uint16_t vertCount = (uint16_t)surf.vertexCount;

            for (uint16_t j = 0; j < vertCount; j++) {
                if (eboIdx + j < worldEBOBuf.size()) {
                    worldEBOBuf[eboIdx + j] = (uint16_t)surf.vertexOffset + j;
                }
            }

            eboIdx += vertCount;
            if (eboIdx < worldEBOBuf.size()) {
                worldEBOBuf[eboIdx] = 0xFFFF; // PRIMITIVE_RESTART_IDX
                eboIdx++;
            }
            vertexCount += vertCount + 1;
        }

        // Убираем последний PRIMITIVE_RESTART_IDX
        if (eboIdx > 0) {
            eboIdx--;
            vertexCount--;
        }

        // Обновляем EBO
        if (eboIdx > eboOffset && eboOffset + vertexCount <= worldEBOBuf.size()) {
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 
                           eboOffset * sizeof(uint16_t),
                           vertexCount * sizeof(uint16_t),
                           worldEBOBuf.data() + eboOffset);

            // Рисуем элементы
            glDrawElements(GL_TRIANGLE_FAN, vertexCount, GL_UNSIGNED_SHORT,
                          reinterpret_cast<const void*>(eboOffset * sizeof(uint16_t)));

            drawnSurfs += (unsigned)textureChain[i].size();
            stats.drawCalls++;
        }
    }

    stats.worldPolys += drawnSurfs;
}

void WorldRenderer::drawSkybox(WorldSurfaceList& surfList, const Shader& shader) {
    if (!initialized || surfList.skySurfaces.empty()) {
        return;
    }

    // Настраиваем GL
    glBindVertexArray(/* нужен VAO */ 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
    glDepthFunc(GL_LEQUAL);

    // Биндим скайбокс материал
    shader.use();
    // shader.setUniform(...);

    // Заполняем EBO
    unsigned eboIdx = 0;
    for (unsigned surfIdx : surfList.skySurfaces) {
        if (surfIdx >= surfaces.size()) continue;
        
        RenderSurface& surf = surfaces[surfIdx];
        uint16_t vertCount = (uint16_t)surf.vertexCount;

        for (uint16_t j = 0; j < vertCount; j++) {
            if (eboIdx + j < worldEBOBuf.size()) {
                worldEBOBuf[eboIdx + j] = (uint16_t)surf.vertexOffset + j;
            }
        }

        eboIdx += vertCount;
        if (eboIdx < worldEBOBuf.size()) {
            worldEBOBuf[eboIdx] = 0xFFFF; // PRIMITIVE_RESTART_IDX
            eboIdx++;
        }
    }

    // Убираем последний PRIMITIVE_RESTART_IDX
    if (eboIdx > 0) {
        eboIdx--;
    }

    // Обновляем EBO
    if (eboIdx > 0) {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 
                       eboIdx * sizeof(uint16_t), 
                       worldEBOBuf.data());

        // Рисуем элементы
        glDrawElements(GL_TRIANGLE_FAN, eboIdx, GL_UNSIGNED_SHORT, nullptr);

        stats.skyPolys += (unsigned)surfList.skySurfaces.size();
        stats.drawCalls++;
    }

    // Восстанавливаем GL
    glDepthFunc(GL_LESS);
}

void WorldRenderer::drawWireframe(WorldSurfaceList& surfList, const Shader& wireframeShader, bool drawSky) {
    if (!initialized) {
        return;
    }

    auto& textureChain = surfList.textureChain;
    auto& textureChainFrames = surfList.textureChainFrames;
    unsigned frame = surfList.textureChainFrame;

    // Заполняем EBO поверхностями мира
    unsigned eboIdx = 0;

    for (size_t i = 0; i < textureChain.size(); i++) {
        if (textureChainFrames[i] != frame) {
            continue;
        }

        for (unsigned surfIdx : textureChain[i]) {
            if (surfIdx >= surfaces.size()) continue;
            
            RenderSurface& surf = surfaces[surfIdx];
            uint16_t vertCount = (uint16_t)surf.vertexCount;

            for (uint16_t j = 0; j < vertCount; j++) {
                if (eboIdx + j < worldEBOBuf.size()) {
                    worldEBOBuf[eboIdx + j] = (uint16_t)surf.vertexOffset + j;
                }
            }

            eboIdx += vertCount;
            if (eboIdx < worldEBOBuf.size()) {
                worldEBOBuf[eboIdx] = 0xFFFF;
                eboIdx++;
            }
        }
    }

    // Заполняем EBO небесными поверхностями
    if (drawSky) {
        for (unsigned surfIdx : surfList.skySurfaces) {
            if (surfIdx >= surfaces.size()) continue;
            
            RenderSurface& surf = surfaces[surfIdx];
            uint16_t vertCount = (uint16_t)surf.vertexCount;

            for (uint16_t j = 0; j < vertCount; j++) {
                if (eboIdx + j < worldEBOBuf.size()) {
                    worldEBOBuf[eboIdx + j] = (uint16_t)surf.vertexOffset + j;
                }
            }

            eboIdx += vertCount;
            if (eboIdx < worldEBOBuf.size()) {
                worldEBOBuf[eboIdx] = 0xFFFF;
                eboIdx++;
            }
        }
    }

    if (eboIdx == 0) {
        return;
    }

    // Настраиваем GL
    glBindVertexArray(/* нужен VAO */ 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);

    // Включаем шейдер wireframe
    wireframeShader.use();
    // shader.setUniform("color", glm::vec3(0.8f));

    // Убираем последний PRIMITIVE_RESTART_IDX
    eboIdx--;

    // Обновляем EBO
    if (eboIdx > 0) {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 
                       eboIdx * sizeof(uint16_t), 
                       worldEBOBuf.data());

        // Рисуем элементы
        glDrawElements(GL_LINE_LOOP, eboIdx, GL_UNSIGNED_SHORT, nullptr);
        stats.drawCalls++;
    }
}

void WorldRenderer::setLightstyleScale(int style, float scale) {
    if (style >= 0 && style < 4) {
        lightstyleScales[style] = scale;
    }
}

void WorldRenderer::createLeaves(const BSPLoader& bspLoader) {
    // Примечание: нужны getters для листьев и vis данных в BSPLoader
    // В оригинальном коде: m_Renderer.m_Level.getLeaves() и getVisData()
    
    // Заглушка для демонстрации структуры
    // Реальная реализация потребует добавления методов в BSPLoader:
    // - getLeaves() -> возвращает массив BSPLeaf
    // - getVisData() -> возвращает массив uint8_t с compressed vis
    
    leaves.clear();
    // Пример:
    // auto& lvlLeaves = bspLoader.getLeaves();
    // auto& lvlVisData = bspLoader.getVisData();
    // leaves.resize(lvlLeaves.size());
    // ... заполнение данных
}

void WorldRenderer::createNodes(const BSPLoader& bspLoader) {
    // Примечание: нужны getters для узлов и плоскостей в BSPLoader
    // В оригинальном коде: m_Renderer.m_Level.getNodes() и getPlanes()
    
    nodes.clear();
    // Пример:
    // auto& lvlNodes = bspLoader.getNodes();
    // auto& lvlPlanes = bspLoader.getPlanes();
    // nodes.resize(lvlNodes.size());
    // ... заполнение данных
}

void WorldRenderer::updateNodeParents(int nodeIdx, int parent) {
    if (nodeIdx >= 0 && nodeIdx < (int)nodes.size()) {
        Node& node = nodes[nodeIdx];
        node.parentIdx = parent;
        updateNodeParents(node.children[0], nodeIdx);
        updateNodeParents(node.children[1], nodeIdx);
    }
    // Для листьев обработка через отрицательные индексы
    // else if (nodeIdx < 0) {
    //     Leaf& leaf = leaves[~nodeIdx];
    //     leaf.parentIdx = parent;
    // }
}

void WorldRenderer::createBuffers() {
    // Вычисляем максимальный размер EBO
    // X = максимальное количество вершин, Y = максимальное количество поверхностей
    unsigned maxVertexCount = 100000; // Заглушка, нужно вычислить из данных
    unsigned maxSurfaceCount = 10000; // Заглушка
    
    unsigned maxEboSize = maxVertexCount + maxSurfaceCount;
    worldEBOBuf.resize(maxEboSize);

    glGenBuffers(1, &worldEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, worldEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                 maxEboSize * sizeof(uint16_t), 
                 nullptr, 
                 GL_DYNAMIC_DRAW);
}

void WorldRenderer::markLeaves(const Camera& camera, WorldSurfaceList& surfList) {
    // PVS (Potentially Visible Set) логика
    // В оригинале используется ConVar r_lockpvs и r_novis
    
    glm::vec3 viewOrigin = camera.getPosition();
    
    // Находим лист, в котором находится камера
    // int viewleaf = bspLoader.pointInLeaf(viewOrigin);
    int viewleaf = 0; // Заглушка

    if (viewleaf == surfList.viewLeaf) {
        // Лист не изменился, оставляем PVS как есть
        return;
    }

    surfList.viewLeaf = viewleaf;
    surfList.visFrame++;
    unsigned visFrame = surfList.visFrame;

    uint8_t visBuf[MAX_MAP_LEAFS / 8];
    const uint8_t* vis = nullptr;

    // Если r_novis включен, делаем все листья видимыми
    bool noVis = false; // Заглушка для консольной переменной
    
    if (noVis) {
        memset(visBuf, 0xFF, (leaves.size() + 7) >> 3);
        vis = visBuf;
    } else {
        // Получаем PVS для текущего листа
        // vis = bspLoader.leafPVS(viewleaf, visBuf);
        vis = visBuf; // Заглушка
        memset(visBuf, 0xFF, (leaves.size() + 7) >> 3);
    }

    // Узел 0 всегда виден
    if (!surfList.nodeVisFrame.empty()) {
        surfList.nodeVisFrame[0] = visFrame;
    }

    // Проходим по всем листьям и отмечаем видимые
    for (unsigned i = 0; i < leaves.size() && i < MAX_MAP_LEAFS - 1; i++) {
        if (vis[i >> 3] & (1 << (i & 7))) {
            int node = ~(i + 1);

            do {
                if (node < 0) {
                    // Лист
                    unsigned leafIdx = ~node;
                    if (leafIdx < surfList.leafVisFrame.size() && 
                        surfList.leafVisFrame[leafIdx] == visFrame) {
                        break;
                    }

                    if (leafIdx < surfList.leafVisFrame.size()) {
                        surfList.leafVisFrame[leafIdx] = visFrame;
                    }
                    
                    if (leafIdx < leaves.size()) {
                        node = leaves[leafIdx].parentIdx;
                    } else {
                        break;
                    }
                } else {
                    // Узел
                    if ((unsigned)node < surfList.nodeVisFrame.size() && 
                        surfList.nodeVisFrame[node] == visFrame) {
                        break;
                    }

                    if ((unsigned)node < surfList.nodeVisFrame.size()) {
                        surfList.nodeVisFrame[node] = visFrame;
                    }
                    
                    if ((unsigned)node < nodes.size()) {
                        node = nodes[node].parentIdx;
                    } else {
                        break;
                    }
                }
            } while (node != 0);
        }
    }
}

void WorldRenderer::recursiveWorldNodesTextured(const Camera& camera, 
                                                WorldSurfaceList& surfList, 
                                                int nodeIdx) {
    if (nodeIdx < 0) {
        // Лист - ничего не делаем, поверхности уже собраны
        return;
    }

    if ((unsigned)nodeIdx >= surfList.nodeVisFrame.size() || 
        surfList.nodeVisFrame[nodeIdx] != surfList.visFrame) {
        // Не в PVS
        return;
    }

    if ((unsigned)nodeIdx >= nodes.size()) {
        return;
    }

    const Node& node = nodes[nodeIdx];

    // Вычисляем сторону относительно плоскости
    glm::vec3 viewOrigin = camera.getPosition();
    float dot = glm::dot(viewOrigin, node.plane ? node.plane->normal : glm::vec3(1,0,0)) 
                - (node.plane ? node.plane->dist : 0);
    int side = (dot >= 0.0f) ? 0 : 1;

    // Передняя сторона
    if ((int)node.children[side] >= 0 || ~(int)node.children[side] < (int)leaves.size()) {
        recursiveWorldNodesTextured(camera, surfList, node.children[side]);
    }

    // Рисуем поверхности текущего узла
    for (unsigned i = 0; i < node.numSurfaces && node.firstSurface + i < surfaces.size(); i++) {
        unsigned idx = node.firstSurface + i;
        RenderSurface& surf = surfaces[idx];

        // Frustum culling можно добавить здесь
        // if (camera.cullSurface(surf)) continue;

        if (surf.flags & SURF_DRAWSKY) {
            surfList.skySurfaces.push_back(idx);
        } else {
            unsigned matIdx = surf.materialIdx;
            
            if (matIdx >= surfList.textureChainFrames.size()) {
                // Расширяем векторы если нужно
                size_t oldSize = surfList.textureChainFrames.size();
                surfList.textureChainFrames.resize(matIdx + 1, 0);
                surfList.textureChain.resize(matIdx + 1);
            }
            
            unsigned oldChainFrame = surfList.textureChainFrames[matIdx];
            if (oldChainFrame != surfList.textureChainFrame) {
                surfList.textureChainFrames[matIdx] = surfList.textureChainFrame;
                surfList.textureChain[matIdx].clear();
            }

            surfList.textureChain[matIdx].push_back(idx);
        }
    }

    // Задняя сторона
    if ((int)node.children[!side] >= 0 || ~(int)node.children[!side] < (int)leaves.size()) {
        recursiveWorldNodesTextured(camera, surfList, node.children[!side]);
    }
}
