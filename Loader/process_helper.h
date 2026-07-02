#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <shellapi.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Shell32.lib")

struct ProcessWindowSearch {
    DWORD processID;
    HWND foundWindow;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    ProcessWindowSearch* data = (ProcessWindowSearch*)lParam;
    DWORD procId = 0;
    GetWindowThreadProcessId(hwnd, &procId);
    if (procId == data->processID && IsWindowVisible(hwnd)) {
        // Дополнительно можно проверить класс или имя окна, если нужно.
        // Для cs2.exe главное окно обычно имеет заголовок "Counter-Strike 2".
        // Мы просто проверим наличие видимого окна.
        data->foundWindow = hwnd;
        return FALSE; // Stop
    }
    return TRUE; // Continue
}

// Проверяет, существует ли процесс с переданным именем
bool IsProcessRunning(const wchar_t* name)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool running = false;

    if (Process32FirstW(hSnap, &pe))
    {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0)
            {
                running = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return running;
}

// Проверяет, виден ли процесс с переданным именем
bool IsProcessWindowVisible(const wchar_t* name)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool windowVisible = false;

    if (Process32FirstW(hSnap, &pe))
    {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0)
            {
                ProcessWindowSearch searchData = { pe.th32ProcessID, NULL };
                EnumWindows(EnumWindowsProc, (LPARAM)&searchData);
                if (searchData.foundWindow != NULL)
                {
                    windowVisible = true;
                    break;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return windowVisible;
}

// Пытается убить процесс с переданным именем
bool KillProcessByName(const wchar_t* name)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool killed = false;

    if (Process32FirstW(hSnap, &pe))
    {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0)
            {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc)
                {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                    killed = true;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return killed;
}