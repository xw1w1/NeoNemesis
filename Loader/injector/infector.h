#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include "hijacking.h"

#define HINFRES_VALUES \
X(EMPTY,            "EMPTY") \
X(BAD_HEAP_ALLOC,   "BAD_HEAP_ALLOC") \
X(NT_QUERYSYSINFO,  "NT_QUERYSYSINFO") \
X(HANDLE_LEAK_DET,  "HANDLE_LEAK_DET") \
X(PROC_NOT_FOUND,   "PROC_NOT_FOUND") \
X(PROC_CANT_OPEN,   "PROC_CANT_OPEN") \
X(PROC_INVALID_ARCH,"PROC_INVALID_ARCH") \
X(PROC_CANT_MAP,    "PROC_CANT_MAP") \
X(DLL_NOT_FOUND,    "DLL_NOT_FOUND") \
X(DLL_CANT_OPEN,    "DLL_CANT_OPEN") \
X(DLL_CANT_ALLOC,   "DLL_CANT_ALLOC") \
X(DLL_FILESIZE,     "DLL_FILESIZE") \
X(MAPPING_ERR,      "MAPPING_ERR") \
X(SUCCESS,          "SUCCESS")

#define X(val, msg) val,
enum HINFRES : size_t
{
    EMPTY,
    BAD_HEAP_ALLOC,
    NT_QUERYSYSINFO,
    HANDLE_LEAK_DET,
    PROC_NOT_FOUND,
    PROC_CANT_OPEN,
    PROC_INVALID_ARCH,
    PROC_CANT_MAP,
    DLL_NOT_FOUND,
    DLL_CANT_OPEN,
    DLL_CANT_ALLOC,
    DLL_FILESIZE,
    MAPPING_ERR,
    SUCCESS
};
#undef X

extern const char* status_message[];

namespace Injector
{
    extern HINFRES   Status;
    extern HANDLE    procHandle;
    extern HANDLE    hProcess;
    extern HANDLE    HijackedHandle;

    bool    IsFine(HINFRES result);
    bool    IsHandleValid(HANDLE handle);
    bool    IsCorrectTargetArch(HANDLE hProc);
    bool    IsWin24H2OrG();
    bool    IsWindows10OrLower();
    DWORD   GetProcId(const wchar_t* name);
    HANDLE  HijackProcHandle(DWORD dwTargetProcessId);
    HINFRES Inject(wchar_t* dll_path, const wchar_t* proc);
}