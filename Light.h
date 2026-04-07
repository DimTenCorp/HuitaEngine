#ifndef LIGHT_H
#define LIGHT_H

#include <string>
#include <vector>
#include <chrono>
#include "Entity.h" // Предполагаем наличие базового класса Entity
#include "MathTypes.h" // Векторы и прочее

// Флаги спавна (из Quake/Half-Life)
#define SF_LIGHT_START_OFF 1

class LightManager; // Forward declaration

class CLight : public CBaseEntity
{
public:
    CLight();
    virtual ~CLight();

    // Инициализация из параметров карты
    void KeyValue(const std::string& key, const std::string& value);

    // Спавн сущности
    void Spawn() override;

    // Обновление состояния (вызывается каждый кадр)
    void Think() override;

    // Обработка активации (триггеры)
    void Use(CBaseEntity* pActivator, CBaseEntity* pCaller);

    // Доступ к данным
    int GetStyle() const { return m_iStyle; }
    const std::string& GetPattern() const { return m_szPattern; }
    bool IsOn() const { return !m_bIsOff; }

protected:
    int m_iStyle;                 // Стиль света (0-31 статические, 32+ динамические)
    std::string m_szPattern;      // Строка паттерна яркости (например, "abcde...")
    bool m_bIsOff;                // Текущее состояние (включен/выключен)

    // Для анимации паттерна
    float m_flNextChangeTime;     // Время следующего изменения яркости
    int m_iCurrentPatternIndex;   // Текущий индекс в строке паттерна
    float m_flFrameTime;          // Накопленное время для анимации
};

// Специализированный класс для окружающего света (небо/солнце)
class CEnvLight : public CLight
{
public:
    void KeyValue(const std::string& key, const std::string& value) override;
    void Spawn() override;

private:
    vec3_t m_vecDirection; // Направление света
    vec3_t m_vecColor;     // Цвет света (RGB)
    int m_iIntensity;      // Интенсивность
};

#endif // LIGHT_H