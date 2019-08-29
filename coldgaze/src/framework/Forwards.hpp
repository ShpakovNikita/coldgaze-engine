#pragma once

#include "defines.h"

#include <signal.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include <vulkan\vulkan.h>


#ifdef _WIN32
#include <windows.h>
#include <libloaderapi.h>
#endif

struct GLFWwindow;

#ifdef _DEBUG
#ifdef _WIN32
#define CG_ASSERT(statement)					 \
{												 \
	if (!statement)								 \
	{											 \
		std::cerr << ##statement << std::endl;   \
		__debugbreak();				     		 \
	}											 \
}
#else
#define CG_ASSERT(statement) {}
#endif
#else
#define CG_ASSERT(statement) {}
#endif