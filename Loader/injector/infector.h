#define _HAS_STD_BYTE 0

#define LOG(text) std::cout << text << std::endl;

#include "injector.h"

#include "hijacking.h"

#include <string>
#include <iostream>
#include <fstream>     // <-- std::ifstream lives here; needed for Inject()

#define HINFRES_VALUES                                                       \
    X(EMPTY, "EMPTY")                                                        \
    X(BAD_HEAP_ALLOC, "BAD_HEAP_ALLOC")                                      \
    X(NT_QUERYSYSINFO, "NT_QUERYSYSINFO")                                    \
    X(HANDLE_LEAK_DET, "HANDLE_LEAK_DET")                                    \
    X(PROC_NOT_FOUND, "PROC_NOT_FOUND")                                      \
    X(PROC_CANT_OPEN, "PROC_CANT_OPEN")                                      \
    X(PROC_INVALID_ARCH, "PROC_INVALID_ARCH")                                \
    X(PROC_CANT_MAP, "PROC_CANT_MAP")                                        \
    X(DLL_NOT_FOUND, "DLL_NOT_FOUND")                                        \
    X(DLL_CANT_OPEN, "DLL_CANT_OPEN")                                        \
    X(DLL_CANT_ALLOC, "DLL_CANT_ALLOC")                                      \
    X(DLL_FILESIZE, "DLL_FILESIZE")                                          \
    X(MAPPING_ERR, "MAPPING_ERR")                                            \
    X(SUCCESS, "SUCCESS")

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

#define X(val, msg) msg,
const char* status_message[] = { HINFRES_VALUES };
#undef X

namespace Injector {

    HINFRES Status = EMPTY;

    bool IsFine(HINFRES result)
    {
        return result == SUCCESS;
    }

    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

    SYSTEM_HANDLE_INFORMATION* hInfo;
    HANDLE procHandle = NULL;
    HANDLE hProcess = NULL;
    HANDLE HijackedHandle = NULL;

    OBJECT_ATTRIBUTES InitObjectAttributes(PUNICODE_STRING name, ULONG attribs, HANDLE hRoot, PSECURITY_DESCRIPTOR sec)
    {
        OBJECT_ATTRIBUTES object;
        object.Length = sizeof(OBJECT_ATTRIBUTES);
        object.ObjectName = name;
        object.Attributes = attribs;
        object.RootDirectory = hRoot;
        object.SecurityDescriptor = sec;
        return object;
    }

    bool IsHandleValid(HANDLE handle)
    {
        if (handle && handle != INVALID_HANDLE_VALUE)
        {
            return true;
        }
        return false;
    }

    bool IsCorrectTargetArch(HANDLE hProc)
    {
        BOOL bTarget = FALSE;
        if (!IsWow64Process(hProc, &bTarget)) {
            LOG("Can't confirm target process acrhitecture: 0x%X", GetLastError());
            return false;
        }
        BOOL bHost = FALSE;
        IsWow64Process(GetCurrentProcess(), &bHost);
        return (bTarget == bHost);
    }

    DWORD GetProcId(const wchar_t* name)
    {
        DWORD id;
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(PROCESSENTRY32W);

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
        if (hSnap == INVALID_HANDLE_VALUE)
            return 0;

        if (Process32FirstW(hSnap, &entry) == TRUE)
        {
            do {
                if (_wcsicmp(entry.szExeFile, name) == 0)
                {
                    id = entry.th32ProcessID;
                    CloseHandle(hSnap);
                    return id;
                }
            } while (Process32NextW(hSnap, &entry));
        }
        CloseHandle(hSnap);
        return 0;
    }

