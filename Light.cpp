/*
===== lights.cpp ========================================================

  Адаптированная версия системы освещения из Half-Life для HuitaEngine.
  Реализует динамические источники света с паттернами мерцания.

*/

#include "Light.h"
#include "Logger.h" // Для логирования
#include "World.h"  // Для доступа к миру
#include <cmath>
#include <algorithm>

// Конвертация символа паттерна (a-z, A-Z) в значение яркости (0-255)
static float LetterToBrightness(char c)
{
    if (c >= 'a' && c <= 'z')
        return (float)(c - 'a') * (255.0f / 26.0f);
    else if (c >= 'A' && c <= 'Z')
        return (float)(c - 'A' + 13) * (255.0f / 26.0f);
    else if (c >= '0' && c <= '9')
        return (float)(c - '0') * (255.0f / 10.0f);
    else
        return 0.0f; // ' ', '-', другие символы = выключено
}

//============================================================================
// CLight
//============================================================================

CLight::CLight() 
    : m_iStyle(0)
    , m_bIsOff(false)
    , m_flNextChangeTime(0.0f)
    , m_iCurrentPatternIndex(0)
    , m_flFrameTime(0.0f)
{
}

CLight::~CLight()
{
}

void CLight::KeyValue(const std::string& key, const std::string& value)
{
    if (key == "style")
    {
        m_iStyle = std::stoi(value);
    }
    else if (key == "pitch")
    {
        // Угол наклона (если нужно для направленных источников)
        m_angles.x = std::stof(value);
    }
    else if (key == "pattern")
    {
        m_szPattern = value;
    }
    else if (key == "targetname")
    {
        m_targetname = value;
    }
    else if (key == "spawnflags")
    {
        int flags = std::stoi(value);
        if (flags & SF_LIGHT_START_OFF)
            m_bIsOff = true;
    }
    else
    {
        // Базовая обработка в родительском классе
        CBaseEntity::KeyValue(key, value);
    }
}

void CLight::Spawn()
{
    // Если нет targetname, свет неактивен и может быть удален
    if (m_targetname.empty())
    {
        Logger::Log("CLight: Удаление инертного источника света без targetname\n");
        // В реальном движке здесь был бы вызов REMOVE_ENTITY
        return;
    }

    // Инициализация состояния
    if (m_iStyle >= 32)
    {
        // Динамический свет с паттерном
        if (m_bIsOff)
        {
            // Начинаем выключенным
            SetBrightness(0.0f);
        }
        else
        {
            // Начинаем включенным с паттерном или дефолтным значением
            if (!m_szPattern.empty())
            {
                float brightness = LetterToBrightness(m_szPattern[0]);
                SetBrightness(brightness);
                m_iCurrentPatternIndex = 0;
                m_flNextChangeTime = 0.1f; // Начальная задержка
            }
            else
            {
                // Дефолтный паттерн "m" (средняя яркость)
                SetBrightness(150.0f);
            }
        }
        
        Logger::Log("CLight: Спавн динамического света стиль=%d, паттерн='%s', состояние=%s\n",
                    m_iStyle, m_szPattern.c_str(), m_bIsOff ? "ВЫКЛ" : "ВКЛ");
    }
    else
    {
        // Статический свет (стили 0-31)
        float brightness = m_bIsOff ? 0.0f : 255.0f;
        SetBrightness(brightness);
        Logger::Log("CLight: Спавн статического света стиль=%d, состояние=%s\n",
                    m_iStyle, m_bIsOff ? "ВЫКЛ" : "ВКЛ");
    }
}

void CLight::Think()
{
    // Обработка только для динамических источников со стилем >= 32
    if (m_iStyle < 32 || m_szPattern.empty() || m_bIsOff)
        return;

    // Обновление времени
    float currentTime = GetFrameTime(); // Получаем время кадра от движка
    
    if (currentTime >= m_flNextChangeTime)
    {
        // Переход к следующему символу паттерна
        m_iCurrentPatternIndex++;
        if (m_iCurrentPatternIndex >= (int)m_szPattern.length())
            m_iCurrentPatternIndex = 0; // Циклический повтор

        char c = m_szPattern[m_iCurrentPatternIndex];
        float brightness = LetterToBrightness(c);
        
        // Установка новой яркости
        SetBrightness(brightness);
        
        // Расчет времени до следующего изменения
        // В Quake/Half-Life каждый символ длится ~0.1 секунды при 10 FPS
        // Но можно сделать плавнее
        float delay = 0.1f; 
        
        // Специальные символы могут менять длительность
        if (c == '-' || c == ' ')
            delay = 0.2f; // Пауза длиннее
        
        m_flNextChangeTime = currentTime + delay;
    }
}

