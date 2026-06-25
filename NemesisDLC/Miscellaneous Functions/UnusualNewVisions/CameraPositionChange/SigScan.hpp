#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <Windows.h>
#include <Psapi.h>

namespace Nemesis::Sig
{
    inline bool ParsePattern(std::string_view pattern, std::vector<int>& out)
    {
        out.clear();
        size_t i = 0;
        while (i < pattern.size())
        {
            const char c = pattern[i];
            if (c == ' ')
            {
                ++i;
                continue;
            }
            if (c == '?')
            {
                out.push_back(-1);
                ++i;
                if (i < pattern.size() && pattern[i] == '?')
                    ++i;
                continue;
            }

            auto hexVal = [](char ch) -> int
            {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };

            if (i + 1 >= pattern.size())
                return false;

            const int hi = hexVal(pattern[i]);
            const int lo = hexVal(pattern[i + 1]);
            if (hi < 0 || lo < 0)
                return false;

            out.push_back((hi << 4) | lo);
            i += 2;
        }
        return !out.empty();
    }

    inline std::uintptr_t Find(const char* moduleName, std::string_view pattern)
    {
        std::vector<int> bytes;
        if (!ParsePattern(pattern, bytes))
            return 0;

        const HMODULE mod = GetModuleHandleA(moduleName);
        if (!mod)
            return 0;

        MODULEINFO info{};
        if (!GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info)))
            return 0;

        const auto base = reinterpret_cast<std::uint8_t*>(mod);
        const size_t size = info.SizeOfImage;
        const size_t count = bytes.size();

        for (size_t i = 0; i + count <= size; ++i)
        {
            bool matched = true;
            for (size_t j = 0; j < count; ++j)
            {
                if (bytes[j] != -1 && base[i + j] != static_cast<std::uint8_t>(bytes[j]))
                {
                    matched = false;
                    break;
                }
            }
            if (matched)
                return reinterpret_cast<std::uintptr_t>(base + i);
        }
        return 0;
    }
}
