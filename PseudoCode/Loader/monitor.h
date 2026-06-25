#pragma once

#include <windows.h>
#include <string>

namespace nemesis_loader {

struct MonitorParams
{
    DWORD                 pid;
    std::wstring          targetName;     // e.g. "notepad.exe"
    std::wstring          dllName;        // e.g. "NemesisLoader.dll"
    unsigned long long    injectBaseAddr; // module base reported by MapImage
    unsigned              injectImgSize;  // module size reported by MapImage
    std::wstring          reportPath;     // where to write the report
};

// Starts a background watcher thread that observes the target process.
// When target dies (exit code != STILL_ACTIVE), it writes a detailed
// report to `params.reportPath`.
// Returns a HANDLE to the watcher thread (can be CloseHandle'd or
// WaitForSingleObject'd). Returns NULL on failure.
HANDLE StartCrashMonitor( const MonitorParams& params );

} // namespace nemesis_loader
