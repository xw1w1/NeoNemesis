#include <Process/Process.h>
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>

#include "monitor.h"

using namespace nemesis;

// ============================================================
//  Config — change here, not below
// ============================================================
static const wchar_t* kTargetExe = L"cs2.exe";
static const wchar_t* kDllName   = L"NemesisLoader.dll";

// ============================================================
//  Helpers
// ============================================================

static void Pause()
{
    printf( "\n[press Enter to exit]\n" );
    (void)getchar();
}

static const char* NtName( NTSTATUS s )
{
    switch ((uint32_t)s)
    {
        case 0x00000000: return "STATUS_SUCCESS";
        case 0xC0000001: return "STATUS_UNSUCCESSFUL";
        case 0xC0000005: return "STATUS_ACCESS_VIOLATION";
        case 0xC0000008: return "STATUS_INVALID_HANDLE";
        case 0xC000000D: return "STATUS_INVALID_PARAMETER";
        case 0xC0000017: return "STATUS_NO_MEMORY";
        case 0xC0000022: return "STATUS_ACCESS_DENIED";
        case 0xC0000034: return "STATUS_OBJECT_NAME_NOT_FOUND";
        case 0xC000003A: return "STATUS_OBJECT_PATH_NOT_FOUND";
        case 0xC000007B: return "STATUS_INVALID_IMAGE_FORMAT";
        case 0xC0000135: return "STATUS_DLL_NOT_FOUND";
        case 0xC0000139: return "STATUS_ENTRYPOINT_NOT_FOUND";
        case 0xC0000142: return "STATUS_DLL_INIT_FAILED";
        case 0xC000010A: return "STATUS_PROCESS_IS_TERMINATING";
        case 0xC000012F: return "STATUS_INVALID_IMAGE_NOT_MZ";
        case 0x8000000D: return "STATUS_PARTIAL_COPY";
        case 0x80000002: return "STATUS_DATATYPE_MISALIGNMENT";
        case 0x80000004: return "STATUS_SINGLE_STEP";
        default:         return "?";
    }
}

static void PrintWin32Error( const char* tag )
{
    DWORD err = GetLastError();
    if (!err) return;
    wchar_t* msg = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT ),
        reinterpret_cast<LPWSTR>(&msg), 0, nullptr );
    printf( "    %s: error %lu — %ls\n", tag, err,
            (len && msg) ? msg : L"(no message)" );
    if (msg) LocalFree( msg );
}

static bool EnableDebugPrivilege()
{
    HANDLE hToken;
    if (!OpenProcessToken( GetCurrentProcess(),
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ))
        return false;

    LUID luid;
    if (!LookupPrivilegeValueW( nullptr, L"SeDebugPrivilege", &luid ))
    {
        CloseHandle( hToken );
        return false;
    }

    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool ok = AdjustTokenPrivileges( hToken, FALSE, &tp, sizeof( tp ), nullptr, nullptr ) != 0
              && GetLastError() == ERROR_SUCCESS;
    CloseHandle( hToken );
    return ok;
}

// Find target PID by exe name. Returns 0 if not found.
static DWORD FindPidByName( const wchar_t* exeName )
{
    HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe = { sizeof( pe ) };
    DWORD pid = 0;

    if (Process32FirstW( snap, &pe ))
    {
        do {
            if (_wcsicmp( pe.szExeFile, exeName ) == 0)
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW( snap, &pe ));
    }
    CloseHandle( snap );
    return pid;
}

static bool IsTargetWow64( DWORD pid )
{
    HANDLE h = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
    if (!h) return false;
    BOOL wow = FALSE;
    IsWow64Process( h, &wow );
    CloseHandle( h );
    return wow == TRUE;
}

