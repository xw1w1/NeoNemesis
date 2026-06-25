#include "LogsSystem.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <Windows.h>

namespace Nemesis::Logs
{
    namespace
    {
        std::atomic<bool> g_enabled{ false };
        std::mutex        g_mutex;
        FILE*             g_out = nullptr;
        FILE*             g_err = nullptr;

        void OpenConsole()
        {
            if (g_out)
                return;

            AllocConsole();
            SetConsoleTitleA("NemesisLoader :: Logs");
            freopen_s(&g_out, "CONOUT$", "w", stdout);
            freopen_s(&g_err, "CONOUT$", "w", stderr);
        }

        void CloseConsole()
        {
            if (g_out)
            {
                fclose(g_out);
                g_out = nullptr;
            }
            if (g_err)
            {
                fclose(g_err);
                g_err = nullptr;
            }
            FreeConsole();
        }

        void Timestamp(char* buffer, size_t size)
        {
            std::time_t now = std::time(nullptr);
            std::tm     tm{};
            localtime_s(&tm, &now);
            std::strftime(buffer, size, "%H:%M:%S", &tm);
        }
    }

    void SetEnabled(bool enabled)
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (enabled == g_enabled.load())
            return;

        if (enabled)
            OpenConsole();
        else
            CloseConsole();

        g_enabled.store(enabled);
    }

    bool IsEnabled()
    {
        return g_enabled.load();
    }

    void Write(const char* level, const char* fmt, ...)
    {
        if (!g_enabled.load())
            return;

        std::lock_guard<std::mutex> lock(g_mutex);

        if (!g_out)
            return;

        char time[16];
        Timestamp(time, sizeof(time));

        char body[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(body, sizeof(body), fmt, args);
        va_end(args);

        std::fprintf(g_out, "[%s][%s] %s\n", time, level, body);
        std::fflush(g_out);
    }
}
