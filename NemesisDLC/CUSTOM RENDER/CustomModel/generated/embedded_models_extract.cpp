#include "embedded_models.h"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <Windows.h>
#include <string>

namespace Nemesis { namespace EmbeddedModels {

    namespace
    {
        std::wstring GetCsgoRoot()
        {
            HMODULE client = GetModuleHandleW(L"client.dll");
            if (!client)
                return L"";
            wchar_t path[MAX_PATH] = {};
            if (!GetModuleFileNameW(client, path, MAX_PATH))
                return L"";

            std::wstring p = path;
            for (int i = 0; i < 3; ++i)
            {
                const size_t slash = p.find_last_of(L"\\/");
                if (slash == std::wstring::npos)
                    return L"";
                p.resize(slash);
            }
            return p;
        }

        void EnsureDirs(const std::wstring& fullPath)
        {
            for (size_t i = 3; i < fullPath.size(); ++i)
            {
                if (fullPath[i] == L'\\' || fullPath[i] == L'/')
                {
                    const std::wstring sub = fullPath.substr(0, i);
                    CreateDirectoryW(sub.c_str(), nullptr);
                }
            }
        }
    }

    void Deploy()
    {
        const std::wstring root = GetCsgoRoot();
        if (root.empty())
        {
            NERR("[embedmodels] csgo root not resolved");
            return;
        }

        int wrote = 0, skipped = 0;
        for (unsigned int f = 0; f < g_fileCount; ++f)
        {
            const FileEntry& e = g_files[f];

            std::wstring rel;
            for (const char* c = e.path; *c; ++c)
                rel += (*c == '/') ? L'\\' : static_cast<wchar_t>(*c);
            const std::wstring full = root + L"\\" + rel;

            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(full.c_str(), GetFileExInfoStandard, &fad))
            {
                const unsigned long long sz =
                    (static_cast<unsigned long long>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                if (sz == e.size)
                {
                    ++skipped;
                    continue;
                }
            }

            EnsureDirs(full);
            HANDLE h = CreateFileW(full.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE)
                continue;

            unsigned int off = e.offset;
            unsigned int remaining = e.size;
            while (remaining)
            {
                const unsigned int chunkIdx = off / g_chunkSize;
                const unsigned int local = off % g_chunkSize;
                unsigned int n = g_chunkSize - local;
                if (n > remaining)
                    n = remaining;
                DWORD written = 0;
                WriteFile(h, g_chunks[chunkIdx] + local, n, &written, nullptr);
                off += n;
                remaining -= n;
            }
            CloseHandle(h);
            ++wrote;
        }

        NLOG("[embedmodels] deploy done wrote=%d skipped=%d files=%u", wrote, skipped, g_fileCount);
    }

}}
