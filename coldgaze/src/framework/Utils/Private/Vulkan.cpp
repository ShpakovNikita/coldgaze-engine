#include "Utils\Vulkan.hpp"
#include <string>
#include <iostream>
#include <sstream>

namespace CG
{
    namespace VkUtils
    {
        namespace Debug
        {
            PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback = VK_NULL_HANDLE;
            PFN_vkDestroyDebugReportCallbackEXT destroyDebugReportCallback = VK_NULL_HANDLE;
            PFN_vkDebugReportMessageEXT dbgBreakCallback = VK_NULL_HANDLE;

            VkDebugReportCallbackEXT msgCallback;

            VKAPI_ATTR VkBool32 VKAPI_CALL MessageCallback(
                VkDebugReportFlagsEXT flags,
                VkDebugReportObjectTypeEXT objType,
                uint64_t srcObject,
                size_t location,
                int32_t msgCode,
                const char* pLayerPrefix,
                const char* pMsg,
                void* pUserData)
            {
                // Select prefix depending on flags passed to the callback
                // Note that multiple flags may be set for a single validation message
                std::string prefix("");

                // Error that may result in undefined behaviour
                if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
                {
                    prefix += "ERROR:";
                };
                // Warnings may hint at unexpected / non-spec API usage
                if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
                {
                    prefix += "WARNING:";
                };
                // May indicate sub-optimal usage of the API
                if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
                {
                    prefix += "PERFORMANCE:";
                };
                // Informal messages that may become handy during debugging
                if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
                {
                    prefix += "INFO:";
                }
                // Diagnostic info from the Vulkan loader and layers
                // Usually not helpful in terms of API usage, but may help to debug layer and loader problems 
                if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
                {
                    prefix += "DEBUG:";
                }

                // Display message to default output (console/logcat)
                std::stringstream debugMessage;
                debugMessage << prefix << " [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg;

#if defined(__ANDROID__)
                if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
                    LOGE("%s", debugMessage.str().c_str());
                }
                else {
                    LOGD("%s", debugMessage.str().c_str());
                }
#else
                if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
                    std::cerr << debugMessage.str() << "\n";
                }
                else {
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
