#include "Render/Vulkan/Debug.hpp"
#include <iostream>
#include <sstream>
#include <string>

namespace CG {
namespace Vk {
    namespace Debug {
        PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback = VK_NULL_HANDLE;
        PFN_vkDestroyDebugReportCallbackEXT destroyDebugReportCallback = VK_NULL_HANDLE;
        PFN_vkDebugReportMessageEXT dbgBreakCallback = VK_NULL_HANDLE;

        VkDebugReportCallbackEXT msgCallback;

        VKAPI_ATTR VkBool32 VKAPI_CALL MessageCallback(
            VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT,
            uint64_t,
            size_t,
            int32_t msgCode,
            const char* pLayerPrefix,
            const char* pMsg,
            void*)
        {
            // Select prefix depending on flags passed to the callback
            // Note that multiple flags may be set for a single validation message
            std::string prefix("");

            // Error that may result in undefined behaviour
            if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
                prefix += "ERROR:";
            };
            // Warnings may hint at unexpected / non-spec API usage
            if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
                prefix += "WARNING:";
            };
            // May indicate sub-optimal usage of the API
            if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
                prefix += "PERFORMANCE:";
            };
            // Informal messages that may become handy during debugging
            if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
                prefix += "INFO:";
            }
            // Diagnostic info from the Vulkan loader and layers
            // Usually not helpful in terms of API usage, but may help to debug layer and loader problems
            if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
                prefix += "DEBUG:";
            }

            // Display message to default output (console/logcat)
            std::stringstream debugMessage;
            debugMessage << prefix << " [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg;

#if defined(__ANDROID__)
            if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
                LOGE("%s", debugMessage.str().c_str());
            } else {
                LOGD("%s", debugMessage.str().c_str());
            }
#else
            if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
                std::cerr << debugMessage.str() << "\n";
            } else {
                std::cout << debugMessage.str() << "\n";
            }
#endif

            fflush(stdout);

            // The return value of this callback controls wether the Vulkan call that caused
            // the validation message will be aborted or not
            // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message
            // (and return a VkResult) to abort
            // If you instead want to have calls abort, pass in VK_TRUE and the function will
            // return VK_ERROR_VALIDATION_FAILED_EXT
            return VK_FALSE;
        }

        std::string ErrorString(VkResult errorCode)
        {
            switch (errorCode) {
#define STR(r)   \
    case VK_##r: \
        return #r
                STR(NOT_READY);
                STR(TIMEOUT);
                STR(EVENT_SET);
                STR(EVENT_RESET);
                STR(INCOMPLETE);
                STR(ERROR_OUT_OF_HOST_MEMORY);
                STR(ERROR_OUT_OF_DEVICE_MEMORY);
                STR(ERROR_INITIALIZATION_FAILED);
                STR(ERROR_DEVICE_LOST);
                STR(ERROR_MEMORY_MAP_FAILED);
                STR(ERROR_LAYER_NOT_PRESENT);
                STR(ERROR_EXTENSION_NOT_PRESENT);
                STR(ERROR_FEATURE_NOT_PRESENT);
                STR(ERROR_INCOMPATIBLE_DRIVER);
                STR(ERROR_TOO_MANY_OBJECTS);
                STR(ERROR_FORMAT_NOT_SUPPORTED);
                STR(ERROR_SURFACE_LOST_KHR);
                STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
                STR(SUBOPTIMAL_KHR);
                STR(ERROR_OUT_OF_DATE_KHR);
                STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
                STR(ERROR_VALIDATION_FAILED_EXT);
                STR(ERROR_INVALID_SHADER_NV);
#undef STR
            default:
                return "UNKNOWN_ERROR";
            }
        }

        std::string PhysicalDeviceTypeString(VkPhysicalDeviceType type)
        {
            switch (type) {
#define STR(r)                        \
    case VK_PHYSICAL_DEVICE_TYPE_##r: \
        return #r
                STR(OTHER);
                STR(INTEGRATED_GPU);
                STR(DISCRETE_GPU);
                STR(VIRTUAL_GPU);
#undef STR
            default:
                return "UNKNOWN_DEVICE_TYPE";
            }
        }

        bool SetupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack)
        {
            createDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
            destroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
            dbgBreakCallback = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT"));

            VkDebugReportCallbackCreateInfoEXT dbgCreateInfo = {};
            dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
            dbgCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)MessageCallback;
            dbgCreateInfo.flags = flags;

            VkResult err = createDebugReportCallback(
                instance,
                &dbgCreateInfo,
                nullptr,
                (callBack != VK_NULL_HANDLE) ? &callBack : &msgCallback);

            return err == VK_SUCCESS;
        }
    }
}
}
