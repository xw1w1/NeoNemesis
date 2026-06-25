#include "KillSound.hpp"
#include "resource.h"

#include <windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "winmm.lib")

namespace Audio
{
    static HMODULE g_self = nullptr;

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
        WriteFile( h, "[Audio] ", 8, &w, nullptr );
        WriteFile( h, buf, (DWORD)n, &w, nullptr );
        WriteFile( h, "\r\n", 2, &w, nullptr );
    }

    void Init( HMODULE selfModule )
    {
        g_self = selfModule;
    }

    static const void* LockWav( int option, DWORD* outSize )
    {
        if (!g_self) return nullptr;
        int rid = (option == 2) ? IDR_KILLSOUND_OPT2 : IDR_KILLSOUND_OPT1;
        HRSRC hRes = FindResourceA( g_self, MAKEINTRESOURCEA( rid ), "RCDATA" );
        if (!hRes) return nullptr;
        HGLOBAL hData = LoadResource( g_self, hRes );
        if (!hData) return nullptr;
        const void* p = LockResource( hData );
        if (outSize) *outSize = SizeofResource( g_self, hRes );
        return p;
    }

    void PlayKillSound( int option )
    {
        ConLog( "PlayKillSound(option=%d) called", option );
        if (!g_self) { ConLog( "  ABORT: g_self is null (Audio::Init was not called)" ); return; }

        DWORD size = 0;
        const void* wav = LockWav( option, &size );
        if (!wav || size == 0)
        {
            ConLog( "  ABORT: LockWav failed (resource missing?)" );
            return;
        }
        ConLog( "  wav loaded, size=%lu bytes", size );

        BOOL ok = PlaySoundA( (LPCSTR)wav, g_self, SND_MEMORY | SND_ASYNC | SND_NODEFAULT );
        ConLog( "  PlaySoundA returned %d (err=%lu)", (int)ok, GetLastError() );
    }
}
