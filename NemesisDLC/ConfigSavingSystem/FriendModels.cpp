#include "FriendModels.hpp"
#include "../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace Nemesis::FriendModels
{
    using namespace Nemesis::Addresses;

    namespace
    {
        struct Friend
        {
            std::uint64_t steamID;
            std::string   model;
            std::string   token;
        };

        std::vector<Friend> g_friends;

        bool HeapPtr(std::uintptr_t v) { return v >= 0x10000 && v <= 0x7FFFFFFFFFFFull; }

        std::wstring ConfigDir()
        {
            std::wstring dir = L"D:\\Nemesis\\NemesisConfigs";
            CreateDirectoryW(L"D:\\Nemesis", nullptr);
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir;
        }

        std::string ExtractString(const std::string& text, const char* key)
        {
            const std::string k = std::string("\"") + key + "\"";
            size_t p = text.find(k);
            if (p == std::string::npos)
                return "";
            p = text.find(':', p);
            if (p == std::string::npos)
                return "";
            size_t q = text.find('"', p);
            if (q == std::string::npos)
                return "";
            size_t r = text.find('"', q + 1);
            if (r == std::string::npos)
                return "";
            return text.substr(q + 1, r - q - 1);
        }

        std::uint64_t ExtractU64(const std::string& text, const char* key)
        {
            const std::string k = std::string("\"") + key + "\"";
            size_t p = text.find(k);
            if (p == std::string::npos)
                return 0;
            p = text.find(':', p);
            if (p == std::string::npos)
                return 0;
            return std::strtoull(text.c_str() + p + 1, nullptr, 10);
        }
    }

    const char* MatchSteamID(std::uint64_t steamID, const char*& token)
    {
        if (g_friends.empty() || !steamID)
            return nullptr;
        for (const Friend& f : g_friends)
        {
            if (f.steamID == steamID)
            {
                token = f.token.c_str();
                return f.model.c_str();
            }
        }
        return nullptr;
    }

    bool SaveMyConfig(std::uintptr_t base)
    {
        const std::uintptr_t controller = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerController);
        if (!HeapPtr(controller))
        {
            NERR("[friend] SaveMyConfig: no local controller (join a server first)");
            return false;
        }
        const std::uint64_t id = Mem::Read<std::uint64_t>(controller + Schema::m_steamID, 0);
        if (!id)
        {
            NERR("[friend] SaveMyConfig: steamID unavailable");
            return false;
        }

        char fileName[64];
        std::snprintf(fileName, sizeof(fileName), "\\cstatus_%llu.json", static_cast<unsigned long long>(id));
        const std::wstring path = ConfigDir() + std::wstring(fileName, fileName + std::strlen(fileName));

        std::ostringstream json;
        json << "{\n"
             << "  \"steamID\": " << id << ",\n"
             << "  \"model\": \"" << Addresses::CustomModel::kModelPath << "\",\n"
             << "  \"token\": \"" << Addresses::CustomModel::kModelToken << "\"\n"
             << "}\n";
        const std::string body = json.str();

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            NERR("[friend] SaveMyConfig: cannot open file for write");
            return false;
        }
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
        out.close();

        NLOG("[friend] config saved: D:\\Nemesis\\NemesisConfigs\\cstatus_%llu.json (steamID=%llu model=%s)",
             static_cast<unsigned long long>(id), static_cast<unsigned long long>(id),
             Addresses::CustomModel::kModelToken);
        return true;
    }

    bool LoadConfig(const char* path)
    {
        if (!path || !*path)
            return false;

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            NERR("[friend] LoadConfig: cannot open '%s'", path);
            return false;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string text = ss.str();

        Friend f;
        f.steamID = ExtractU64(text, "steamID");
        f.model = ExtractString(text, "model");
        f.token = ExtractString(text, "token");
        if (!f.steamID || f.model.empty())
        {
            NERR("[friend] LoadConfig: bad config (steamID/model missing)");
            return false;
        }
        if (f.token.empty())
            f.token = f.model;

        for (Friend& existing : g_friends)
        {
            if (existing.steamID == f.steamID)
            {
                existing = f;
                NLOG("[friend] config updated: steamID=%llu model=%s",
                     static_cast<unsigned long long>(f.steamID), f.token.c_str());
                return true;
            }
        }
        g_friends.push_back(f);
        NLOG("[friend] config loaded: steamID=%llu model=%s (total friends=%d)",
             static_cast<unsigned long long>(f.steamID), f.token.c_str(), static_cast<int>(g_friends.size()));
        return true;
    }

    void Clear()
    {
        g_friends.clear();
    }

    int LoadedCount()
    {
        return static_cast<int>(g_friends.size());
    }
}
