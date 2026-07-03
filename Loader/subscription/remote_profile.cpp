#include "remote_profile.h"
#include "../api/json.h"
#include "../api/loader_api.h"

RemoteProfile::RemoteProfile(const std::string& user_id)
    : user_id_(user_id)
{
}

RemoteProfile::~RemoteProfile()
{
}

void RemoteProfile::SyncFromCDN()
{
    LoadMetaAsync();
}

void RemoteProfile::LoadMetaAsync()
{
    std::string path = "subscriptions/" + user_id_ + ".json";

    NemesisAPI::DownloadAsync(path, [this](bool ok, std::vector<uint8_t> data) {
        if (!ok || data.empty()) return;

        std::string text(data.begin(), data.end());
        Json json;
        if (!Json::Parse(text, json)) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            username_ = json.GetString("username");
            last_checkout_ = json.GetInt("last_checkout", -1);
            sub_id_ = json.GetString("sub_id");
            products_ = json.GetArray("products");
        }

        meta_loaded_ = true;
        });
}