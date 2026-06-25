#include "monitor.h"

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <cstdio>
#include <ctime>
#include <vector>
#include <string>
#include <memory>

#pragma comment(lib, "psapi.lib")

namespace nemesis_loader {

// ============================================================
//  helpers
// ============================================================

static std::string Now()
{
    SYSTEMTIME st;
    GetLocalTime( &st );
    char buf[64];
    sprintf_s( buf, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );
    return buf;
}

static const char* DescribeExitCode( DWORD code )
{
    switch (code)
    {
        case STILL_ACTIVE:  return "STILL_ACTIVE (process still alive)";
        case 0:             return "clean exit (0)";
        case 0x80000002:    return "STATUS_DATATYPE_MISALIGNMENT (warning) — context likely corrupted by Thread Hijack";
        case 0x80000003:    return "STATUS_BREAKPOINT (INT 3) — debug breakpoint hit";
        case 0x80000004:    return "STATUS_SINGLE_STEP (TF flag was set) — single-step trap";
        case 0xC0000005:    return "STATUS_ACCESS_VIOLATION — invalid memory access";
        case 0xC0000008:    return "STATUS_INVALID_HANDLE";
        case 0xC000001D:    return "STATUS_ILLEGAL_INSTRUCTION";
        case 0xC0000025:    return "STATUS_NONCONTINUABLE_EXCEPTION";
        case 0xC0000026:    return "STATUS_INVALID_DISPOSITION";
        case 0xC000008C:    return "STATUS_ARRAY_BOUNDS_EXCEEDED";
        case 0xC0000094:    return "STATUS_INTEGER_DIVIDE_BY_ZERO";
        case 0xC00000FD:    return "STATUS_STACK_OVERFLOW";
        case 0xC0000135:    return "STATUS_DLL_NOT_FOUND";
        case 0xC0000139:    return "STATUS_ENTRYPOINT_NOT_FOUND";
        case 0xC0000142:    return "STATUS_DLL_INIT_FAILED";
        case 0xC0000409:    return "STATUS_STACK_BUFFER_OVERRUN (/GS check failed)";
        case 0xC0000417:    return "STATUS_INVALID_CRUNTIME_PARAMETER";
        case 0xC0000374:    return "STATUS_HEAP_CORRUPTION";
        case 0xC015000F:    return "STATUS_SXS_EARLY_DEACTIVATION";
        case 0xC0000420:    return "STATUS_ASSERTION_FAILURE";
        default:            return "(unknown / app-defined)";
    }
}

struct ThreadSnap
{
    DWORD tid;
    DWORD startAddrLow;  // best-effort
};

static std::vector<ThreadSnap> SnapshotThreads( DWORD pid )
{
    std::vector<ThreadSnap> out;
    HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if (snap == INVALID_HANDLE_VALUE) return out;

    THREADENTRY32 te = { sizeof( te ) };
    if (Thread32First( snap, &te ))
    {
        do {
            if (te.th32OwnerProcessID == pid)
            {
                ThreadSnap t;
                t.tid = te.th32ThreadID;
                t.startAddrLow = 0;
                out.push_back( t );
            }
        } while (Thread32Next( snap, &te ));
    }
    CloseHandle( snap );
    return out;
}

static std::vector<std::wstring> SnapshotModules( DWORD pid )
{
    std::vector<std::wstring> out;
    HANDLE h = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid );
    if (!h)
        h = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid );
    if (!h) return out;

    HMODULE mods[1024] = {};
    DWORD needed = 0;
    if (EnumProcessModulesEx( h, mods, sizeof( mods ), &needed, LIST_MODULES_ALL ))
    {
        DWORD n = needed / sizeof( HMODULE );
        if (n > 1024) n = 1024;
        for (DWORD i = 0; i < n; ++i)
        {
            wchar_t name[MAX_PATH] = {};
            if (GetModuleFileNameExW( h, mods[i], name, _countof( name ) ) > 0)
            {
                out.push_back( name );
            }
            else if (GetModuleBaseNameW( h, mods[i], name, _countof( name ) ) > 0)
            {
                out.push_back( std::wstring( L"<no path> " ) + name );
            }
            else
            {
                wchar_t fallback[64];
                swprintf_s( fallback, L"<unknown module @ 0x%p>", mods[i] );
                out.push_back( fallback );
            }
        }
    }
    CloseHandle( h );
    return out;
}