static bool ValidateDll( const wchar_t* dllName, wchar_t* outFullPath, size_t outCap )
{
    DWORD pathLen = GetFullPathNameW( dllName, (DWORD)outCap, outFullPath, nullptr );
    if (pathLen == 0 || pathLen >= outCap)
    {
        printf( "[X] DLL: GetFullPathName failed\n" );
        PrintWin32Error( "GetFullPathName" );
        return false;
    }

    DWORD attr = GetFileAttributesW( outFullPath );
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        printf( "[X] DLL not found: %ls\n", outFullPath );
        return false;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        printf( "[X] DLL path is a directory: %ls\n", outFullPath );
        return false;
    }

    HANDLE hFile = CreateFileW( outFullPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf( "[X] DLL: CreateFile failed\n" );
        PrintWin32Error( "CreateFile" );
        return false;
    }

    LARGE_INTEGER fsz = {};
    GetFileSizeEx( hFile, &fsz );
    unsigned char mz[2] = {};
    DWORD got = 0;
    BOOL ok = ReadFile( hFile, mz, 2, &got, nullptr );
    CloseHandle( hFile );

    if (!ok || got != 2)
    {
        printf( "[X] DLL: ReadFile failed (got %lu bytes)\n", got );
        return false;
    }
    if (mz[0] != 'M' || mz[1] != 'Z')
    {
        printf( "[X] DLL is not a valid PE — first 2 bytes are 0x%02X 0x%02X, expected 'MZ'\n", mz[0], mz[1] );
        printf( "    File size: %lld bytes\n", (long long)fsz.QuadPart );
        return false;
    }
    return true;
}

