#define _HAS_STD_BYTE 0
#define LOG(text) std::cout << text << std::endl;

#include "infector.h"
#include "hijacking.h"
#include "injector.h"

#include <string>
#include <iostream>
#include <fstream>
#include <winternl.h>

#define X(val, msg) msg,
const char* status_message[] = { HINFRES_VALUES };
#undef X

namespace Injector
{
    HINFRES Status = EMPTY;

    HANDLE procHandle = NULL;
    HANDLE hProcess = NULL;
    HANDLE HijackedHandle = NULL;

    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

    SYSTEM_HANDLE_INFORMATION* hInfo = nullptr;

    static OBJECT_ATTRIBUTES InitObjectAttributes(
        PUNICODE_STRING name,
        ULONG attribs,
        HANDLE hRoot,
        PSECURITY_DESCRIPTOR sec)
    {
        OBJECT_ATTRIBUTES object;
        object.Length = sizeof(OBJECT_ATTRIBUTES);
        object.ObjectName = name;
        object.Attributes = attribs;
        object.RootDirectory = hRoot;
        object.SecurityDescriptor = sec;
        return object;
    }

    bool IsFine(HINFRES result)
    {
        return result == SUCCESS;
    }

    bool IsHandleValid(HANDLE handle)
    {
        return handle && handle != INVALID_HANDLE_VALUE;
    }

    bool IsCorrectTargetArch(HANDLE hProc)
    {
        BOOL bTarget = FALSE;
        if (!IsWow64Process(hProc, &bTarget))
            return false;

        BOOL bHost = FALSE;
        IsWow64Process(GetCurrentProcess(), &bHost);
        return (bTarget == bHost);
    }

    DWORD GetProcId(const wchar_t* name)
    {
        DWORD id = 0;
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(PROCESSENTRY32W);

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
        if (Process32FirstW(hSnap, &entry) == TRUE)
        {
            do {
                if (_wcsicmp(entry.szExeFile, name) == 0)
                {
                    id = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &entry));
        }
        CloseHandle(hSnap);
        return id;
    }

    bool IsWin24H2OrG()
    {
        HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
        if (!hMod) return false;

        auto rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (!rtlGetVersion) return false;

        RTL_OSVERSIONINFOW osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        if (rtlGetVersion(&osvi) != 0) return false;

        if (osvi.dwMajorVersion > 10) return true;
        if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 26100) return true;
        return false;
    }

    bool IsWindows10OrLower()
    {
        HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
        if (!hMod) return false;

        auto rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (!rtlGetVersion) return false;

        RTL_OSVERSIONINFOW osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        if (rtlGetVersion(&osvi) != 0) return false;

        if (osvi.dwMajorVersion > 10) return false;
        if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 22000) return false;
        return true;
    }

    HANDLE HijackProcHandle(DWORD dwTargetProcessId)
    {
        HMODULE Ntdll = GetModuleHandleA("ntdll");

        auto RtlAdjustPrivilege = (_RtlAdjustPrivilege)GetProcAddress(Ntdll, "RtlAdjustPrivilege");
        boolean OldPriv;
        RtlAdjustPrivilege(SeDebugPriv, TRUE, FALSE, &OldPriv);

        auto NtQuerySystemInformation = (_NtQuerySystemInformation)GetProcAddress(Ntdll, "NtQuerySystemInformation");
        auto NtDuplicateObject = (_NtDuplicateObject)GetProcAddress(Ntdll, "NtDuplicateObject");
        auto NtOpenProcess = (_NtOpenProcess)GetProcAddress(Ntdll, "NtOpenProcess");

        OBJECT_ATTRIBUTES Obj_Attribute = InitObjectAttributes(NULL, NULL, NULL, NULL);
        CLIENT_ID clientID = { 0 };

        DWORD size = sizeof(SYSTEM_HANDLE_INFORMATION);
        hInfo = (SYSTEM_HANDLE_INFORMATION*)new byte[size];
        ZeroMemory(hInfo, size);

        NTSTATUS NtRet = NULL;
        do
        {
            delete[] hInfo;
            size = (DWORD)(size * 1.5);
            try {
                hInfo = (PSYSTEM_HANDLE_INFORMATION)new byte[size];
            }
            catch (std::bad_alloc&) {
                return nullptr;
            }
            Sleep(1);
        } while ((NtRet = NtQuerySystemInformation(
            SystemHandleInformation, hInfo, size, NULL))
            == STATUS_INFO_LENGTH_MISMATCH);

        if (!NT_SUCCESS(NtRet))
            return nullptr;

        for (unsigned int i = 0; i < hInfo->HandleCount; ++i)
        {
            static DWORD NumOfOpenHandles;
            GetProcessHandleCount(GetCurrentProcess(), &NumOfOpenHandles);

            if (!IsHandleValid((HANDLE)hInfo->Handles[i].Handle)
                || hInfo->Handles[i].ObjectTypeNumber != ProcessHandleType)
                continue;

            clientID.UniqueProcess = (HANDLE)(ULONG_PTR)hInfo->Handles[i].ProcessId;
            clientID.UniqueThread = nullptr;

            if (procHandle) CloseHandle(procHandle);

            NtRet = NtOpenProcess(&procHandle, PROCESS_DUP_HANDLE,
                &Obj_Attribute, &clientID);
            if (!IsHandleValid(procHandle) || !NT_SUCCESS(NtRet))
                continue;

            NtRet = NtDuplicateObject(procHandle,
                (HANDLE)hInfo->Handles[i].Handle,
                NtCurrentProcess, &HijackedHandle,
                PROCESS_ALL_ACCESS, 0, 0);
            if (!IsHandleValid(HijackedHandle) || !NT_SUCCESS(NtRet))
                continue;

            if (GetProcessId(HijackedHandle) != dwTargetProcessId)
            {
                CloseHandle(HijackedHandle);
                continue;
            }

            hProcess = HijackedHandle;
            break;
        }

        return hProcess;
    }

    HINFRES Inject(wchar_t* dll_path, const wchar_t* proc)
    {
        DWORD PID = GetProcId(proc);
        if (PID == 0)
            return PROC_NOT_FOUND;

        HANDLE hProc = HijackProcHandle(PID);
        if (!hProc)
            return PROC_CANT_OPEN;

        if (!IsCorrectTargetArch(hProc))
            return PROC_INVALID_ARCH;

        if (GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES)
            return DLL_NOT_FOUND;

        std::ifstream fDLL(dll_path, std::ios::binary | std::ios::ate);
        if (fDLL.fail())
        {
            fDLL.close();
            CloseHandle(hProc);
            return DLL_CANT_OPEN;
        }

        auto file_size = fDLL.tellg();
        if (file_size < 0x1000)
        {
            fDLL.close();
            CloseHandle(hProc);
            return DLL_FILESIZE;
        }

        BYTE* pSrcData = new BYTE[(UINT_PTR)file_size];
        if (!pSrcData)
        {
            fDLL.close();
            CloseHandle(hProc);
            return DLL_CANT_ALLOC;
        }

        fDLL.seekg(0, std::ios::beg);
        fDLL.read((char*)pSrcData, file_size);
        fDLL.close();

        if (!ManualMapDll(hProc, pSrcData, file_size))
        {
            delete[] pSrcData;
            CloseHandle(hProc);
            return MAPPING_ERR;
        }

        delete[] pSrcData;
        return SUCCESS;
    }
}