static SIZE_T QueryWorkingSet( DWORD pid )
{
    HANDLE h = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
    if (!h) return 0;
    PROCESS_MEMORY_COUNTERS pmc = {};
    GetProcessMemoryInfo( h, &pmc, sizeof( pmc ) );
    CloseHandle( h );
    return pmc.WorkingSetSize;
}

// ============================================================
//  writer
// ============================================================

class ReportWriter
{
public:
    explicit ReportWriter( const std::wstring& path )
    {
        _h = CreateFileW( path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                          nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
    }
    ~ReportWriter()
    {
        if (_h && _h != INVALID_HANDLE_VALUE) CloseHandle( _h );
    }
    bool ok() const { return _h && _h != INVALID_HANDLE_VALUE; }

    void Line( const char* line )
    {
        if (!ok()) return;
        DWORD w;
        WriteFile( _h, line, (DWORD)strlen( line ), &w, nullptr );
        WriteFile( _h, "\r\n", 2, &w, nullptr );
    }
    void LineW( const std::wstring& s )
    {
        if (!ok()) return;
        // narrow to UTF-8 for the report (good enough for module paths)
        int n = WideCharToMultiByte( CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr );
        std::string out( (size_t)n, '\0' );
        WideCharToMultiByte( CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n, nullptr, nullptr );
        DWORD w;
        WriteFile( _h, out.data(), (DWORD)out.size(), &w, nullptr );
        WriteFile( _h, "\r\n", 2, &w, nullptr );
    }
    void Header( const char* title )
    {
        char bar[64];
        sprintf_s( bar, "============================================================" );
        Line( bar );
        Line( title );
        Line( bar );
    }
    void Pair( const char* key, const char* val )
    {
        char buf[1024];
        sprintf_s( buf, "%-22s : %s", key, val );
        Line( buf );
    }
    void PairU( const char* key, unsigned long val )
    {
        char buf[256];
        sprintf_s( buf, "%-22s : %lu", key, val );
        Line( buf );
    }
    void PairHex( const char* key, unsigned long long val )
    {
        char buf[256];
        sprintf_s( buf, "%-22s : 0x%llX", key, val );
        Line( buf );
    }

private:
    HANDLE _h = nullptr;
};

// ============================================================
//  watcher thread
// ============================================================

struct WatcherCtx
{
    MonitorParams        params;
    ULONGLONG            startTickMs;
};

struct Snapshot
{
    ULONGLONG  ageMs;          // ms since monitor started
    DWORD      threadCount;
    DWORD      moduleCount;
    SIZE_T     workingSet;
};

// Live log path next to the report
static std::wstring LiveLogPath( const std::wstring& reportPath )
{
    std::wstring out = reportPath;
    size_t dot = out.find_last_of( L'.' );
    if (dot != std::wstring::npos) out.erase( dot );
    out += L".live.log";
    return out;
}

static void AppendLine( const std::wstring& path, const char* line )
{
    HANDLE h = CreateFileW( path.c_str(), FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer( h, 0, nullptr, FILE_END );
    DWORD w;
    WriteFile( h, line, (DWORD)strlen( line ), &w, nullptr );
    WriteFile( h, "\r\n", 2, &w, nullptr );
    CloseHandle( h );
}

static DWORD WINAPI WatcherThread( LPVOID raw )
{
    std::unique_ptr<WatcherCtx> ctx( reinterpret_cast<WatcherCtx*>(raw) );
    const MonitorParams& p = ctx->params;

    const std::wstring liveLog = LiveLogPath( p.reportPath );

    {
        char hdr[256];
        sprintf_s( hdr, "=== Crash monitor started, watching PID %lu (%s) ===",
                   p.pid, Now().c_str() );
        AppendLine( liveLog, hdr );
    }

    HANDLE h = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE,
        FALSE, p.pid );

    if (!h)
    {
        DWORD err1 = GetLastError();
        h = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
            FALSE, p.pid );
        if (!h)
        {
            DWORD err2 = GetLastError();
            char msg[256];
            sprintf_s( msg, "OpenProcess FAILED. err1=%lu err2=%lu — cannot monitor PID %lu.",
                       err1, err2, p.pid );
            AppendLine( liveLog, msg );

            ReportWriter w( p.reportPath );
            if (w.ok())
            {
                w.Header( "NEMESIS LOADER — CRASH MONITOR (failed to open target)" );
                w.Pair( "Time", Now().c_str() );
                w.PairU( "Target PID", p.pid );
                w.PairU( "OpenProcess error (full)",    err1 );
                w.PairU( "OpenProcess error (limited)", err2 );
                w.Line( "Try running loader as administrator." );
            }
            return 1;
        }
        AppendLine( liveLog, "OpenProcess: full failed, using limited handle (no module info will be available)." );
    }
    else
    {
        AppendLine( liveLog, "OpenProcess: got full handle." );
    }