void CLight::Use(CBaseEntity* pActivator, CBaseEntity* pCaller)
{
    if (m_iStyle < 32)
        return; // Только динамические источники реагируют на Use

    // Переключение состояния
    bool shouldTurnOn = !m_bIsOff; // Инвертируем текущее состояние
    
    if (shouldTurnOn)
    {
        // Включаем свет
        m_bIsOff = false;
        
        if (!m_szPattern.empty())
        {
            // Восстанавливаем паттерн с первого символа
            float brightness = LetterToBrightness(m_szPattern[0]);
            SetBrightness(brightness);
            m_iCurrentPatternIndex = 0;
            m_flNextChangeTime = GetFrameTime() + 0.1f;
        }
        else
        {
            SetBrightness(150.0f); // Дефолтная яркость
        }
        
        Logger::Log("CLight: Включен по триггеру (стиль %d)\n", m_iStyle);
    }
    else
    {
        // Выключаем свет
        m_bIsOff = true;
        SetBrightness(0.0f);
        Logger::Log("CLight: Выключен по триггеру (стиль %d)\n", m_iStyle);
    }
}

// Вспомогательный метод для установки яркости (должен интегрироваться с системой рендеринга)
void CLight::SetBrightness(float brightness)
{
    // Здесь должна быть интеграция с системой освещения движка
    // Например, обновление uniform-переменной в шейдере или списка источников света
    
    // Для демонстрации просто логируем
    // Logger::Log("CLight: Стиль %d, яркость=%.1f\n", m_iStyle, brightness);
    
    // TODO: Вызвать LightManager::UpdateLightSource(m_iStyle, brightness);
}

//============================================================================
// CEnvLight (окружающее освещение)
//============================================================================

void CEnvLight::KeyValue(const std::string& key, const std::string& value)
{
    if (key == "_light")
    {
        // Парсинг формата: "R G B Intensity" или "Intensity" (оттенки серого)
        int r = 0, g = 0, b = 0, v = 255;
        int count = sscanf(value.c_str(), "%d %d %d %d", &r, &g, &b, &v);
        
        if (count == 1)
        {
            // Один параметр - оттенки серого
            g = b = r;
        }
        else if (count == 4)
        {
            // Четыре параметра - RGBA с умножением на интенсивность
            r = (int)(r * (v / 255.0));
            g = (int)(g * (v / 255.0));
            b = (int)(b * (v / 255.0));
        }
        
        // Эмуляция расчетов QRAD (как в оригинальном коде)
        // Формула: pow(value / 114.0, 0.6) * 264
        r = (int)(pow(r / 114.0, 0.6) * 264);
        g = (int)(pow(g / 114.0, 0.6) * 264);
        b = (int)(pow(b / 114.0, 0.6) * 264);
        
        // Ограничение диапазона [0, 255]
        m_vecColor.x = std::clamp((float)r, 0.0f, 255.0f);
        m_vecColor.y = std::clamp((float)g, 0.0f, 255.0f);
        m_vecColor.z = std::clamp((float)b, 0.0f, 255.0f);
        
        Logger::Log("CEnvLight: Цвет неба установлен на (%.1f, %.1f, %.1f)\n", 
                    m_vecColor.x, m_vecColor.y, m_vecColor.z);
    }
    else
    {
        CLight::KeyValue(key, value);
    }
}

void CEnvLight::Spawn()
{
    // Вычисление направления света из углов сущности
    // UTIL_MakeAimVectors в оригинале устанавливает forward/right/up векторы
    vec3_t forward;
    AngleVectors(m_angles, &forward, nullptr, nullptr);
    
    m_vecDirection = forward;
    
    Logger::Log("CEnvLight: Направление окружающего света (%.2f, %.2f, %.2f)\n",
                m_vecDirection.x, m_vecDirection.y, m_vecDirection.z);
    
    // Установка глобальных переменных окружения (аналог CVAR в оригинале)
    // В современном движке лучше использовать singleton или систему настроек
    World::GetInstance()->SetSkyColor(m_vecColor);
    World::GetInstance()->SetSkyDirection(m_vecDirection);
    
    // Вызов базового спавна
    CLight::Spawn();
}
