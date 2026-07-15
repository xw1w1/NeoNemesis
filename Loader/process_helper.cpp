#include "process_helper.h"

#include <tlhelp32.h>
#include <cwchar>

namespace
{
    struct WindowSearchData
    {
        DWORD processId = 0;
        HWND  foundWindow = nullptr;
    };

    BOOL CALLBACK EnumWindowsProc_FindVisibleProcessWindow(HWND hwnd, LPARAM lParam)
    {
        auto* data = reinterpret_cast<WindowSearchData*>(lParam);
        if (!data)
            return TRUE;

        DWORD windowProcessId = 0;
        GetWindowThreadProcessId(hwnd, &windowProcessId);

        if (windowProcessId != data->processId)
            return TRUE;

        if (!IsWindowVisible(hwnd))
            return TRUE;

        // Только top-level окно без owner
        if (GetWindow(hwnd, GW_OWNER) != nullptr)
            return TRUE;

        data->foundWindow = hwnd;
        return FALSE; // stop enumeration
    }

    HANDLE CreateProcessSnapshot()
    {
        return CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    }
}

namespace ProcessHelper
{
    DWORD FindProcessId(const wchar_t* processName)
    {
        if (!processName || !*processName)
            return 0;

        HANDLE snapshot = CreateProcessSnapshot();
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        DWORD result = 0;

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, processName) == 0)
                {
                    result = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return result;
    }

    bool IsProcessRunning(const wchar_t* processName)
    {
        return FindProcessId(processName) != 0;
    }

    HWND FindVisibleWindowByProcessName(const wchar_t* processName)
    {
        DWORD processId = FindProcessId(processName);
        if (processId == 0)
            return nullptr;

        WindowSearchData data{};
        data.processId = processId;

        EnumWindows(EnumWindowsProc_FindVisibleProcessWindow, reinterpret_cast<LPARAM>(&data));
        return data.foundWindow;
    }

    bool IsProcessWindowVisible(const wchar_t* processName)
    {
        return FindVisibleWindowByProcessName(processName) != nullptr;
    }

    bool KillProcessByName(const wchar_t* processName)
    {
        if (!processName || !*processName)
            return false;

        HANDLE snapshot = CreateProcessSnapshot();
        if (snapshot == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        bool killedAny = false;

        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, processName) != 0)
                    continue;

                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (!process)
                    continue;

                if (TerminateProcess(process, 0))
                    killedAny = true;

                CloseHandle(process);
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return killedAny;
    }
}