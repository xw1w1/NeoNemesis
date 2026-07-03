#include "loader_api.h"
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <thread>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

std::string NemesisAPI::s_base_url;

void NemesisAPI::Initialize(const std::string& base_url)
{
    s_base_url = base_url;
    while (!s_base_url.empty() && s_base_url.back() == '/')
        s_base_url.pop_back();
}

void NemesisAPI::Shutdown() {}

void NemesisAPI::DownloadAsync(const std::string& relative_path, DataCallback callback, int timeout)
{
    std::string full_url = s_base_url + "/" + relative_path;
    std::thread(&NemesisAPI::WorkerThread, full_url, callback, timeout).detach();
}

void NemesisAPI::DownloadCachedAsync(const std::string& relative_path, DataCallback callback, int timeout)
{
    std::thread([relative_path, callback, timeout]() {
        std::vector<uint8_t> data;

        if (LoadFromCache(relative_path, data))
        {
            callback(true, std::move(data));
            return;
        }

        std::string full_url = s_base_url + "/" + relative_path;
        if (DownloadSync(relative_path, data, timeout))
        {
            SaveToCache(relative_path, data);
            callback(true, std::move(data));
        }
        else
        {
            callback(false, {});
        }
        }).detach();
}

bool NemesisAPI::DownloadSync(const std::string& relative_path, std::vector<uint8_t>& out_data, int timeout)
{
    std::string full_url = s_base_url + "/" + relative_path;

    std::wstring wurl(full_url.begin(), full_url.end());

    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostname[256] = { 0 };
    wchar_t urlpath[2048] = { 0 };
    urlComp.lpszHostName = hostname;
    urlComp.dwHostNameLength = _countof(hostname);
    urlComp.lpszUrlPath = urlpath;
    urlComp.dwUrlPathLength = _countof(urlpath);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp))
        return false;

    HINTERNET hSession = WinHttpOpen(L"Nemesis Loader/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    DWORD tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof(tls));

    DWORD timeout_ms = timeout * 1000;
    WinHttpSetTimeouts(hSession, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

    HINTERNET hConnect = WinHttpConnect(hSession, hostname, urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlpath,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr);

    if (ok)
    {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);

        if (status != 200) ok = false;
    }

    if (ok)
    {
        DWORD size = 0;
        do {
            size = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &size) || size == 0) break;

            size_t old = out_data.size();
            out_data.resize(old + size);

            DWORD read = 0;
            if (!WinHttpReadData(hRequest, out_data.data() + old, size, &read))
            {
                out_data.resize(old);
                ok = false;
                break;
            }
            out_data.resize(old + read);
        } while (size > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return ok && !out_data.empty();
}

void NemesisAPI::WorkerThread(std::string url, DataCallback callback, int timeout)
{
    std::vector<uint8_t> data;
    std::string temp_base = s_base_url;
    if (url.length() > temp_base.length() && url.compare(0, temp_base.length(), temp_base) == 0)
    {
        std::string rel = url.substr(temp_base.length() + 1);
        bool ok = DownloadSync(rel, data, timeout);
        callback(ok, std::move(data));
    }
    else
    {
        callback(false, {});
    }
}

std::string NemesisAPI::GetCachePath(const std::string& relative_path)
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::wstring path = exe_path;
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path = path.substr(0, pos + 1);

    path += L"Cache\\CDN\\";
    SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);

    std::string safe_name = relative_path;
    std::replace(safe_name.begin(), safe_name.end(), '/', '_');
    std::replace(safe_name.begin(), safe_name.end(), '\\', '_');

    std::wstring wname(safe_name.begin(), safe_name.end());
    return std::string(path.begin(), path.end()) + std::string(safe_name);
}

bool NemesisAPI::LoadFromCache(const std::string& relative_path, std::vector<uint8_t>& out_data)
{
    std::string cache_path = GetCachePath(relative_path);
    std::ifstream file(cache_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto size = file.tellg();
    if (size <= 0) return false;

    file.seekg(0, std::ios::beg);
    out_data.resize((size_t)size);
    file.read((char*)out_data.data(), size);

    return file.good() && !out_data.empty();
}

void NemesisAPI::SaveToCache(const std::string& relative_path, const std::vector<uint8_t>& data)
{
    std::string cache_path = GetCachePath(relative_path);
    std::ofstream file(cache_path, std::ios::binary);
    if (file.is_open())
        file.write((const char*)data.data(), data.size());
}