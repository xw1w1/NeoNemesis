#include <windows.h>
#include <cstdio>

static void WriteMarker( const wchar_t* name )
{
    wchar_t path[MAX_PATH];
    swprintf_s( path, L"D:\\Nemesis\\Ready\\%s_marker.txt", name );

    HANDLE h = CreateFileW( path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
    if (h != INVALID_HANDLE_VALUE)
    {
        char buf[128];
        int len = sprintf_s( buf, "Injected! PID=%lu\r\n", GetCurrentProcessId() );
        DWORD written = 0;
        WriteFile( h, buf, len, &written, nullptr );
        CloseHandle( h );
    }
}

static void LaunchCmd()
{
    // Видимый эффект инжекта — открываем cmd.exe с сообщением "Пример мир".
    // CreateProcess не блокирует поток, поэтому DllMain вернётся быстро
    // (важно для Thread Hijack — мы не должны висеть в угнанном потоке).
    STARTUPINFOW si = { sizeof( si ) };
    PROCESS_INFORMATION pi = {};
    wchar_t cmdLine[] = L"cmd.exe /K echo Пример мир — injected from TestDll!";
    if (CreateProcessW( nullptr, cmdLine, nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi ))
    {
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
    }
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID )
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        WriteMarker( L"TestDll" );
        LoadLibraryW( L"SecondaryDll.dll" );
        LaunchCmd();
    }

    return TRUE;
}