    // poll periodically and snapshot fresh state — we want the LAST known
    // good state before the crash, not the post-mortem corpse.
    std::vector<ThreadSnap>      lastThreads;
    std::vector<std::wstring>    lastModules;
    SIZE_T                       lastWorkingSet = 0;
    ULONGLONG                    lastSnapshotTick = 0;

    // Keep a rolling history of last N snapshots
    constexpr size_t kHistoryCap = 20;
    std::vector<Snapshot> history;
    history.reserve( kHistoryCap );

    int beat = 0;
    while (true)
    {
        DWORD rc = WaitForSingleObject( h, 500 ); // poll every 500ms
        if (rc == WAIT_OBJECT_0) break;          // target died
        if (rc != WAIT_TIMEOUT)
        {
            char msg[128];
            sprintf_s( msg, "WaitForSingleObject failed: rc=0x%lX err=%lu", rc, GetLastError() );
            AppendLine( liveLog, msg );
            break;
        }

        // refresh snapshot every second
        if (GetTickCount64() - lastSnapshotTick > 1000)
        {
            lastThreads      = SnapshotThreads( p.pid );
            lastModules      = SnapshotModules( p.pid );
            lastWorkingSet   = QueryWorkingSet( p.pid );
            lastSnapshotTick = GetTickCount64();

            Snapshot s;
            s.ageMs       = GetTickCount64() - ctx->startTickMs;
            s.threadCount = (DWORD)lastThreads.size();
            s.moduleCount = (DWORD)lastModules.size();
            s.workingSet  = lastWorkingSet;

            if (history.size() >= kHistoryCap) history.erase( history.begin() );
            history.push_back( s );

            ++beat;
            char beatLine[256];
            sprintf_s( beatLine,
                "  [t=%llums] threads=%lu modules=%lu workingSet=%llu  (beat #%d)",
                (unsigned long long)s.ageMs, s.threadCount, s.moduleCount,
                (unsigned long long)s.workingSet, beat );
            AppendLine( liveLog, beatLine );
        }
    }

    AppendLine( liveLog, "=== Target died — generating final report ===" );

    // Target died — collect post-mortem
    DWORD exitCode = 0;
    GetExitCodeProcess( h, &exitCode );
    ULONGLONG elapsedMs = GetTickCount64() - ctx->startTickMs;

    ReportWriter w( p.reportPath );
    if (!w.ok())
    {
        CloseHandle( h );
        return 1;
    }

    w.Header( "NEMESIS LOADER — TARGET CRASH REPORT" );
    w.Pair( "Generated",         Now().c_str() );

    // ---- target identity ----
    w.Header( "TARGET" );
    char tmp[512];
    sprintf_s( tmp, "%ls", p.targetName.c_str() );
    w.Pair( "Name",              tmp );
    w.PairU( "PID",              p.pid );

    // ---- inject summary ----
    w.Header( "INJECTION SUMMARY" );
    sprintf_s( tmp, "%ls", p.dllName.c_str() );
    w.Pair( "Injected DLL",      tmp );
    w.PairHex( "Image base",     p.injectBaseAddr );
    w.PairHex( "Image size",     p.injectImgSize );

    sprintf_s( tmp, "%llu ms (%.2f s)", (unsigned long long)elapsedMs, elapsedMs / 1000.0 );
    w.Pair( "Survived after inject", tmp );

    // ---- exit info ----
    w.Header( "EXIT" );
    sprintf_s( tmp, "0x%08lX (%lu)", exitCode, exitCode );
    w.Pair( "Exit code",         tmp );
    w.Pair( "Exit meaning",      DescribeExitCode( exitCode ) );

    // High bit set + small value → likely an NTSTATUS exception code
    if ((exitCode & 0xF0000000) == 0xC0000000)
        w.Line( "Note: looks like an unhandled native exception (NTSTATUS C0000xxx)." );
    else if ((exitCode & 0xF0000000) == 0x80000000)
        w.Line( "Note: looks like a warning-level NTSTATUS (80000xxx)." );