    // Актуально только до версии Windows 11 24H2
    bool IsWin24H2OrG()
    {
        HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
        if (hMod) {
            auto rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
            if (rtlGetVersion) {
                RTL_OSVERSIONINFOW osvi = { 0 };
                osvi.dwOSVersionInfoSize = sizeof(osvi);
                if (rtlGetVersion(&osvi) == 0) {
                    if (osvi.dwMajorVersion > 10) return true;
                    if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 26100) return true;
                }
            }
        }
        return false;
    }

    bool IsWindows10OrLower()
    {
        HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
        if (hMod) {
            auto rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
            if (rtlGetVersion) {
                RTL_OSVERSIONINFOW osvi = { 0 };
                osvi.dwOSVersionInfoSize = sizeof(osvi);
                if (rtlGetVersion(&osvi) == 0) {
                    if (osvi.dwMajorVersion > 10) return false;
                    if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber >= 22000) return false;
                    return true;
                }
            }
        }
        return false;
    }

    // ---- Local function-pointer typedefs. Defined here, not in
    // hijacking.h, so that C-style / reinterpret_cast expressions below
    // never depend on how the header spells its own typedefs.
    // ----
    using fnRtlAdjustPrivilege =
        NTSTATUS(NTAPI*)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
    using fnNtQuerySystemInformation =
        NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);
    using fnNtDuplicateObject =
        NTSTATUS(NTAPI*)(HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK,
            ULONG, ULONG);
    using fnNtOpenProcess =
        NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, CLIENT_ID*);

    HANDLE HijackProcHandle(DWORD dwTargetProcessId)
    {
        HMODULE Ntdll = GetModuleHandleA("ntdll");
        if (!Ntdll) return nullptr;

        // Reinterpret_cast<fnXxx>(...) makes the cast type explicit and
        // does not require any token from hijacking.h to be a type-name.
        auto pRtlAdjustPrivilege = reinterpret_cast<fnRtlAdjustPrivilege>(
            GetProcAddress(Ntdll, "RtlAdjustPrivilege"));
        auto pNtQuerySystemInformation = reinterpret_cast<fnNtQuerySystemInformation>(
            GetProcAddress(Ntdll, "NtQuerySystemInformation"));
        auto pNtDuplicateObject = reinterpret_cast<fnNtDuplicateObject>(
            GetProcAddress(Ntdll, "NtDuplicateObject"));
        auto pNtOpenProcess = reinterpret_cast<fnNtOpenProcess>(
            GetProcAddress(Ntdll, "NtOpenProcess"));

        if (!pRtlAdjustPrivilege || !pNtQuerySystemInformation ||
            !pNtDuplicateObject || !pNtOpenProcess) {
            return nullptr;
        }

        BOOLEAN OldPriv = FALSE;
        pRtlAdjustPrivilege(SeDebugPriv, TRUE, FALSE, &OldPriv);

        OBJECT_ATTRIBUTES Obj_Attribute = InitObjectAttributes(NULL, NULL, NULL, NULL);
        CLIENT_ID clientID = { 0 };

        DWORD size = sizeof(SYSTEM_HANDLE_INFORMATION);
        hInfo = (PSYSTEM_HANDLE_INFORMATION) new byte[size];
        ZeroMemory(hInfo, size);

        NTSTATUS NtRet = STATUS_UNSUCCESSFUL;
        while ((NtRet = pNtQuerySystemInformation(
            SystemHandleInformation, hInfo, size, NULL))
            == STATUS_INFO_LENGTH_MISMATCH)
        {
            delete[] hInfo;
            size = (DWORD)(size * 1.5);
            try {
                hInfo = (PSYSTEM_HANDLE_INFORMATION) new byte[size];
            }
            catch (std::bad_alloc) {
                LOG("Bad Heap Allocation");
                return nullptr;
            }
            ZeroMemory(hInfo, size);
            Sleep(1);
        }

        if (!NT_SUCCESS(NtRet))
        {
            LOG("NtQuerySystemInformation Failed");
            return nullptr;
        }

        for (unsigned int i = 0; i < hInfo->HandleCount; ++i)
        {
            static DWORD NumOfOpenHandles;
            GetProcessHandleCount(GetCurrentProcess(), &NumOfOpenHandles);
            if (NumOfOpenHandles > 50)
            {
                LOG("Error Handle Leakage Detected");
            }

            if (!IsHandleValid((HANDLE)hInfo->Handles[i].Handle) ||
                hInfo->Handles[i].ObjectTypeNumber != ProcessHandleType)
                continue;

            clientID.UniqueProcess = (HANDLE)(ULONG_PTR)hInfo->Handles[i].ProcessId;
            clientID.UniqueThread = nullptr;

            if (procHandle) CloseHandle(procHandle);
            NtRet = pNtOpenProcess(&procHandle, PROCESS_DUP_HANDLE,
                &Obj_Attribute, &clientID);
            if (!IsHandleValid(procHandle) || !NT_SUCCESS(NtRet))
                continue;

            NtRet = pNtDuplicateObject(procHandle,
                (HANDLE)hInfo->Handles[i].Handle,
                NtCurrentProcess, &HijackedHandle,
                PROCESS_ALL_ACCESS, 0, 0);
            if (!IsHandleValid(HijackedHandle) || !NT_SUCCESS(NtRet))
                continue;

            if (GetProcessId(HijackedHandle) != dwTargetProcessId) {
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
        LOG("[Inject] target proc: " << proc << "\n");
        LOG("[Inject] dll path:    " << dll_path << "\n");

        // Resolve DLL path: if relative, anchor it next to the host .exe.
        wchar_t absDllPath[MAX_PATH] = { 0 };
        if (GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES) {
            wchar_t hostDir[MAX_PATH] = { 0 };
            if (GetModuleFileNameW(nullptr, hostDir, MAX_PATH) > 0) {
                wchar_t* slash = wcsrchr(hostDir, L'\\');
                if (slash) {
                    *slash = L'\0';
                    _snwprintf_s(absDllPath, _TRUNCATE, L"%s\\%s", hostDir, dll_path);
                    if (GetFileAttributesW(absDllPath) != INVALID_FILE_ATTRIBUTES) {
                        LOG("[Inject] Resolved DLL path -> " << absDllPath << "\n");
                        dll_path = absDllPath;
                    }
                }
            }
        }

        DWORD PID = GetProcId(proc);
        if (PID == 0)
        {
            LOG("[Inject] FAILED at GetProcId — process not found\n");
            return PROC_NOT_FOUND;
        }
        LOG("[Inject] Found proc PID: " << PID << "\n");

        // ---- First attempt: hijacked handle (the original path).
        HANDLE hProc = HijackProcHandle(PID);
        if (hProc)
        {
            HINFRES r = InjectWithHandle(hProc, dll_path);
            if (r == SUCCESS) return SUCCESS;

            // Hijack-specific failures fall through to the fallback.
            if (r != PROC_CANT_OPEN && r != PROC_INVALID_ARCH) {
                LOG("[Inject] ManualMapDll failed (status=" << r << "), NOT retrying via fallback\n");
                return r;
            }
            LOG("[Inject] Hijack path returned " << r << ", falling back to OpenProcess\n");
        }
        else
        {
            LOG("[Inject] HijackProcHandle returned NULL, falling back to OpenProcess\n");
        }

        // ---- Fallback: OpenProcess(PROCESS_ALL_ACCESS) directly.
        // Requires SeDebugPrivilege (which HijackProcHandle already enabled
        // earlier in this session, so it should be sticky). If the target
        // denies PROCESS_ALL_ACCESS, fall back to a smaller set.
        HANDLE hFallback = ::OpenProcess(
            PROCESS_ALL_ACCESS, FALSE, PID);
        if (!hFallback) {
            DWORD err = GetLastError();
            LOG("[Inject] OpenProcess(PROCESS_ALL_ACCESS) failed err=" << err
                << ", trying limited access set\n");
            hFallback = ::OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                FALSE, PID);
        }
        if (!hFallback) {
            DWORD err = GetLastError();
            LOG("[Inject] FAILED at PROC_CANT_OPEN (fallback) err=" << err << "\n");
            return PROC_CANT_OPEN;
        }
        LOG("[Inject] Fallback handle: 0x" << std::hex << (uintptr_t)hFallback << std::dec << "\n");

        HINFRES r = InjectWithHandle(hFallback, dll_path);
        ::CloseHandle(hFallback);
        return r;
    }

    // Shared core: maps `dll_path` into `hProc` via ManualMapDll.
    // Returns SUCCESS or one of the HINFRES values.
    HINFRES InjectWithHandle(HANDLE hProc, wchar_t* dll_path)
    {
        if (!IsCorrectTargetArch(hProc))
        {
            LOG("[Inject] FAILED at IsCorrectTargetArch — target/host arch mismatch\n");
            return PROC_INVALID_ARCH;
        }
        LOG("[Inject] Arch matches host\n");

        if (GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES)
        {
            DWORD attrErr = GetLastError();
            LOG("[Inject] FAILED at DLL_NOT_FOUND — " << dll_path
                << " (err=" << attrErr << ")\n");
            return DLL_NOT_FOUND;
        }
        LOG("[Inject] DLL exists: " << dll_path << "\n");

        std::ifstream fDLL(dll_path, std::ios::binary | std::ios::ate);
        if (fDLL.fail())
        {
            LOG("[Inject] FAILED at DLL_CANT_OPEN — ifstream open failed\n");
            return DLL_CANT_OPEN;
        }

        std::streampos file_size = fDLL.tellg();
        if (file_size < 0x1000) {
            LOG("[Inject] FAILED at DLL_FILESIZE — file too small: " << (long long)file_size << "\n");
            return DLL_FILESIZE;
        }
        LOG("[Inject] File size: " << (long long)file_size << " bytes\n");

        BYTE* pSrcData = new (std::nothrow) BYTE[static_cast<size_t>(file_size)];
        if (!pSrcData)
        {
            LOG("[Inject] FAILED at DLL_CANT_ALLOC — heap alloc failed for " << (long long)file_size << " bytes\n");
            return DLL_CANT_ALLOC;
        }

        fDLL.seekg(0, std::ios::beg);
        fDLL.read(reinterpret_cast<char*>(pSrcData), file_size);
        if (fDLL.gcount() != file_size) {
            LOG("[Inject] FAILED at DLL_CANT_OPEN — short read (got "
                << fDLL.gcount() << " of " << (long long)file_size << ")\n");
            delete[] pSrcData;
            return DLL_CANT_OPEN;
        }
        fDLL.close();

        LOG("[Inject] Mapping...\n");

        BOOL mapOk = ManualMapDll(
            hProc, pSrcData, static_cast<SIZE_T>(file_size),
            true,   // ClearHeader
            true,   // ClearNonNeededSections
            true,   // AdjustProtections
            true,   // SEHExceptionSupport
            DLL_PROCESS_ATTACH,
            nullptr);
        delete[] pSrcData;

        if (!mapOk) {
            LOG("[Inject] FAILED at ManualMapDll (MAPPING_ERR)\n");
            return MAPPING_ERR;
        }

        LOG("[Inject] OK — DLL mapped, returning SUCCESS\n");
        return SUCCESS;
    }
};
