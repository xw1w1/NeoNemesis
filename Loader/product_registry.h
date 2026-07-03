#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <d3d11.h>

#include "subscription/product.h"
#include "subscription/remote_profile.h"

class ProductRegistry
{
public:
    static std::unique_ptr<RemoteProfile> ProductRegistry::s_profile;

    static void Initialize(ID3D11Device* device);
    static void Shutdown();

    static void Sync();

    static void LoadSubscription(const std::string& user_id = "default");

    static void ProcessCompletedTasks();

    static std::vector<Product*> GetAllProducts();
    static Product* GetByHash(const std::string& hash);
    static Product* GetByProcName(const std::string& proc_name);

    static bool IsSynced() { return s_synced.load(); }

private:
    static ID3D11Device* s_device;
    static std::vector<std::unique_ptr<Product>> s_products;
    static std::mutex s_mutex;
    static std::atomic<bool> s_synced;
};