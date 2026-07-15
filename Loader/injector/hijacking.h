#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>

#define SeDebugPriv 20
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// Не переопределяй STATUS_INFO_LENGTH_MISMATCH если уже есть
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#endif

#define NtCurrentProcess ( (HANDLE)(LONG_PTR) -1 )
#define ProcessHandleType 0x7
#define SystemHandleInformation 16

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
    ULONG       ProcessId;
    BYTE        ObjectTypeNumber;
    BYTE        Flags;
    USHORT      Handle;
    PVOID       Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, * PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
    ULONG        HandleCount;
    SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

typedef NTSTATUS(NTAPI* _NtDuplicateObject)(
    HANDLE      SourceProcessHandle,
    HANDLE      SourceHandle,
    HANDLE      TargetProcessHandle,
    PHANDLE     TargetHandle,
    ACCESS_MASK DesiredAccess,
    ULONG       Attributes,
    ULONG       Options
    );

typedef NTSTATUS(NTAPI* _RtlAdjustPrivilege)(
    ULONG    Privilege,
    BOOLEAN  Enable,
    BOOLEAN  CurrentThread,
    PBOOLEAN Enabled
    );

typedef NTSYSAPI NTSTATUS(NTAPI* _NtOpenProcess)(
    PHANDLE             ProcessHandle,
    ACCESS_MASK         DesiredAccess,
    POBJECT_ATTRIBUTES  ObjectAttributes,
    CLIENT_ID* ClientId
    );

typedef NTSTATUS(NTAPI* _NtQuerySystemInformation)(
    ULONG  SystemInformationClass,
    PVOID  SystemInformation,
    ULONG  SystemInformationLength,
    PULONG ReturnLength
    );