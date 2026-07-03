#include <d3d11.h>

#include "api/json.h"
#include "api/loader_api.h"
#include "subscription/remote_profile.h"
#include "product_registry.h"

ID3D11Device* ProductRegistry::s_device = nullptr;
std::vector<std::unique_ptr<Product>> ProductRegistry::s_products;
std::mutex ProductRegistry::s_mutex;
std::atomic<bool> ProductRegistry::s_synced{ false };

std::unique_ptr<RemoteProfile> ProductRegistry::s_profile;

void ProductRegistry::Initialize(ID3D11Device* device)
{
    s_device = device;
    s_synced = false;
}

void ProductRegistry::Shutdown()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_products.clear();
}

void ProductRegistry::Sync()
{
    NemesisAPI::DownloadCachedAsync("indexes/products.json", [](bool ok, std::vector<uint8_t> data) {
        if (!ok || data.empty()) return;

        std::string text(data.begin(), data.end());
        std::vector<Json> products;
        if (!Json::ParseArray(text, products)) return;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_products.clear();

            for (auto& prod_json : products)
            {
                std::string hash = prod_json.GetString("productId");
                if (hash.empty()) continue;

                auto product = std::make_unique<Product>(s_device, hash);
                product->SyncFromCDN();
                s_products.push_back(std::move(product));
            }
        }

        s_synced = true;

        LoadSubscription("default");
        });
}

void ProductRegistry::LoadSubscription(const std::string& user_id)
{
    s_profile = std::make_unique<RemoteProfile>(user_id);
    s_profile->SyncFromCDN();

    std::string path = "subscriptions/" + user_id + ".json";

    NemesisAPI::DownloadCachedAsync(path, [](bool ok, std::vector<uint8_t> data) {
        if (!ok || data.empty()) return;

        std::string text(data.begin(), data.end());
        Json sub;
        if (!Json::Parse(text, sub)) return;

        auto subscribed_products = sub.GetArray("products");

        std::lock_guard<std::mutex> lock(s_mutex);
        for (auto& product : s_products)
        {
            bool has = std::find(subscribed_products.begin(), subscribed_products.end(),
                product->GetHash()) != subscribed_products.end();
            product->SetAvailable(has);
        }
        });
}

void ProductRegistry::ProcessCompletedTasks()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto& product : s_products)
        product->ProcessCompletedTasks();
}

std::vector<Product*> ProductRegistry::GetAllProducts()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    std::vector<Product*> result;
    result.reserve(s_products.size());
    for (auto& p : s_products) result.push_back(p.get());
    return result;
}

Product* ProductRegistry::GetByHash(const std::string& hash)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto& p : s_products) if (p->GetHash() == hash) return p.get();
    return nullptr;
}

Product* ProductRegistry::GetByProcName(const std::string& proc_name)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto& p : s_products) if (p->GetProcName() == proc_name) return p.get();
    return nullptr;
}