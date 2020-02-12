#include "vulkan\vulkan_core.h"
#include <iostream>
#include <string>
#include <cassert>

#if defined(__ANDROID__)
#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		LOGE("Fatal : VkResult is \" %s \" in %s at line %d", vks::tools::errorString(res).c_str(), __FILE__, __LINE__); \
		assert(res == VK_SUCCESS);																		\
	}																									\
}
#else
#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << CG::Vk::Debug::ErrorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}
#endif

namespace CG
{
    namespace Vk
    {
        namespace Debug
        {
            // Default debug callback
            VKAPI_ATTR VkBool32 VKAPI_CALL MessageCallback(
                VkDebugReportFlagsEXT flags,
                VkDebugReportObjectTypeEXT objType,
                uint64_t srcObject,
                size_t location,
                int32_t msgCode,
                const char* pLayerPrefix,
                const char* pMsg,
                void* pUserData);

			std::string ErrorString(VkResult errorCode);
			std::string PhysicalDeviceTypeString(VkPhysicalDeviceType type);
            bool SetupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack);
        }
    }
}