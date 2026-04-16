// pch.h - Precompiled Header
#pragma once

// Prevent Windows header macro conflicts
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX

// GLAD MUST BE FIRST before any OpenGL headers
#include <glad/glad.h>

// Then GLFW
#include <GLFW/glfw3.h>

// Then other OpenGL-dependent headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Standard Library containers
#include <unordered_map>
#include <unordered_set>