#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <d3d11.h>

enum ServerStatus
{
    SS_Ready = 0,
    SS_InMaintenance = 1,
    SS_ComingSoon = 2,
    SS_Offline = 3
};

class Product
{
public:
    Product(ID3D11Device* device, const std::string& product_hash);
    ~Product();

    Product(const Product&) = delete;
    Product& operator=(const Product&) = delete;

    // идентификатр
    const std::string& GetHash() const noexcept { return hash_; }

    // мета из products/<hash>.json
    const std::string& GetProcName()  const noexcept { return proc_name_; }
    const std::wstring& GetProcNameW() const noexcept { return proc_name_w_; }
    const std::string& GetTitle()     const noexcept { return title_; }
    int  GetSteamId()   const noexcept { return steam_id_; }
    int64_t GetLastUpdate() const noexcept { return last_update_; }
    ServerStatus GetStatus() const noexcept { return status_; }

    // состояния
    bool IsMetaLoaded() const noexcept { return meta_loaded_.load(); }
    bool IsAvailable() const noexcept { return available_; } // подписка есть
    void SetAvailable(bool v) noexcept { available_ = v; }

    // готов ли к запуску (статус OK + подписка есть)
    bool IsReady() const noexcept { return available_ && status_ == SS_Ready; }

    int  GetDefaultInjectTime() const noexcept { return def_inject_time_; }

    // асинхронно с CDN текстуры
    ID3D11ShaderResourceView* GetPoster() { return poster_.srv; }
    ID3D11ShaderResourceView* GetPosterDisabled() { return poster_disabled_.srv; }
    ID3D11ShaderResourceView* GetIcon() { return icon_.srv; }
    ID3D11ShaderResourceView* GetIconDisabled() { return icon_disabled_.srv; }
    ID3D11ShaderResourceView* GetHeader() { return header_.srv; }

    ID3D11ShaderResourceView* GetCurrentPoster() {
        if (IsReady()) return poster_.srv;
        return poster_disabled_.srv ? poster_disabled_.srv : poster_.srv;
    }
    ID3D11ShaderResourceView* GetCurrentIcon() {
        if (IsReady()) return icon_.srv;
        return icon_disabled_.srv ? icon_disabled_.srv : icon_.srv;
    }

    // синк с сервером
    // запросить мету + все изображения асинхронно с CDN
    void SyncFromCDN();

    // вызывать каждый кадр в главном потоке чтобы создат текстуры из загруженных пикселей
    void ProcessCompletedTasks();

private:
    struct TextureSlot
    {
        std::string hash;
        std::vector<uint8_t> pending_pixels;
        std::atomic<bool> pending{ false };
        int pending_w = 0, pending_h = 0;
        ID3D11ShaderResourceView* srv = nullptr;
    };

    void LoadMetaAsync();
    void LoadImageAsync(TextureSlot& slot);
    void CreateTextureFromPixels(TextureSlot& slot);

    ID3D11Device* device_;
    std::string   hash_;

    std::string  proc_name_;
    std::wstring proc_name_w_;
    std::string  title_;
    int          steam_id_ = 0;
    int64_t      last_update_ = 0;
    ServerStatus status_ = SS_Offline;
    int          def_inject_time_ = 30;
    std::atomic<bool> meta_loaded_{ false };

    bool available_ = false;

    TextureSlot poster_;
    TextureSlot poster_disabled_;
    TextureSlot icon_;
    TextureSlot icon_disabled_;
    TextureSlot header_;

    std::mutex mutex_;
};