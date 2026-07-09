#include "AdminConsole.hpp"
#include "FriendModels.hpp"
#include "../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace Nemesis::AdminConsole
{
    using namespace Nemesis::Addresses;

    namespace
    {
        HANDLE g_thread = nullptr;
        bool   g_run = false;
        bool   g_adminMode = false;

        std::string Trim(const std::string& s)
        {
            size_t a = 0, b = s.size();
            while (a < b && (unsigned char)s[a] <= ' ') ++a;
            while (b > a && (unsigned char)s[b - 1] <= ' ') --b;
            return s.substr(a, b - a);
        }

        bool StartsWith(const std::string& s, const char* pre)
        {
            return s.rfind(pre, 0) == 0;
        }

        void PrintMain()
        {
            std::printf("\n=== Nemesis Console ===\n");
            std::printf("  /AdminTools   - open Admin Tools commands\n");
            std::printf("  /close        - close this console permanently\n\n");
            std::fflush(stdout);
        }

        void PrintAdmin()
        {
            std::printf("\n=== Admin Tools ===\n");
            std::printf("  /cstatusMod              - export my SteamID + selected model to a config\n");
            std::printf("  /cstatusLoad \"<path>\"    - load a friend config (shows their model on them)\n\n");
            std::fflush(stdout);
        }

        DWORD WINAPI InputThread(LPVOID)
        {
            FILE* in = nullptr;
            freopen_s(&in, "CONIN$", "r", stdin);

            PrintMain();

            char buf[1024];
            while (g_run)
            {
                if (!std::fgets(buf, sizeof(buf), stdin))
                {
                    Sleep(50);
                    continue;
                }

                const std::string cmd = Trim(buf);
                if (cmd.empty())
                    continue;

                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);

                if (cmd == "/AdminTools")
                {
                    g_adminMode = true;
                    PrintAdmin();
                    continue;
                }

                if (cmd == "/close")
                {
                    std::printf("[console] closing.\n");
                    std::fflush(stdout);
                    g_run = false;
                    Nemesis::Logs::SetEnabled(false);
                    break;
                }

                if (g_adminMode)
                {
                    if (cmd == "/cstatusMod")
                    {
                        Nemesis::FriendModels::SaveMyConfig(base);
                        continue;
                    }
                    if (StartsWith(cmd, "/cstatusLoad"))
                    {
                        std::string path = Trim(cmd.substr(std::strlen("/cstatusLoad")));
                        if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
                            path = path.substr(1, path.size() - 2);
                        if (path.empty())
                            std::printf("[admin] usage: /cstatusLoad \"C:\\\\path\\\\config.json\"\n");
                        else
                            Nemesis::FriendModels::LoadConfig(path.c_str());
                        std::fflush(stdout);
                        continue;
                    }
                    std::printf("[admin] unknown command: %s\n", cmd.c_str());
                    std::fflush(stdout);
                }
                else
                {
                    std::printf("[console] type /AdminTools for admin commands, or /close\n");
                    std::fflush(stdout);
                }
            }
            return 0;
        }
    }

    void Start()
    {
        if (g_thread)
            return;
        Nemesis::Logs::SetEnabled(true);
        g_run = true;
        g_thread = CreateThread(nullptr, 0, InputThread, nullptr, 0, nullptr);
    }
}
