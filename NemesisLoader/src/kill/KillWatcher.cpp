#include "KillWatcher.hpp"

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../audio/KillSound.hpp"
#include "../address/AllUsedAddresses/AllUsedAddresses.hpp"

namespace Kill
{
    static int g_option = 1;

    static void ConLog( const char* fmt, ... )
    {
        char buf[256];
        va_list va; va_start( va, fmt );
        int n = _vsnprintf_s( buf, _TRUNCATE, fmt, va );
        va_end( va );
        if (n <= 0) return;
        HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE );
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return;
        DWORD w = 0;
        WriteFile( h, "[Kill] ", 7, &w, nullptr );
        WriteFile( h, buf, (DWORD)n, &w, nullptr );
        WriteFile( h, "\r\n", 2, &w, nullptr );
    }

    // Diagnostic state — print the chain values once every N reads so we don't
    // spam the console.
    static int  g_diagTick   = 0;
    static bool g_diagPrintNext = true;

    static int ReadKills()
    {
        HMODULE client = GetModuleHandleA( "client.dll" );
        if (!client) return -1;

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>( client );
        std::uintptr_t controller = 0;
        std::uintptr_t tracking   = 0;
        std::int32_t   kills      = 0;

        __try
        {
            controller = *reinterpret_cast<std::uintptr_t*>(
                base + Addr::Client::dwLocalPlayerController );
            if (!controller)
            {
                if (g_diagPrintNext) { ConLog( "chain: client=%p controller=NULL", (void*)base ); g_diagPrintNext = false; }
                return -1;
            }

            tracking = *reinterpret_cast<std::uintptr_t*>(
                controller + Addr::PlayerController::m_pActionTrackingServices );
            if (!tracking)
            {
                if (g_diagPrintNext) { ConLog( "chain: ctrl=%p tracking=NULL", (void*)controller ); g_diagPrintNext = false; }
                return -1;
            }

            kills = *reinterpret_cast<std::int32_t*>(
                tracking + Addr::ActionTracking::m_iKills );

            if (g_diagPrintNext)
            {
                ConLog( "chain: ctrl=%p track=%p raw_kills=%d (0x%X)",
                        (void*)controller, (void*)tracking, kills, (unsigned)kills );
                g_diagPrintNext = false;
            }

            // Sanity: real kill counter is small (0..200). Anything wild means
            // we are following a stale or mistyped offset.
            if (kills < 0 || kills > 1000) return -1;
            return kills;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (g_diagPrintNext) { ConLog( "chain: SEH exception while reading" ); g_diagPrintNext = false; }
            return -1;
        }
    }

    static DWORD WINAPI WatcherThread( LPVOID )
    {
        ConLog( "watcher started, option=%d", g_option );
        int prev = -1;
        int tick = 0;
        int lastReported = -2;
        for (;;)
        {
            Sleep( 100 );
            int kills = ReadKills();

            // Periodic heartbeat (every ~5s) so we can see what we read.
            if (++tick >= 50) { tick = 0; g_diagPrintNext = true;
                if (kills != lastReported) { ConLog( "m_iKills=%d  prev=%d", kills, prev ); lastReported = kills; } }

            if (kills < 0)         { prev = -1; continue; }
            if (prev < 0)          { prev = kills; ConLog( "baseline set: m_iKills=%d", kills ); continue; }
            if (kills > prev)
            {
                ConLog( "KILL DETECTED %d -> %d, playing sound option=%d", prev, kills, g_option );
                Audio::PlayKillSound( g_option );
            }
            else if (kills < prev)
            {
                ConLog( "counter reset %d -> %d (round wipe)", prev, kills );
            }
            prev = kills;
        }
    }

    void Start( int soundOption )
    {
        g_option = (soundOption == 2) ? 2 : 1;
        HANDLE h = CreateThread( nullptr, 0, WatcherThread, nullptr, 0, nullptr );
        if (h) CloseHandle( h );
    }
}
