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
#include <functional>

namespace CG
{
	enum class eAssertResponse
	{
		kNone = 0,
		kBreak,
		kContinue,
	};
}

extern std::function<CG::eAssertResponse(const char*)> kShowAssertMessage;

#ifdef _DEBUG
#ifdef _WIN32
#define CG_ASSERT(statement)											\
{																		\
	if (!statement)														\
	{																	\
		switch (kShowAssertMessage( #statement ))                       \
		{																\
		case CG::eAssertResponse::kBreak:								\
			__debugbreak();												\
			break;														\
		case CG::eAssertResponse::kContinue:							\
			std::cerr << #statement << std::endl;						\
			break;														\
		default:														\
			std::cerr << #statement << std::endl;						\
			break;														\
		}																\
	}																	\
}
#else
#define CG_ASSERT(statement) {}
#endif
#else
#define CG_ASSERT(statement) {}
#endif