// ============================================================
//  Main
// ============================================================
int main()
{
    SetConsoleOutputCP( CP_UTF8 );
    setvbuf( stdout, nullptr, _IONBF, 0 );

    // ---- privileges
    bool dbgPriv = EnableDebugPrivilege();
    printf( "[*] SeDebugPrivilege : %s\n", dbgPriv ? "ok" : "FAILED (run as admin)" );

    // ---- validate DLL on disk
    wchar_t dllPath[MAX_PATH] = {};
    if (!ValidateDll( kDllName, dllPath, _countof( dllPath ) ))
    {
        Pause();
        return 1;
    }
    printf( "[*] DLL              : %ls\n", dllPath );

    // ---- find target
    printf( "[*] Looking for      : %ls\n", kTargetExe );
    DWORD pid = FindPidByName( kTargetExe );
    if (pid == 0)
    {
        printf( "\n[X] %ls is not running. Start it first, then run this loader.\n", kTargetExe );
        Pause();
        return 1;
    }
    printf( "[+] Found running    : PID = %lu\n", pid );

    // ---- check bitness match
    bool loaderIs64 = (sizeof( void* ) == 8);
    bool targetIs64 = !IsTargetWow64( pid );
    if (loaderIs64 != targetIs64)
    {
        printf( "\n[X] Bitness mismatch:\n" );
        printf( "    Loader is %s, target is %s\n",
                loaderIs64 ? "x64" : "x86",
                targetIs64 ? "x64" : "x86 (WoW64)" );
        printf( "    Use a loader matching the target architecture.\n" );
        Pause();
        return 1;
    }

    // ---- attach
    Process proc;
    NTSTATUS s = proc.Attach( pid );
    if (!NT_SUCCESS( s ))
    {
        printf( "\n[X] Attach failed: 0x%08X %s\n", s, NtName( s ) );
        PrintWin32Error( "Win32" );
        if ((uint32_t)s == 0xC0000022)
            printf( "    Likely cause: target runs as elevated/protected — try running loader as admin.\n" );
        Pause();
        return 1;
    }
    printf( "[+] Attached to PID  : %lu\n\n", pid );

    // ---- inject
    printf( "[>] MapImage %ls into PID %lu (Manual Map + Thread Hijack)\n", kDllName, pid );
    SetLastError( 0 );
    auto result = proc.mmap().MapImage( kDllName, NoThreads );

    if (!result.success())
    {
        printf( "\n[X] MapImage FAILED\n" );
        printf( "    NTSTATUS: 0x%08X  %s\n", result.status, NtName( result.status ) );

        // FormatMessage from ntdll
        wchar_t* msg = nullptr;
        HMODULE hNtdll = GetModuleHandleW( L"ntdll.dll" );
        DWORD len = FormatMessageW(
            FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            hNtdll, (DWORD)result.status, MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT ),
            reinterpret_cast<LPWSTR>(&msg), 0, nullptr );
        if (len && msg)
        {
            for (DWORD i = 0; i < len; ++i)
                if (msg[i] == L'\r' || msg[i] == L'\n') msg[i] = L' ';
            printf( "    Meaning : %ls\n", msg );
            LocalFree( msg );
        }
        PrintWin32Error( "Win32" );

        // target alive?
        DWORD exitCode = 0;
        HANDLE h = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
        if (h)
        {
            if (GetExitCodeProcess( h, &exitCode ))
                printf( "    Target  : %s (exit code 0x%08lX)\n",
                        exitCode == STILL_ACTIVE ? "ALIVE" : "DEAD",
                        exitCode );
            CloseHandle( h );
        }

        // hint based on NTSTATUS
        printf( "\n    Likely cause:\n" );
        switch ((uint32_t)result.status)
        {
            case 0xC0000022:
                printf( "    → Access denied. Run loader as administrator.\n" ); break;
            case 0xC000007B:
                printf( "    → DLL bitness doesn't match target.\n" ); break;
            case 0xC000012F:
                printf( "    → Target memory unreadable (process protected or already dead).\n" ); break;
            case 0xC0000135:
                printf( "    → DLL has a missing dependency. Use dumpbin /imports to check.\n" ); break;
            case 0xC0000139:
                printf( "    → DLL imports a function that doesn't exist in target's loaded modules.\n" ); break;
            case 0xC0000142:
                printf( "    → DllMain returned FALSE.\n" ); break;
            case 0x8000000D:
                printf( "    → Read/WriteProcessMemory was partially blocked — target has protected pages.\n" ); break;
            case 0xC0000001:
                printf( "    → Remote thread timeout — a DLL dependency hung loading inside target.\n" ); break;
            default:
                printf( "    → Unrecognized code, see meaning above.\n" ); break;
        }
        Pause();
        return 1;
    }

    printf( "\n[+] SUCCESS\n" );
    unsigned long long modBase = 0;
    unsigned           modSize = 0;
    if (result.result())
    {
        auto& mod = *result.result();
        modBase = (unsigned long long)mod.baseAddress;
        modSize = (unsigned)mod.size;
        printf( "    Image base : 0x%llX\n", modBase );
        printf( "    Image size : 0x%X\n",   modSize );
    }
    printf( "    Target PID %lu left running (we attached, didn't create).\n", pid );

    // ---- start crash monitor and wait ----
    nemesis_loader::MonitorParams mp;
    mp.pid             = pid;
    mp.targetName      = kTargetExe;
    mp.dllName         = kDllName;
    mp.injectBaseAddr  = modBase;
    mp.injectImgSize   = modSize;
    mp.reportPath      = L"D:\\Nemesis\\Ready\\crash_report.txt";

    HANDLE watcher = nemesis_loader::StartCrashMonitor( mp );
    if (!watcher)
    {
        printf( "[!] Could not start crash monitor (continuing anyway)\n" );
        return 0;
    }

    printf( "\n[*] Crash monitor watching target. Live log + final report:\n" );
    printf( "    Live log : D:\\Nemesis\\Ready\\crash_report.live.log\n" );
    printf( "    Report   : %ls\n", mp.reportPath.c_str() );
    printf( "[*] Press Ctrl+C to stop the loader (target keeps running).\n\n" );

    // Print a heartbeat every second so we know loader is alive
    ULONGLONG t0 = GetTickCount64();
    while (true)
    {
        DWORD rc = WaitForSingleObject( watcher, 1000 );
        if (rc == WAIT_OBJECT_0) break;
        ULONGLONG alive = (GetTickCount64() - t0) / 1000;
        printf( "    [loader heartbeat] target watched for %llus...\r",
                (unsigned long long)alive );
    }
    CloseHandle( watcher );

    printf( "\n\n[!] Target process died. Crash report written to:\n" );
    printf( "    %ls\n", mp.reportPath.c_str() );
    Pause();
    return 0;
}
