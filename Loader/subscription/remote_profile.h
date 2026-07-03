#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>

class RemoteProfile
{
public:
    RemoteProfile(const std::string& user_id);
    ~RemoteProfile();

    RemoteProfile(const RemoteProfile&) = delete;
    RemoteProfile& operator=(const RemoteProfile&) = delete;

    const std::string& GetUserId() const noexcept { return user_id_; }

    const std::string& GetUsername() const noexcept { return username_; }
    int64_t GetLastCheckout() const noexcept { return last_checkout_; }
    const std::string& GetSubId() const noexcept { return sub_id_; }
    const std::vector<std::string>& GetProducts() const noexcept { return products_; }

    bool IsMetaLoaded() const noexcept { return meta_loaded_.load(); }

    void SyncFromCDN();

private:
    void LoadMetaAsync();

    std::string user_id_;

    std::string username_;
    int64_t     last_checkout_ = -1;
    std::string sub_id_;
    std::vector<std::string> products_;

    std::atomic<bool> meta_loaded_{ false };
    std::mutex mutex_;
};