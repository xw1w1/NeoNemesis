#pragma once
#include <string>
#include <d3d11.h>

struct SystemInfo
{
    std::string hwid;              
    std::string hwid_full;         
    std::string cpu_name;          
    std::string cpu_cores;         
    std::string gpu_name;          
    std::string gpu_vram;          
    std::string ram_total;         
    std::string ram_available;     
    std::string windows_version;   
    std::string windows_build;     
    std::string os_arch;           
};

class SystemInfoCollector
{
public:
    static SystemInfo Collect(ID3D11Device* device = nullptr);

    static std::string GetHardwareID();
};