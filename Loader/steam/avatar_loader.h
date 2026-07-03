#pragma once

#include <d3d11.h>
#include <cstdint>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>

class SteamAccount;

class AvatarLoader
{
public:
    using AccountResolver = std::function<SteamAccount* (uint64_t)>;

    static void Initialize(ID3D11Device* device, AccountResolver resolver);
    static void Shutdown();
    static void RequestAvatar(uint64_t steamID64);
    static void ProcessCompletedTasks();

private:
    struct AvatarTask
    {
        uint64_t steamID64 = 0;
        std::vector<uint8_t> pixel_data;
        int width = 0;
        int height = 0;
        bool success = false;
    };

    static ID3D11Device* s_device;
    static std::mutex s_mutex;
    static std::vector<AvatarTask*> s_completed_tasks;
    static std::atomic<bool> s_shutdown;

    static void LoadThread(uint64_t steamID64);

    static bool DownloadUrl(const std::wstring& url, std::vector<uint8_t>& out_data);
    static std::wstring ParseAvatarUrlFromXml(const std::string& xml);
    static bool DecodeImage(const std::vector<uint8_t>& file_data,
        std::vector<uint8_t>& out_pixels, int& out_w, int& out_h);
    static std::wstring GetAvatarCachePath(uint64_t steamID64);
    static bool LoadFromLocalCache(uint64_t steamID64, std::vector<uint8_t>& out_data);
    static bool LoadFromSteamCache(uint64_t steamID64, std::vector<uint8_t>& out_data);
    static void SaveToLocalCache(uint64_t steamID64, const std::vector<uint8_t>& data);
};