    // ---- snapshot just before crash ----
    w.Header( "LAST-KNOWN-GOOD SNAPSHOT (taken <=1s before crash)" );
    sprintf_s( tmp, "%llu bytes", (unsigned long long)lastWorkingSet );
    w.Pair( "Working set",       tmp );
    w.PairU( "Thread count",     (unsigned long)lastThreads.size() );
    w.PairU( "Module count",     (unsigned long)lastModules.size() );

    // ---- timeline ----
    w.Header( "TIMELINE (last 20 snapshots, 1/sec)" );
    if (history.empty())
    {
        w.Line( "(no snapshots — target died too quickly)" );
    }
    else
    {
        w.Line( "  t(ms)        threads   modules   workingSet" );
        for (auto& s : history)
        {
            char ln[256];
            sprintf_s( ln, "  %-12llu %-9lu %-9lu %llu",
                       (unsigned long long)s.ageMs,
                       (unsigned long)s.threadCount,
                       (unsigned long)s.moduleCount,
                       (unsigned long long)s.workingSet );
            w.Line( ln );
        }
        // Detect deltas in the last few entries
        if (history.size() >= 2)
        {
            const auto& a = history.front();
            const auto& z = history.back();
            char delta[256];
            sprintf_s( delta, "Delta over %llu ms: threads %+ld, modules %+ld, workingSet %+lld",
                       (unsigned long long)(z.ageMs - a.ageMs),
                       (long)z.threadCount - (long)a.threadCount,
                       (long)z.moduleCount - (long)a.moduleCount,
                       (long long)z.workingSet - (long long)a.workingSet );
            w.Line( delta );
        }
    }

    // ---- module list ----
    w.Header( "MODULES (last snapshot)" );
    bool sawOurDll = false;
    for (auto& m : lastModules)
    {
        w.LineW( m );
        // crude case-insensitive substring match for our DLL name
        std::wstring lo = m;
        for (auto& c : lo) c = (wchar_t)towlower( c );
        std::wstring needle = p.dllName;
        for (auto& c : needle) c = (wchar_t)towlower( c );
        if (lo.find( needle ) != std::wstring::npos)
            sawOurDll = true;
    }

    w.Header( "DIAGNOSIS HINTS" );
    if (sawOurDll)
        w.Line( "OK: our DLL was present in the module list before the crash." );
    else
        w.Line( "WARN: our DLL was NOT visible in the module list — either MapImage's HideVAD/unlink hid it, or it was never properly inserted." );

    switch (exitCode)
    {
        case 0x80000002:
            w.Line( "Likely cause: Thread-Hijack restored a corrupted CONTEXT — try injecting later, or use a thread that is not in syscall/wait." );
            break;
        case 0x80000003:
        case 0x80000004:
            w.Line( "Likely cause: hijacked thread returned to an address with INT 3 / TF flag set. Check that EFlags were preserved correctly." );
            break;
        case 0xC0000005:
            w.Line( "Likely cause: AV in our DLL OR in code reached after the hijacked thread returned to its original RIP. Check NemesisLoader.log for SEH entries first." );
            break;
        case 0xC0000409:
            w.Line( "Likely cause: stack buffer overrun. Our shellcode RSP delta may misalign target's stack. /GS check tripped." );
            break;
        case 0xC0000374:
            w.Line( "Likely cause: heap corruption — host's heap may have been freed across stale TLS or LDR entry from our manual-mapped DLL." );
            break;
        case 0xC0000139:
        case 0xC0000135:
            w.Line( "Likely cause: import resolution failed — our DLL's IAT references something the target doesn't have." );
            break;
        case 0xC0000142:
            w.Line( "Likely cause: a DllMain in a manually-mapped dependency returned FALSE." );
            break;
        default:
            w.Line( "No specific hint for this exit code — see 'Exit meaning' above." );
            break;
    }

    w.Header( "END OF REPORT" );

    CloseHandle( h );
    return 0;
}

// ============================================================
//  public entry
// ============================================================

HANDLE StartCrashMonitor( const MonitorParams& params )
{
    auto ctx = std::make_unique<WatcherCtx>();
    ctx->params      = params;
    ctx->startTickMs = GetTickCount64();

    HANDLE h = CreateThread( nullptr, 0, WatcherThread, ctx.get(), 0, nullptr );
    if (!h) return nullptr;
    ctx.release(); // ownership transferred to the thread
    return h;
}

} // namespace nemesis_loader
