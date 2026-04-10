#include "Water.h"
#include <cmath>
#include <algorithm>

// Предварительно вычисленная таблица синусов (как в GL_WARP.C)
// turbsin[] - lookup table для ускорения расчетов sin/cos

constexpr float PI = 3.14159265358979323846f;
constexpr float TURB_SCALE = 256.0f / (2.0f * PI);

WaterManager::WaterManager() 
    : currentTime(0.0f)
    , underwaterColor(0.0f, 0.3f, 0.5f)
    , underwaterFogDensity(0.5f)
{
    waterParams.resize(static_cast<size_t>(SurfaceType::Underwater) + 1);
    
    // Параметры для обычной воды (из палитры HL1)
    waterParams[static_cast<int>(SurfaceType::Water)].color = glm::vec3(0.2f, 0.4f, 0.6f);
    waterParams[static_cast<int>(SurfaceType::Water)].fogDensity = 0.5f;
    waterParams[static_cast<int>(SurfaceType::Water)].waveSpeed = 1.0f;
    
    // Параметры для слизи/лавы
    waterParams[static_cast<int>(SurfaceType::Slime)].color = glm::vec3(0.4f, 0.6f, 0.2f);
    waterParams[static_cast<int>(SurfaceType::Slime)].fogDensity = 0.7f;
    waterParams[static_cast<int>(SurfaceType::Slime)].waveSpeed = 1.5f;
}

WaterManager::~WaterManager() {
}

void WaterManager::init() {
    // Инициализация таблицы синусов происходит автоматически через TurbSinTable::instance()
}

void WaterManager::update(float deltaTime, float time) {
    currentTime = time;
}

float WaterManager::getWarpOffset(const glm::vec3& vertex, float time) const {
    // Реализация из EmitWaterPolys в GL_WARP.C:
    // s = turbsin[(int)(cl.time * 160.0 + v[0] + v[1]) & 255] + 8.0;
    // s += (turbsin[(int)(cl.time * 171.0 + v[0] * 5.0 - v[1]) & 255] + 8.0) * 0.8;
    
    const auto& turbTable = TurbSinTable::instance();
    
    int index1 = static_cast<int>(time * TURB_SPEED + vertex.x + vertex.y) & 255;
    float offset1 = turbTable.get(index1) + TURB_AMPLITUDE;
    
    int index2 = static_cast<int>(time * 171.0f + vertex.x * 5.0f - vertex.y) & 255;
    float offset2 = turbTable.get(index2) + TURB_AMPLITUDE;
    
    return offset1 + offset2 * 0.8f;
}

glm::vec2 WaterManager::getWarpedTexCoord(const glm::vec2& originalTC, 
                                           const glm::vec3& vertex, 
                                           float time) const {
    // Реализация из EmitWaterPolys в GL_WARP.C:
    // s = os + turbsin[(int)((ot * 0.125 + cl.time) * TURBSCALE) & 255];
    // s *= (1.0 / 64);
    // t = ot + turbsin[(int)((os * 0.125 + cl.time) * TURBSCALE) & 255];
    // t *= (1.0 / 64);
    
    const auto& turbTable = TurbSinTable::instance();
    
    float os = originalTC.x;
    float ot = originalTC.y;
    
    int indexS = static_cast<int>((ot * 0.125f + time) * TURB_SCALE) & 255;
    float s = os + turbTable.get(indexS);
    s *= (1.0f / 64.0f);
    
    int indexT = static_cast<int>((os * 0.125f + time) * TURB_SCALE) & 255;
    float t = ot + turbTable.get(indexT);
    t *= (1.0f / 64.0f);
    
    return glm::vec2(s, t);
}

void WaterManager::boundPoly(const std::vector<glm::vec3>& verts,
                              glm::vec3& mins, glm::vec3& maxs) const {
    // Реализация BoundPoly из GL_WARP.C
    mins = glm::vec3(9999.0f);
    maxs = glm::vec3(-9999.0f);
    
    for (const auto& v : verts) {
        for (int j = 0; j < 3; j++) {
            if (v[j] < mins[j]) mins[j] = v[j];
            if (v[j] > maxs[j]) maxs[j] = v[j];
        }
    }
}

