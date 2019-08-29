#pragma once

#include "defines.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include <vulkan\vulkan.h>


#ifdef _WIN32
#include <windows.h>
#include <libloaderapi.h>

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

struct GLFWwindow;