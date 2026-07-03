#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

class NemesisAPI
{
public:
    // базовая ссылка на апи, типа "https://raw.githubusercontent.com/xw1w1/Nemesis/main"
    static void Initialize(const std::string& base_url);

    // асинхронная загрузка файла в память
    // callback вызывается ИЗ ФОНОВОГО ПОТОКА чтобы не тормозить лоадер
    using DataCallback = std::function<void(bool success, std::vector<uint8_t> data)>;
    static void DownloadAsync(const std::string& relative_path, DataCallback callback, int timeout_seconds = 15);

    // синхронная загрузка (блокирует поток)
    static bool DownloadSync(const std::string& relative_path, std::vector<uint8_t>& out_data, int timeout_seconds = 15);

    // ручной выход из потоков при завершении программы
    static void Shutdown();

    // кеш хранится в <exe_dir>/Cache/CDN/<safe_filename>
    // Файлы кэшируются на диске, чтобы не качать повторно

    // скачивает файл, но сначала проверяет локальный кэш
    static void DownloadCachedAsync(
        const std::string& relative_path,
        DataCallback callback,
        int timeout_seconds = 15
    );

private:
    static std::string s_base_url;
    static void WorkerThread(std::string url, DataCallback callback, int timeout);

    static std::string GetCachePath(const std::string& relative_path);
    static bool LoadFromCache(const std::string& relative_path, std::vector<uint8_t>& out_data);
    static void SaveToCache(const std::string& relative_path, const std::vector<uint8_t>& data);
};