bool WaterManager::clipPolygon(const std::vector<glm::vec3>& inVerts,
                                const std::vector<glm::vec2>& inTexCoords,
                                const glm::vec3& normal,
                                float dist,
                                std::vector<glm::vec3>& frontVerts,
                                std::vector<glm::vec2>& frontTexCoords,
                                std::vector<glm::vec3>& backVerts,
                                std::vector<glm::vec2>& backTexCoords) const {
    // Реализация ClipSkyPolygon из GL_WARP.C
    const float ON_EPSILON = 0.1f;
    
    int nump = static_cast<int>(inVerts.size());
    if (nump == 0) return false;
    
    std::vector<float> distances(nump + 1);
    std::vector<int> sides(nump + 1);
    
    bool front = false;
    bool back = false;
    
    for (int i = 0; i < nump; i++) {
        float d = glm::dot(inVerts[i], normal) - dist;
        distances[i] = d;
        
        if (d > ON_EPSILON) {
            sides[i] = 1; // SIDE_FRONT
            front = true;
        } else if (d < -ON_EPSILON) {
            sides[i] = 2; // SIDE_BACK
            back = true;
        } else {
            sides[i] = 0; // SIDE_ON
        }
    }
    
    sides[nump] = sides[0];
    distances[nump] = distances[0];
    
    if (!front || !back) {
        // Полигон полностью с одной стороны
        if (front) {
            frontVerts = inVerts;
            frontTexCoords = inTexCoords;
        } else {
            backVerts = inVerts;
            backTexCoords = inTexCoords;
        }
        return true;
    }
    
    // Разрезаем полигон
    for (int i = 0; i < nump; i++) {
        switch (sides[i]) {
            case 1: // SIDE_FRONT
                frontVerts.push_back(inVerts[i]);
                frontTexCoords.push_back(inTexCoords[i]);
                break;
            case 2: // SIDE_BACK
                backVerts.push_back(inVerts[i]);
                backTexCoords.push_back(inTexCoords[i]);
                break;
            case 0: // SIDE_ON
                frontVerts.push_back(inVerts[i]);
                frontTexCoords.push_back(inTexCoords[i]);
                backVerts.push_back(inVerts[i]);
                backTexCoords.push_back(inTexCoords[i]);
                break;
        }
        
        if (sides[i] == 0 || sides[i + 1] == 0 || sides[i] == sides[i + 1])
            continue;
        
        // Точка пересечения
        float frac = distances[i] / (distances[i] - distances[i + 1]);
        glm::vec3 intersect = inVerts[i] + frac * (inVerts[i + 1] - inVerts[i]);
        glm::vec2 tcIntersect = inTexCoords[i] + frac * (inTexCoords[i + 1] - inTexCoords[i]);
        
        frontVerts.push_back(intersect);
        frontTexCoords.push_back(tcIntersect);
        backVerts.push_back(intersect);
        backTexCoords.push_back(tcIntersect);
    }
    
    return true;
}

void WaterManager::subdividePolygonRecursive(const std::vector<glm::vec3>& verts,
                                              const std::vector<glm::vec2>& texCoords,
                                              SurfaceType type,
                                              int stage,
                                              std::vector<WaterPoly>& outPolys) {
    // Реализация SubdividePolygon из GL_WARP.C
    constexpr int MAX_VERTS = 64;
    
    if (verts.size() > MAX_VERTS) {
        return; // Слишком много вершин
    }
    
    glm::vec3 mins, maxs;
    boundPoly(verts, mins, maxs);
    
    for (int i = 0; i < 3; i++) {
        float m = (mins[i] + maxs[i]) * 0.5f;
        m = SUBDIVIDE_SIZE * floor(m / SUBDIVIDE_SIZE + 0.5f);
        
        if (maxs[i] - m < 8.0f) continue;
        if (m - mins[i] < 8.0f) continue;
        
        // Разрезаем полигон
        glm::vec3 normal(0.0f);
        normal[i] = 1.0f;
        
        std::vector<glm::vec3> frontVerts, backVerts;
        std::vector<glm::vec2> frontTexCoords, backTexCoords;
        
        if (clipPolygon(verts, texCoords, normal, m, 
                        frontVerts, frontTexCoords, 
                        backVerts, backTexCoords)) {
            subdividePolygonRecursive(frontVerts, frontTexCoords, type, stage + 1, outPolys);
            subdividePolygonRecursive(backVerts, backTexCoords, type, stage + 1, outPolys);
            return;
        }
    }
    
    // Полигон достаточно мал, добавляем его
    WaterPoly poly;
    poly.vertices = verts;
    poly.texCoords = texCoords;
    poly.type = type;
    outPolys.push_back(poly);
}

void WaterManager::subdividePolygon(const std::vector<glm::vec3>& verts,
                                     const std::vector<glm::vec2>& texCoords,
                                     SurfaceType type,
                                     std::vector<WaterPoly>& outPolys) {
    // Точка входа для разбиения полигона (как GL_SubdivideSurface)
    subdividePolygonRecursive(verts, texCoords, type, 0, outPolys);
}

bool WaterManager::isCameraUnderwater(const glm::vec3& cameraPos,
                                       const std::vector<WaterPoly>& waterPolys) const {
    // Простая проверка: если камера ниже любой водной поверхности
    // В полной реализации нужно проверять пересечение с каждым водным полигоном
    
    for (const auto& poly : waterPolys) {
        if (poly.vertices.empty()) continue;
        
        // Проверяем среднюю высоту полигона
        float avgHeight = 0.0f;
        for (const auto& v : poly.vertices) {
            avgHeight += v.z;
        }
        avgHeight /= poly.vertices.size();
        
        if (cameraPos.z < avgHeight) {
            return true;
        }
    }
    
    return false;
}

const WaterParams& WaterManager::getWaterParams(SurfaceType type) const {
    return waterParams[static_cast<int>(type)];
}

void WaterManager::setUnderwaterColor(const glm::vec3& color, float density) {
    underwaterColor = color;
    underwaterFogDensity = density;
}

// Для совместимости с D_SetFadeColor из water.h
void D_SetScreenFade(int r, int g, int b, int alpha, int type) {
    // Эта функция устанавливает цветовой фильтр экрана
    // В современном движке это можно реализовать через post-processing
    // type определяет тип эффекта (водный, ядовитый, и т.д.)
}
