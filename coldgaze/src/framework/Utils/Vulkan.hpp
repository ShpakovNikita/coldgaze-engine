#include "vulkan\vulkan_core.h"


namespace CG
{
    namespace VkUtils
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

            bool SetupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack);
        }
    }
}