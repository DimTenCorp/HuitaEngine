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

bool WorldRenderer::init(const BSPLoader& bspLoader) {
    if (initialized) {
        return true;
    }

    // Создаем листья и узлы
    createLeaves(bspLoader);
    createNodes(bspLoader);
    
    if (!nodes.empty()) {
        updateNodeParents(0, 0);
    }

    // Создаем буферы
    createBuffers();

    // Инициализируем список поверхностей
    nextMaterialIndex = 1;
    surfaces.reserve(1000);

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

void WorldRenderer::createLeaves(const BSPLoader& bspLoader) {
    // Заглушка: нужно добавить getter для листьев в BSPLoader
    // leaves = bspLoader.getLeaves();
    leaves.clear();
}

void WorldRenderer::createNodes(const BSPLoader& bspLoader) {
    // Заглушка: нужно добавить getter для узлов в BSPLoader
    // nodes = bspLoader.getNodes();
    nodes.clear();
}

void WorldRenderer::updateNodeParents(int nodeIdx, int parent) {
    if (nodeIdx >= static_cast<int>(nodes.size())) {
        return;
    }
    
    nodes[nodeIdx].parentIdx = parent;
    
    // Рекурсивно обновляем детей
    Node& node = nodes[nodeIdx];
    if (node.children[0] != 0) {
        updateNodeParents(node.children[0], nodeIdx);
    }
    if (node.children[1] != 0) {
        updateNodeParents(node.children[1], nodeIdx);
    }
}

void WorldRenderer::createBuffers() {
    // Создаем EBO для батчинга
    glGenBuffers(1, &worldEBO);
    worldEBOBuf.reserve(10000);
}

void WorldRenderer::render(const glm::vec3& viewPos, const glm::mat4& view, const glm::mat4& projection,
                           Shader& shader, const BSPLightmap& lightmap) {
    if (!initialized) {
        return;
    }

    stats.reset();
    
    WorldSurfaceList surfList;
    surfList.nodeVisFrame.resize(nodes.size(), 0);
    surfList.leafVisFrame.resize(leaves.size(), 0);
    
    // Получаем видимые поверхности
    getVisibleSurfaces(viewPos, surfList);
    
    // Рендерим текстурный мир
    drawTexturedWorld(surfList, shader, lightmap);
    
    // Сохраняем статистику
    lastDrawCalls = stats.drawCalls;
    lastTriangles = stats.worldPolys + stats.skyPolys;
}

void WorldRenderer::getVisibleSurfaces(const glm::vec3& viewPos, WorldSurfaceList& surfList) {
    surfList.textureChain.clear();
    surfList.skySurfaces.clear();
    surfList.textureChainFrames.clear();
    surfList.visFrame++;
    
    // Находим лист, в котором находится камера
    // Заглушка: нужна реальная логика поиска листа
    surfList.viewLeaf = 0;
    
    // Помечаем видимые листья
    markLeaves(viewPos, surfList);
    
    // Обходим узлы и собираем видимые поверхности
    if (!nodes.empty()) {
        recursiveWorldNodesTextured(viewPos, surfList, 0);
    }
}

void WorldRenderer::markLeaves(const glm::vec3& viewPos, WorldSurfaceList& surfList) {
    // Заглушка: реализация PVS (Potentially Visible Set)
    // В реальной реализации нужно использовать visData из BSP
    if (surfList.viewLeaf >= 0 && surfList.viewLeaf < static_cast<int>(leaves.size())) {
        surfList.leafVisFrame[surfList.viewLeaf] = surfList.visFrame;
    }
}

void WorldRenderer::recursiveWorldNodesTextured(const glm::vec3& viewPos, WorldSurfaceList& surfList, int nodeIdx) {
    if (nodeIdx < 0 || nodeIdx >= static_cast<int>(nodes.size())) {
        return;
    }
    
    Node& node = nodes[nodeIdx];
    
    // Frustum culling можно добавить здесь
    
    // Проверяем, является ли узел листом
    if (node.contents != CONTENTS_EMPTY) {
        // Это лист
        if (nodeIdx < static_cast<int>(leaves.size())) {
            Leaf& leaf = leaves[nodeIdx];
            if (leafVisFrame[nodeIdx] == surfList.visFrame) {
                // Добавляем поверхности листа
                // Заглушка: нужна реальная логика
            }
        }
        return;
    }
    
    // Обходим детей
    recursiveWorldNodesTextured(viewPos, surfList, node.children[0]);
    recursiveWorldNodesTextured(viewPos, surfList, node.children[1]);
    
    // Добавляем поверхности узла
    for (unsigned int i = 0; i < node.numSurfaces; ++i) {
        unsigned int surfIdx = node.firstSurface + i;
        if (surfIdx < surfaces.size()) {
            RenderSurface& surf = surfaces[surfIdx];
            
            // Пропускаем небо в обычном рендеринге
            if (surf.flags & SURF_DRAWSKY) {
                surfList.skySurfaces.push_back(surfIdx);
                continue;
            }
            
            // Добавляем в цепочку текстуры
            if (surf.materialIdx >= surfList.textureChain.size()) {
                surfList.textureChain.resize(surf.materialIdx + 1);
                surfList.textureChainFrames.resize(surf.materialIdx + 1, 0);
            }
            
            surfList.textureChain[surf.materialIdx].push_back(surfIdx);
            surfList.textureChainFrames[surf.materialIdx] = surfList.visFrame;
        }
    }
}

void WorldRenderer::drawTexturedWorld(WorldSurfaceList& surfList, const Shader& shader, const BSPLightmap& lightmap) {
    if (surfList.textureChain.empty()) {
        return;
    }
    
    shader.bind();
    
    // Bind VAO (нужно создать при инициализации)
    // glBindVertexArray(worldVAO);
    
    // Настраиваем lightmap texture array
    // GLuint texArray = lightmap.getTextureArray();
    // glActiveTexture(GL_TEXTURE1);
    // glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
    
    // Рендерим каждую цепочку текстур
    for (size_t i = 0; i < surfList.textureChain.size(); ++i) {
        if (surfList.textureChain[i].empty()) {
            continue;
        }
        
        // Активируем текстуру материала
        // В реальной реализации: gTextures[materialIdx]->bind()
        
        stats.drawCalls++;
        stats.worldPolys += static_cast<unsigned int>(surfList.textureChain[i].size());
        
        // Рендерим поверхности в цепочке
        // Здесь нужен batching через glMultiDrawElements
        for (unsigned int surfIdx : surfList.textureChain[i]) {
            // RenderSurface& surf = surfaces[surfIdx];
            // glDrawElements(GL_TRIANGLES, surf.vertexCount, GL_UNSIGNED_SHORT, ...);
        }
    }
    
    // Восстанавливаем состояние
    // glBindVertexArray(0);
}

void WorldRenderer::drawSkybox(WorldSurfaceList& surfList, const Shader& shader) {
    if (surfList.skySurfaces.empty()) {
        return;
    }
    
    shader.bind();
    
    stats.drawCalls++;
    stats.skyPolys += static_cast<unsigned int>(surfList.skySurfaces.size());
    
    // Рендерим sky поверхности
    // В реальной реализации нужен отдельный шейдер для неба
}

void WorldRenderer::drawWireframe(WorldSurfaceList& surfList, const Shader& wireframeShader, bool drawSky) {
    // Заглушка для wireframe рендеринга
}

void WorldRenderer::setLightstyleScale(int style, float scale) {
    if (style >= 0 && style < 4) {
        lightstyleScales[style] = scale;
    }
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
        
        shader.bind();
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
    shader.bind();
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
    wireframeShader.bind();
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
    
    // Получаем позицию камеры через существующий API
    float camX, camY, camZ;
    camera.getPosition(&camX, &camY, &camZ);
    glm::vec3 viewOrigin(camX, camY, camZ);
    
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
    float camX, camY, camZ;
    camera.getPosition(&camX, &camY, &camZ);
    glm::vec3 viewOrigin(camX, camY, camZ);
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
