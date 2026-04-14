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

// Enable experimental GLM extensions
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

// Disable deprecation warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS