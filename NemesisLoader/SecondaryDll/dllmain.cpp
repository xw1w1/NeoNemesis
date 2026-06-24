#include <windows.h>
#include <cstdio>

BOOL APIENTRY DllMain( HMODULE hModule, DWORD reason, LPVOID )
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        HANDLE h = CreateFileW(
            L"D:\\Nemesis\\Ready\\SecondaryDll_marker.txt",
            GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
        if (h != INVALID_HANDLE_VALUE)
        {
            char buf[128];
            int len = sprintf_s( buf, "SecondaryDll loaded! PID=%lu\r\n", GetCurrentProcessId() );
            DWORD written = 0;
            WriteFile( h, buf, len, &written, nullptr );
            CloseHandle( h );
        }
    }

    return TRUE;
}
