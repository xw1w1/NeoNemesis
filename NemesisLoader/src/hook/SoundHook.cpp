#include "SoundHook.hpp"

#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../address/AllUsedAddresses/AllUsedAddresses.hpp"
#include "../audio/KillSound.hpp"

#pragma comment(lib, "psapi.lib")

namespace SoundHook
{
    using StartSoundEventFn = void(__fastcall*)( void* self, const char* eventName );

    // Console-only logger — writes to the cmd window that dllmain opens via
    // AllocConsole(). No files are created.
    static void Diag( const char* fmt, ... )
    {
        char buf[1024];
        va_list va; va_start( va, fmt );
        int n = _vsnprintf_s( buf, _TRUNCATE, fmt, va );
        va_end( va );
        if (n <= 0) return;
        // Direct write to console handle (works even if CRT stdout was not
        // wired up). dllmain's OpenCmdInTarget already AllocConsole'd.
        HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE );
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return;
        DWORD w = 0;
        WriteFile( h, "[SoundHook] ", 12, &w, nullptr );
        WriteFile( h, buf, (DWORD)n, &w, nullptr );
        WriteFile( h, "\r\n", 2, &w, nullptr );
    }

    static void DumpModules()
    {
        HMODULE mods[1024];
        DWORD needed = 0;
        if (!EnumProcessModulesEx( GetCurrentProcess(), mods, sizeof( mods ), &needed, LIST_MODULES_64BIT ))
        {
            Diag( "[Diag] EnumProcessModulesEx failed, err=%lu", GetLastError() );
            return;
        }
        int n = (int)( needed / sizeof( HMODULE ) );
        Diag( "[Diag] loaded modules in cs2.exe: %d", n );
        for (int i = 0; i < n; ++i)
        {
            char path[MAX_PATH] = {};
            if (GetModuleFileNameExA( GetCurrentProcess(), mods[i], path, sizeof( path ) ))
            {
                MODULEINFO mi{};
                GetModuleInformation( GetCurrentProcess(), mods[i], &mi, sizeof( mi ) );
                Diag( "  %p  size=0x%X  %s", mods[i], mi.SizeOfImage, path );
            }
        }
    }

    static StartSoundEventFn g_trampoline      = nullptr;
    static BYTE*             g_target          = nullptr;
    static BYTE              g_savedBytes[5]   = {};
    static int               g_soundOption     = 1;
    static bool              g_suppressOrig    = true;

    // Print captured sound event name to the same cmd console.
    static void LogName( const char* name )
    {
        HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE );
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return;
        DWORD w = 0;
        WriteFile( h, "[Sound] ", 8, &w, nullptr );
        WriteFile( h, name, (DWORD)strlen( name ), &w, nullptr );
        WriteFile( h, "\r\n", 2, &w, nullptr );
    }

    // --- kill-sound matcher: case-insensitive substring scan ---
    static bool ContainsCI( const char* hay, const char* needle )
    {
        if (!hay || !needle) return false;
        for (; *hay; ++hay)
        {
            const char* h = hay; const char* n = needle;
            while (*h && *n)
            {
                char a = *h; char b = *n;
                if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
                if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
                if (a != b) break;
                ++h; ++n;
            }
            if (!*n) return true;
        }
        return false;
    }

    static bool IsKillSound( const char* name )
    {
        if (!name) return false;
        // Conservative initial pattern set. Refine after observing the log.
        return ContainsCI( name, "death" )
            || ContainsCI( name, "headshot" )
            || ContainsCI( name, "kill" );
    }

    // --- detour: runs INSTEAD of original prologue ---
    static void __fastcall Detour( void* self, const char* eventName )
    {
        if (eventName) LogName( eventName );

        if (g_suppressOrig && IsKillSound( eventName ))
        {
            Audio::PlayKillSound( g_soundOption );
            return; // do NOT call trampoline → original sound is suppressed
        }

        // Pass-through: invoke original function via trampoline.
        if (g_trampoline) g_trampoline( self, eventName );
    }

    // --- low-level: write JMP rel32 at addr → target ---
    static bool WriteJmpRel32( BYTE* at, void* target )
    {
        DWORD oldProt = 0;
        if (!VirtualProtect( at, 5, PAGE_EXECUTE_READWRITE, &oldProt )) return false;
        std::intptr_t rel = (std::intptr_t)target - ((std::intptr_t)at + 5);
        if (rel < INT32_MIN || rel > INT32_MAX)
        {
            VirtualProtect( at, 5, oldProt, nullptr );
            return false;
        }
        at[0] = 0xE9;
        std::int32_t rel32 = (std::int32_t)rel;
        std::memcpy( at + 1, &rel32, 4 );
        DWORD ignored = 0;
        VirtualProtect( at, 5, oldProt, &ignored );
        FlushInstructionCache( GetCurrentProcess(), at, 5 );
        return true;
    }

    bool Install( int soundOption, bool suppressOriginal )
    {
        Diag( "[Diag] Install called, option=%d suppress=%d", soundOption, (int)suppressOriginal );

        if (g_target) { Diag( "[Diag] already installed" ); return true; }

        g_soundOption  = (soundOption == 2) ? 2 : 1;
        g_suppressOrig = suppressOriginal;

        DumpModules();

        HMODULE ss = GetModuleHandleA( "soundsystem.dll" );
        Diag( "[Diag] GetModuleHandle(soundsystem.dll) = %p", ss );
        if (!ss)
        {
            // Try a few alternative names CS2 has used in the past.
            ss = GetModuleHandleA( "soundsystem_dx11.dll" );
            Diag( "[Diag] GetModuleHandle(soundsystem_dx11.dll) = %p", ss );
        }
        if (!ss) { Diag( "[Diag] FAIL: soundsystem module not present" ); return false; }

        BYTE* target = (BYTE*)ss + Addr::SoundSystem::StartSoundEvent;
        Diag( "[Diag] target = %p (base=%p + RVA=0x%X)",
              target, ss, (unsigned)Addr::SoundSystem::StartSoundEvent );

        // Strong sanity-check: full 17-byte prologue (rare combination,
        // negligible chance of accidental match elsewhere in soundsystem.dll).
        //   48 89 54 24 10   mov [rsp+10], rdx
        //   48 89 4C 24 08   mov [rsp+08], rcx
        //   55               push rbp
        //   57               push rdi
        //   48 8D 6C 24 B1   lea rbp, [rsp-4Fh]
        static const BYTE kExpected[17] = {
            0x48, 0x89, 0x54, 0x24, 0x10,
            0x48, 0x89, 0x4C, 0x24, 0x08,
            0x55,
            0x57,
            0x48, 0x8D, 0x6C, 0x24, 0xB1
        };
        Diag( "[Diag] target bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
              target[0], target[1], target[2], target[3], target[4],
              target[5], target[6], target[7], target[8], target[9],
              target[10], target[11], target[12], target[13], target[14],
              target[15], target[16] );
        if (std::memcmp( target, kExpected, 17 ) != 0)
        {
            Diag( "[Diag] FAIL: prologue mismatch — RVA is stale or CS2 was patched" );
            return false;
        }
        Diag( "[Diag] prologue OK (full 17-byte match)" );

        // Save original 5 bytes
        std::memcpy( g_savedBytes, target, 5 );

        // Allocate a trampoline: [orig 5 bytes][absolute JMP to target+5]
        // Absolute JMP = FF 25 00 00 00 00  +  qword(addr)  → 14 bytes
        const SIZE_T trSize = 5 + 14;
        BYTE* trampoline = (BYTE*)VirtualAlloc( nullptr, trSize,
                                                MEM_COMMIT | MEM_RESERVE,
                                                PAGE_EXECUTE_READWRITE );
        if (!trampoline) return false;

        std::memcpy( trampoline, g_savedBytes, 5 );
        trampoline[5]  = 0xFF;
        trampoline[6]  = 0x25;
        trampoline[7]  = 0x00; trampoline[8] = 0x00;
        trampoline[9]  = 0x00; trampoline[10] = 0x00;
        void* continuation = target + 5;
        std::memcpy( trampoline + 11, &continuation, 8 );
        FlushInstructionCache( GetCurrentProcess(), trampoline, trSize );

        g_trampoline = (StartSoundEventFn)trampoline;
        g_target     = target;

        if (!WriteJmpRel32( target, (void*)&Detour ))
        {
            Diag( "[Diag] FAIL: WriteJmpRel32" );
            VirtualFree( trampoline, 0, MEM_RELEASE );
            g_trampoline = nullptr;
            g_target     = nullptr;
            return false;
        }
        Diag( "[Diag] OK: hook installed" );
        return true;
    }

    void Uninstall()
    {
        if (!g_target) return;
        DWORD oldProt = 0;
        if (VirtualProtect( g_target, 5, PAGE_EXECUTE_READWRITE, &oldProt ))
        {
            std::memcpy( g_target, g_savedBytes, 5 );
            DWORD ignored = 0;
            VirtualProtect( g_target, 5, oldProt, &ignored );
            FlushInstructionCache( GetCurrentProcess(), g_target, 5 );
        }
        if (g_trampoline) VirtualFree( (void*)g_trampoline, 0, MEM_RELEASE );
        g_trampoline = nullptr;
        g_target     = nullptr;
    }
}
