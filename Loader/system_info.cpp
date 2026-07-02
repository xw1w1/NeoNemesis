#include "system_info.h"
#include <windows.h>
#include <intrin.h>
#include <dxgi.h>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "dxgi.lib")

static uint64_t FnvHash64(const std::string& data)
{
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : data)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static std::string GetVolumeSerialNumber(const wchar_t* drive)
{
    DWORD serial = 0;
    if (!GetVolumeInformationW(drive, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0))
        return "";

    char buf[32];
    sprintf_s(buf, "%08X", serial);
    return buf;
}

static std::string GetCpuID()
{
    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 1);

    char buf[64];
    sprintf_s(buf, "%08X%08X", cpuInfo[3], cpuInfo[0]);
    return buf;
}

static std::string GetMachineGuid()
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography",
        0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return "";

    char buf[128] = { 0 };
    DWORD size = sizeof(buf);
    DWORD type;
    LONG res = RegQueryValueExA(hKey, "MachineGuid", nullptr, &type, (LPBYTE)buf, &size);
    RegCloseKey(hKey);

    return (res == ERROR_SUCCESS) ? std::string(buf) : "";
}

std::string SystemInfoCollector::GetHardwareID()
{
    std::string components;
    components += GetVolumeSerialNumber(L"C:\\");
    components += "|";
    components += GetCpuID();
    components += "|";
    components += GetMachineGuid();

    uint64_t hash = FnvHash64(components);

    char buf[32];
    sprintf_s(buf, "%04X-%04X-%04X-%04X",
        (unsigned)((hash >> 48) & 0xFFFF),
        (unsigned)((hash >> 32) & 0xFFFF),
        (unsigned)((hash >> 16) & 0xFFFF),
        (unsigned)(hash & 0xFFFF));

    return buf;
}

static std::string GetCpuName()
{
    int cpuInfo[4] = { 0 };
    char name[64] = { 0 };

    __cpuid(cpuInfo, 0x80000000);
    unsigned max_ext = cpuInfo[0];

    if (max_ext >= 0x80000004)
    {
        __cpuid((int*)&name[0], 0x80000002);
        __cpuid((int*)&name[16], 0x80000003);
        __cpuid((int*)&name[32], 0x80000004);
    }

    std::string result(name);
    while (!result.empty() && result.back() == ' ') result.pop_back();
    while (!result.empty() && result.front() == ' ') result.erase(0, 1);

    return result.empty() ? "Unknown CPU" : result;
}

static std::string GetCpuCoresInfo()
{
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    DWORD logical = si.dwNumberOfProcessors;
    DWORD physical = 0;
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    if (len > 0)
    {
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(info.data(), &len))
        {
            for (const auto& item : info)
                if (item.Relationship == RelationProcessorCore)
                    physical++;
        }
    }

    char buf[64];
    if (physical > 0 && physical != logical)
        sprintf_s(buf, "%lu cores / %lu threads", physical, logical);
    else
        sprintf_s(buf, "%lu cores", logical);
    return buf;
}

static std::string FormatBytes(uint64_t bytes)
{
    const double GB = 1024.0 * 1024.0 * 1024.0;
    double gb = (double)bytes / GB;

    char buf[32];
    if (gb >= 100.0)
        sprintf_s(buf, "%.0f GB", gb);
    else
        sprintf_s(buf, "%.1f GB", gb);
    return buf;
}

static void GetRamInfo(std::string& total, std::string& available)
{
    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (GlobalMemoryStatusEx(&mem))
    {
        total = FormatBytes(mem.ullTotalPhys);
        available = FormatBytes(mem.ullAvailPhys) + " available";
    }
    else
    {
        total = "Unknown";
        available = "Unknown";
    }
}

