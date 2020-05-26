#pragma once

#include <vcruntime_exception.h>


namespace CG
{
    namespace Vk
    {
        struct AssetLoadingException : public std::exception
        {
            AssetLoadingException(const char* msg) : std::exception(msg) {}
        };
    }
}
