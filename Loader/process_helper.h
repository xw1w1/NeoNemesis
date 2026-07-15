#pragma once

#include <windows.h>

namespace ProcessHelper
{
    // Возвращает PID первого найденного процесса по имени, либо 0
    DWORD FindProcessId(const wchar_t* processName);

    // Возвращает true, если процесс с таким именем существует
    bool IsProcessRunning(const wchar_t* processName);

    // Возвращает HWND первого видимого top-level окна процесса, либо nullptr
    HWND FindVisibleWindowByProcessName(const wchar_t* processName);

    // Возвращает true, если у процесса есть видимое окно
    bool IsProcessWindowVisible(const wchar_t* processName);

    // Пытается завершить все процессы с таким именем
    // true, если удалось завершить хотя бы один
    bool KillProcessByName(const wchar_t* processName);
}

#pragma region Legacy Section
inline bool IsProcessRunning(const wchar_t* processName)
{
    return ProcessHelper::IsProcessRunning(processName);
}

inline bool IsProcessWindowVisible(const wchar_t* processName)
{
    return ProcessHelper::IsProcessWindowVisible(processName);
}

inline bool KillProcessByName(const wchar_t* processName)
{
    return ProcessHelper::KillProcessByName(processName);
}
#pragma endregion