static void GetGpuInfo(ID3D11Device* device, std::string& gpu_name, std::string& vram)
{
    gpu_name = "Unknown GPU";
    vram = "Unknown";

    if (device)
    {
        IDXGIDevice* dxgi_device = nullptr;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
        {
            IDXGIAdapter* adapter = nullptr;
            if (SUCCEEDED(dxgi_device->GetAdapter(&adapter)))
            {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc)))
                {
                    char buf[128] = { 0 };
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, buf, sizeof(buf), nullptr, nullptr);
                    gpu_name = buf;

                    if (desc.DedicatedVideoMemory > 0)
                        vram = FormatBytes(desc.DedicatedVideoMemory) + " VRAM";
                }
                adapter->Release();
            }
            dxgi_device->Release();
        }
        if (gpu_name != "Unknown GPU") return;
    }

    IDXGIFactory* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory))))
    {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters(0, &adapter)))
        {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc)))
            {
                char buf[128] = { 0 };
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, buf, sizeof(buf), nullptr, nullptr);
                gpu_name = buf;

                if (desc.DedicatedVideoMemory > 0)
                    vram = FormatBytes(desc.DedicatedVideoMemory) + " VRAM";
            }
            adapter->Release();
        }
        factory->Release();
    }
}

typedef LONG(WINAPI* RtlGetVersion_t)(PRTL_OSVERSIONINFOW);

static void GetWindowsInfo(std::string& version, std::string& build, std::string& arch)
{
    version = "Windows";
    build = "Unknown";
    arch = "Unknown";

    RTL_OSVERSIONINFOW info = { sizeof(info) };
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll)
    {
        auto func = (RtlGetVersion_t)GetProcAddress(ntdll, "RtlGetVersion");
        if (func && func(&info) == 0)
        {
            if (info.dwMajorVersion == 10 && info.dwBuildNumber >= 22000)
                version = "Windows 11";
            else if (info.dwMajorVersion == 10)
                version = "Windows 10";
            else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3)
                version = "Windows 8.1";
            else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 2)
                version = "Windows 8";
            else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 1)
                version = "Windows 7";

            HKEY hKey;
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                char edition[64] = { 0 };
                DWORD sz = sizeof(edition);
                if (RegQueryValueExA(hKey, "EditionID", nullptr, nullptr,
                    (LPBYTE)edition, &sz) == ERROR_SUCCESS)
                {
                    version += " ";
                    version += edition;
                }

                DWORD ubr = 0;
                sz = sizeof(ubr);
                if (RegQueryValueExA(hKey, "UBR", nullptr, nullptr,
                    (LPBYTE)&ubr, &sz) == ERROR_SUCCESS)
                {
                    char buf[32];
                    sprintf_s(buf, "%lu.%lu", info.dwBuildNumber, ubr);
                    build = buf;
                }
                else
                {
                    char buf[32];
                    sprintf_s(buf, "%lu", info.dwBuildNumber);
                    build = buf;
                }

                RegCloseKey(hKey);
            }
        }
    }

    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64"; break;
    case PROCESSOR_ARCHITECTURE_ARM64: arch = "ARM64"; break;
    case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
    default: arch = "Unknown"; break;
    }
}

SystemInfo SystemInfoCollector::Collect(ID3D11Device* device)
{
    SystemInfo info;

    info.hwid_full = GetHardwareID();

    // обфусцированный хвид чтобы показывать только 1, 4 indices
    // "4A72-F183-B591-C0E4" → "4A72-****-****-C0E4"
    if (info.hwid_full.length() >= 19)
    {
        info.hwid = info.hwid_full.substr(0, 4) + "-****-****-" + info.hwid_full.substr(15, 4);
    }
    else
    {
        info.hwid = info.hwid_full;
    }

    info.cpu_name = GetCpuName();
    info.cpu_cores = GetCpuCoresInfo();

    GetRamInfo(info.ram_total, info.ram_available);

    GetGpuInfo(device, info.gpu_name, info.gpu_vram);

    GetWindowsInfo(info.windows_version, info.windows_build, info.os_arch);

    return info;
}