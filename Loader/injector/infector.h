#define _HAS_STD_BYTE 0
#define LOG(text) std::cout << text << std::endl;

#include "injector.h"
#include "hijacking.h"

#include <string>
#include <iostream>

#define HINFRES_VALUES \
X(EMPTY, "EMPTY") \
X(BAD_HEAP_ALLOC, "BAD_HEAP_ALLOC") \
X(NT_QUERYSYSINFO,"NT_QUERYSYSINFO") \
X(HANDLE_LEAK_DET,"HANDLE_LEAK_DET") \
X(PROC_NOT_FOUND,"PROC_NOT_FOUND") \
X(PROC_CANT_OPEN,"PROC_CANT_OPEN") \
X(PROC_INVALID_ARCH, "PROC_INVALID_ARCH") \
X(PROC_CANT_MAP, "PROC_CANT_MAP") \
X(DLL_NOT_FOUND, "DLL_NOT_FOUND") \
X(DLL_CANT_OPEN, "DLL_CANT_OPEN") \
X(DLL_CANT_ALLOC, "DLL_CANT_ALLOC") \
X(DLL_FILESIZE,"DLL_FILESIZE") \
X(MAPPING_ERR, "MAPPING_ERR") \
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

	// поскольку этот тип инжекта актуален только до версии Windows 11 24H2
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

	HANDLE HijackProcHandle(DWORD dwTargetProcessId)
	{
		HMODULE Ntdll = GetModuleHandleA("ntdll");

		_RtlAdjustPrivilege RtlAdjustPrivilege = (_RtlAdjustPrivilege)GetProcAddress(Ntdll, "RtlAdjustPrivilege");

		boolean OldPriv;

		RtlAdjustPrivilege(SeDebugPriv, TRUE, FALSE, &OldPriv);

		_NtQuerySystemInformation NtQuerySystemInformation = (_NtQuerySystemInformation)GetProcAddress(Ntdll, "NtQuerySystemInformation");

		_NtDuplicateObject NtDuplicateObject = (_NtDuplicateObject)GetProcAddress(Ntdll, "NtDuplicateObject");
		_NtOpenProcess NtOpenProcess = (_NtOpenProcess)GetProcAddress(Ntdll, "NtOpenProcess");
		OBJECT_ATTRIBUTES Obj_Attribute = InitObjectAttributes(NULL, NULL, NULL, NULL);

		CLIENT_ID clientID = { 0 };

		DWORD size = sizeof(SYSTEM_HANDLE_INFORMATION);

		hInfo = (SYSTEM_HANDLE_INFORMATION*) new byte[size];

		ZeroMemory(hInfo, size);

		NTSTATUS NtRet = NULL;

		do
		{
			delete[] hInfo;

			size *= 1.5;
			try
			{
				hInfo = (PSYSTEM_HANDLE_INFORMATION) new byte[size];
			}
			catch (std::bad_alloc)
			{
				LOG("Bad Heap Allocation");
				return nullptr;
			}
			Sleep(1);

		} while ((NtRet = NtQuerySystemInformation(SystemHandleInformation, hInfo, size, NULL)) == STATUS_INFO_LENGTH_MISMATCH);

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

			if (!IsHandleValid((HANDLE)hInfo->Handles[i].Handle) || hInfo->Handles[i].ObjectTypeNumber != ProcessHandleType)
				continue;

			clientID.UniqueProcess = (HANDLE)(ULONG_PTR)hInfo->Handles[i].ProcessId;
			clientID.UniqueThread = nullptr;

			procHandle ? CloseHandle(procHandle) : 0;

			NtRet = NtOpenProcess(&procHandle, PROCESS_DUP_HANDLE, &Obj_Attribute, &clientID);
			if (!IsHandleValid(procHandle) || !NT_SUCCESS(NtRet))
			{
				continue;
			}

			NtRet = NtDuplicateObject(procHandle, (HANDLE)hInfo->Handles[i].Handle, NtCurrentProcess, &HijackedHandle, PROCESS_ALL_ACCESS, 0, 0);
			if (!IsHandleValid(HijackedHandle) || !NT_SUCCESS(NtRet))
			{

				continue;
			}

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
		DWORD PID = GetProcId(proc);

		if (PID == 0)
		{
			return PROC_NOT_FOUND;
		}

		LOG("Found proc PID: %d\n", PID);

		HANDLE hProc = HijackProcHandle(PID);
		if (!hProc)
		{
			return PROC_CANT_OPEN;
		}

		if (!IsCorrectTargetArch(hProc))
		{
			return PROC_INVALID_ARCH;
		}

		if (GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES)
		{
			return DLL_NOT_FOUND;
		}

		std::ifstream fDLL(dll_path, std::ios::binary | std::ios::ate);

		if (fDLL.fail())
		{
			fDLL.close();
			CloseHandle(hProc);
			return DLL_CANT_OPEN;
		}

		auto file_size = fDLL.tellg();

		if (file_size < 0x1000) {
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
		fDLL.read((char*)(pSrcData), file_size);
		fDLL.close();

		LOG("Mapping...\n");
		if (!ManualMapDll(hProc, pSrcData, file_size))
		{
			delete[] pSrcData;
			CloseHandle(hProc);
			return MAPPING_ERR;
		}

		delete[] pSrcData;
		LOG("OK\n");

		return SUCCESS;
	}
};
