#pragma once

#include <cstdint>
#include <Windows.h>

namespace Nemesis::Mem
{
    inline std::uintptr_t ModuleBase(const char* moduleName)
    {
        return reinterpret_cast<std::uintptr_t>(GetModuleHandleA(moduleName));
    }

    template <typename T>
    inline T Read(std::uintptr_t address, T fallback = T{})
    {
        if (!address)
            return fallback;

        __try
        {
            return *reinterpret_cast<T*>(address);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return fallback;
        }
    }

    template <typename T>
    inline bool Write(std::uintptr_t address, const T& value)
    {
        if (!address)
            return false;

        __try
        {
            *reinterpret_cast<T*>(address) = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }
}
