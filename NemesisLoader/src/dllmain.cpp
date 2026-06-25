#include <windows.h>
#include <cstdio>

#include "address/AllUsedAddresses/AllUsedAddresses.hpp"
#include "config/Config.hpp"
#include "audio/KillSound.hpp"
#include "kill/KillWatcher.hpp"
#include "hook/SoundHook.hpp"

static const wchar_t* kLogPath = L"D:\\Nemesis\\Ready\\NemesisLoader.log";

// Logging master switch. Defaults to OFF so nothing is written before the
// config is loaded. WorkerThread re-applies the value from Nemesis.json.
static volatile LONG g_loggingEnabled = 0;

// ============================================================
//  Read local player name from CS2 client.dll memory.
//  Safe to call before the player is connected — returns nullptr.
// ============================================================
static const char* GetLocalPlayerName()
{
    HMODULE client = GetModuleHandleA( "client.dll" );
    if (!client) return nullptr;

    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>( client );

    __try
    {
        auto controller = *reinterpret_cast<std::uintptr_t*>(
            base + Addr::Client::dwLocalPlayerController );
        if (!controller) return nullptr;

        const char* name = reinterpret_cast<const char*>(
            controller + Addr::PlayerController::m_iszPlayerName );
        if (!name || !*name) return nullptr;
        return name;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

// ============================================================
//  Logging
// ============================================================
// Always echoes to the cmd console that OpenCmdInTarget creates.
// File logging is only active when Nemesis.json has logging_enabled=true.
static void LogLine( const char* line )
{
    HANDLE hCon = GetStdHandle( STD_OUTPUT_HANDLE );
    if (hCon != INVALID_HANDLE_VALUE && hCon != nullptr)
    {
        DWORD w = 0;
        WriteFile( hCon, "[Loader] ", 9, &w, nullptr );
        WriteFile( hCon, line, (DWORD)strlen( line ), &w, nullptr );
        WriteFile( hCon, "\r\n", 2, &w, nullptr );
    }

    if (!InterlockedCompareExchange( &g_loggingEnabled, 0, 0 )) return;
    HANDLE h = CreateFileW( kLogPath, FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer( h, 0, nullptr, FILE_END );
    DWORD w = 0;
    WriteFile( h, line, (DWORD)strlen( line ), &w, nullptr );
    WriteFile( h, "\r\n", 2, &w, nullptr );
    CloseHandle( h );
}

static void LogF( const char* fmt, ... )
{
    char buf[512];
    va_list va;
    va_start( va, fmt );
    int n = _vsnprintf_s( buf, _TRUNCATE, fmt, va );
    va_end( va );
    if (n > 0) LogLine( buf );
}

static DWORD LogException( EXCEPTION_POINTERS* xp, const char* where )
{
    if (!xp || !xp->ExceptionRecord)
        return EXCEPTION_EXECUTE_HANDLER;

    EXCEPTION_RECORD* er = xp->ExceptionRecord;
    LogF( "[SEH] (%s) code=0x%08lX  addr=0x%p  params=%lu",
          where, (unsigned long)er->ExceptionCode, er->ExceptionAddress,
          (unsigned long)er->NumberParameters );
    for (DWORD i = 0; i < er->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
        LogF( "[SEH]   param[%lu] = 0x%p", i, (void*)er->ExceptionInformation[i] );
    return EXCEPTION_EXECUTE_HANDLER;
}

// ============================================================
//  Run cmd "inside" the target process
//  - AllocConsole creates a console attached to the host process
//  - The new cmd.exe inherits that console (no CREATE_NEW_CONSOLE)
//  - cmd.exe becomes a CHILD of the target — closing target closes cmd
// ============================================================
static void OpenCmdInTarget()
{
    LogLine( "[Worker] OpenCmdInTarget begin" );

    // If host already has a console (rare for GUI app), reuse it.
    BOOL freshConsole = AllocConsole();
    if (freshConsole)
    {
        // Wire up CRT std handles to the new console (so writes from CRT work)
        FILE* dummy = nullptr;
        freopen_s( &dummy, "CONOUT$", "w", stdout );
        freopen_s( &dummy, "CONOUT$", "w", stderr );
        freopen_s( &dummy, "CONIN$",  "r", stdin );

        SetConsoleTitleW( L"NemesisLoader — embedded shell" );
        LogLine( "[Worker] AllocConsole OK (host had no console)" );
    }
    else
    {
        LogF( "[Worker] AllocConsole returned 0, err=%lu (host probably already had a console)",
              GetLastError() );
    }

    // Print marker into the new console
    HANDLE hOut = GetStdHandle( STD_OUTPUT_HANDLE );
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr)
    {
        const char* playerName = GetLocalPlayerName();
        if (!playerName) playerName = "<unknown>";

        char banner[256];
        int blen = sprintf_s( banner,
            "================================================\r\n"
            "  Hello user %s\r\n"
            "================================================\r\n",
            playerName );
        DWORD w;
        WriteFile( hOut, banner, blen, &w, nullptr );
    }

    // Spawn cmd as CHILD of target, sharing the same console
    STARTUPINFOW si = { sizeof( si ) };
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle( STD_INPUT_HANDLE );
    si.hStdOutput = GetStdHandle( STD_OUTPUT_HANDLE );
    si.hStdError  = GetStdHandle( STD_ERROR_HANDLE );

    PROCESS_INFORMATION pi = {};
    wchar_t cmdLine[256];
    swprintf_s( cmdLine, L"cmd.exe /K title cmd in PID %lu", GetCurrentProcessId() );

    // No CREATE_NEW_CONSOLE — cmd inherits target's console
    BOOL ok = CreateProcessW( nullptr, cmdLine, nullptr, nullptr,
                              TRUE /* inherit handles */, 0,
                              nullptr, nullptr, &si, &pi );
    if (ok)
    {
        LogF( "[Worker] cmd.exe spawned as child, PID=%lu, parent=PID %lu",
              pi.dwProcessId, GetCurrentProcessId() );
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
    }
    else
    {
        LogF( "[Worker] CreateProcess(cmd.exe) failed, err=%lu", GetLastError() );
    }

    LogLine( "[Worker] OpenCmdInTarget end" );
}

static DWORD WINAPI HeartbeatThread( LPVOID )
{
    ULONGLONG start = GetTickCount64();
    int i = 0;
    while (true)
    {
        Sleep( 1000 );
        ULONGLONG alive = (GetTickCount64() - start) / 1000;
        LogF( "[Heartbeat #%d] alive=%llus, thread_id=%lu, threads_in_host=approx",
              ++i, (unsigned long long)alive, GetCurrentThreadId() );
        // The last heartbeat in NemesisLoader.log shows when the host died.
    }
    return 0;
}

static DWORD WINAPI WorkerThread( LPVOID )
{
    // Open the cmd console FIRST so every subsequent log line is visible.
    __try
    {
        OpenCmdInTarget();
    }
    __except (LogException( GetExceptionInformation(), "WorkerThread" ))
    {
    }

    // Now the console exists — load config and announce state.
    Cfg::Config cfg = Cfg::Load();
    InterlockedExchange( &g_loggingEnabled, cfg.logging_enabled ? 1 : 0 );

    LogF( "[Worker] thread started, PID=%lu TID=%lu",
          GetCurrentProcessId(), GetCurrentThreadId() );
    LogF( "[Cfg] logging=%d sound_enabled=%d sound_hook_enabled=%d sound_option=%d",
          (int)cfg.logging_enabled, (int)cfg.sound_enabled,
          (int)cfg.sound_hook_enabled, cfg.sound_option );

    if (cfg.sound_enabled)
    {
        // Memory-poll watcher: detects kills via m_iKills delta and plays our
        // sound. Always safe — no code patching.
        Kill::Start( cfg.sound_option );
    }

    // Hook is opt-in (RISK if the RVA is stale on a patched CS2).
    if (cfg.sound_hook_enabled)
    {
        for (int i = 0; i < 50; ++i)
        {
            if (GetModuleHandleA( "soundsystem.dll" )) break;
            Sleep( 100 );
        }
        bool hooked = SoundHook::Install( cfg.sound_option, /*suppressOriginal*/ true );
        LogF( "[SoundHook] Install -> %s", hooked ? "OK" : "FAIL" );
    }

    // Heartbeat is opt-in: only useful when logging is on.
    if (cfg.logging_enabled)
    {
        LogLine( "[Worker] OpenCmd done, starting heartbeat" );
        HANDLE hb = CreateThread( nullptr, 0, HeartbeatThread, nullptr, 0, nullptr );
        if (hb) CloseHandle( hb );
    }

    LogLine( "[Worker] thread exit" );
    return 0;
}

// ============================================================
//  DllMain — minimal, defers everything to a worker thread
// ============================================================
BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID )
{
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;

    __try
    {
        DisableThreadLibraryCalls( hModule );
        Audio::Init( hModule );
        LogF( "[DllMain] PROCESS_ATTACH, PID=%lu TID=%lu",
              GetCurrentProcessId(), GetCurrentThreadId() );

        HANDLE h = CreateThread( nullptr, 0, WorkerThread, nullptr, 0, nullptr );
        if (h)
        {
            LogLine( "[DllMain] CreateThread OK" );
            CloseHandle( h );
        }
        else
        {
            LogF( "[DllMain] CreateThread FAILED, err=%lu", GetLastError() );
        }
    }
    __except (LogException( GetExceptionInformation(), "DllMain" ))
    {
    }

    return TRUE;
}
