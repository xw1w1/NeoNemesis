#include "avatar_loader.h"
#include "steamacc.h"
#include "../file_utils.h"

#include <windows.h>
#include <winhttp.h>
#include <wincodec.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <regex>

#pragma comment(lib, "winhttp.lib")

ID3D11Device* AvatarLoader::s_device = nullptr;
std::mutex AvatarLoader::s_mutex;
std::vector<AvatarLoader::AvatarTask*> AvatarLoader::s_completed_tasks;
std::atomic<bool> AvatarLoader::s_shutdown{ false };

static AvatarLoader::AccountResolver s_resolver = nullptr;

void AvatarLoader::Initialize(ID3D11Device* device, AccountResolver resolver)
{
    s_device = device;
    s_resolver = resolver;
    s_shutdown = false;
}

void AvatarLoader::Shutdown()
{
    s_shutdown = true;

    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto* task : s_completed_tasks)
        delete task;
    s_completed_tasks.clear();
}

void AvatarLoader::RequestAvatar(uint64_t steamID64)
{
    SteamAccount* acc = s_resolver ? s_resolver(steamID64) : nullptr;
    if (!acc || acc->GetAvatarState() != SteamAccount::AvatarState::NotLoaded)
        return;

    acc->SetAvatarState(SteamAccount::AvatarState::Loading);
    std::thread(&AvatarLoader::LoadThread, steamID64).detach();
}

void AvatarLoader::ProcessCompletedTasks()
{
    std::lock_guard<std::mutex> lock(s_mutex);

    for (auto* task : s_completed_tasks)
    {
        if (!task) continue;
        SteamAccount* acc = s_resolver ? s_resolver(task->steamID64) : nullptr;

        if (task->success && acc && s_device)
        {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = task->width;
            desc.Height = task->height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA sub = {};
            sub.pSysMem = task->pixel_data.data();
            sub.SysMemPitch = task->width * 4;

            ID3D11Texture2D* texture = nullptr;
            if (SUCCEEDED(s_device->CreateTexture2D(&desc, &sub, &texture)))
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                srv_desc.Format = desc.Format;
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MipLevels = 1;

                ID3D11ShaderResourceView* srv = nullptr;
                if (SUCCEEDED(s_device->CreateShaderResourceView(texture, &srv_desc, &srv)))
                {
                    acc->SetAvatar(srv);
                    acc->SetAvatarState(SteamAccount::AvatarState::Loaded);
                }
                else
                {
                    acc->SetAvatarState(SteamAccount::AvatarState::Failed);
                }
                texture->Release();
            }
            else
            {
                acc->SetAvatarState(SteamAccount::AvatarState::Failed);
            }
        }
        else if (acc)
        {
            acc->SetAvatarState(SteamAccount::AvatarState::Failed);
        }

        delete task;
    }
    s_completed_tasks.clear();
}

void AvatarLoader::LoadThread(uint64_t steamID64)
{
    AvatarTask* task = new AvatarTask();
    SteamAccount* acc = s_resolver ? s_resolver(task->steamID64) : nullptr;
    task->steamID64 = steamID64;
    task->success = false;

    std::vector<uint8_t> image_data;

    if (LoadFromLocalCache(steamID64, image_data))
    {
        if (DecodeImage(image_data, task->pixel_data, task->width, task->height))
        {
            task->success = true;
        }
    }

    if (!task->success && !s_shutdown)
    {
        if (LoadFromSteamCache(steamID64, image_data))
        {
            if (DecodeImage(image_data, task->pixel_data, task->width, task->height))
            {
                task->success = true;
                SaveToLocalCache(steamID64, image_data);
            }
        }
    }

    if (!task->success && !s_shutdown)
    {
        std::wstring profile_url = L"https://steamcommunity.com/profiles/"
            + std::to_wstring(steamID64) + L"/?xml=1";

        std::vector<uint8_t> xml_data;
        if (DownloadUrl(profile_url, xml_data))
        {
            std::string xml(xml_data.begin(), xml_data.end());
            std::wstring avatar_url = ParseAvatarUrlFromXml(xml);

            if (!avatar_url.empty() && !s_shutdown)
            {
                if (DownloadUrl(avatar_url, image_data))
                {
                    if (DecodeImage(image_data, task->pixel_data, task->width, task->height))
                    {
                        task->success = true;
                        SaveToLocalCache(steamID64, image_data);
                    }
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    s_completed_tasks.push_back(task);
}

bool AvatarLoader::DownloadUrl(const std::wstring& url, std::vector<uint8_t>& out_data)
{
    out_data.clear();

    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostname[256] = { 0 };
    wchar_t urlpath[1024] = { 0 };
    urlComp.lpszHostName = hostname;
    urlComp.dwHostNameLength = _countof(hostname);
    urlComp.lpszUrlPath = urlpath;
    urlComp.dwUrlPathLength = _countof(urlpath);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp))
        return false;

    HINTERNET hSession = WinHttpOpen(L"Nemesis Loader/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    DWORD secure_protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
        | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
        | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
        &secure_protocols, sizeof(secure_protocols));

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
        DWORD size = 0;
        do {
            size = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &size) || size == 0)
                break;

            size_t old_size = out_data.size();
            out_data.resize(old_size + size);

            DWORD read = 0;
            if (!WinHttpReadData(hRequest, out_data.data() + old_size, size, &read))
            {
                out_data.resize(old_size);
                ok = false;
                break;
            }
            out_data.resize(old_size + read);
        } while (size > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return ok && !out_data.empty();
}

std::wstring AvatarLoader::ParseAvatarUrlFromXml(const std::string& xml)
{
    const char* tags[] = { "avatarFull", "avatarMedium", "avatarIcon" };

    for (const char* tag : tags)
    {
        std::string open_tag = "<" + std::string(tag) + ">";
        std::string close_tag = "</" + std::string(tag) + ">";

        size_t start = xml.find(open_tag);
        if (start == std::string::npos) continue;

        start += open_tag.length();
        size_t end = xml.find(close_tag, start);
        if (end == std::string::npos) continue;

        std::string content = xml.substr(start, end - start);

        const std::string cdata_start = "<![CDATA[";
        const std::string cdata_end = "]]>";
        size_t cs = content.find(cdata_start);
        if (cs != std::string::npos)
        {
            cs += cdata_start.length();
            size_t ce = content.find(cdata_end, cs);
            if (ce != std::string::npos)
                content = content.substr(cs, ce - cs);
        }

        auto trim = [](std::string& s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\n' || s.front() == '\r' || s.front() == '\t')) s.erase(0, 1);
            while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r' || s.back() == '\t')) s.pop_back();
            };
        trim(content);

        if (content.empty()) continue;

        std::wstring result(content.begin(), content.end());
        return result;
    }

    return L"";
}

bool AvatarLoader::DecodeImage(const std::vector<uint8_t>& file_data,
    std::vector<uint8_t>& out_pixels,
    int& out_w, int& out_h)
{
    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool need_uninit = SUCCEEDED(hr_com);

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { if (need_uninit) CoUninitialize(); return false; }

    IWICStream* stream = nullptr;
    factory->CreateStream(&stream);
    if (!stream) { factory->Release(); if (need_uninit) CoUninitialize(); return false; }

    stream->InitializeFromMemory(const_cast<BYTE*>(file_data.data()), (DWORD)file_data.size());

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
    {
        stream->Release(); factory->Release();
        if (need_uninit) CoUninitialize();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);
    if (!frame)
    {
        decoder->Release(); stream->Release(); factory->Release();
        if (need_uninit) CoUninitialize();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    converter->GetSize(&w, &h);

    out_pixels.resize(w * h * 4);
    hr = converter->CopyPixels(nullptr, w * 4, (UINT)out_pixels.size(), out_pixels.data());

    out_w = (int)w;
    out_h = (int)h;

    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();
    if (need_uninit) CoUninitialize();

    return SUCCEEDED(hr);
}

std::wstring AvatarLoader::GetAvatarCachePath(uint64_t steamID64)
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::wstring path = exe_path;
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path = path.substr(0, pos + 1);

    path += L"Cache\\Avatars\\";

    SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);

    path += std::to_wstring(steamID64) + L".jpg";
    return path;
}

bool AvatarLoader::LoadFromLocalCache(uint64_t steamID64, std::vector<uint8_t>& out_data)
{
    std::wstring path = GetAvatarCachePath(steamID64);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto size = file.tellg();
    if (size <= 0) return false;

    file.seekg(0, std::ios::beg);
    out_data.resize((size_t)size);
    file.read((char*)out_data.data(), size);

    return file.good() && !out_data.empty();
}

bool AvatarLoader::LoadFromSteamCache(uint64_t steamID64, std::vector<uint8_t>& out_data)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    char steam_path[MAX_PATH] = { 0 };
    DWORD size = sizeof(steam_path);
    DWORD type;
    LONG res = RegQueryValueExA(hKey, "SteamPath", nullptr, &type, (LPBYTE)steam_path, &size);
    RegCloseKey(hKey);

    if (res != ERROR_SUCCESS) return false;

    std::string cache_path = steam_path;
    std::replace(cache_path.begin(), cache_path.end(), '/', '\\');
    cache_path += "\\config\\avatarcache\\" + std::to_string(steamID64) + ".png";

    std::ifstream file(cache_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto file_size = file.tellg();
    if (file_size <= 0) return false;

    file.seekg(0, std::ios::beg);
    out_data.resize((size_t)file_size);
    file.read((char*)out_data.data(), file_size);

    return file.good() && !out_data.empty();
}

void AvatarLoader::SaveToLocalCache(uint64_t steamID64, const std::vector<uint8_t>& data)
{
    std::wstring path = GetAvatarCachePath(steamID64);
    std::ofstream file(path, std::ios::binary);
    if (file.is_open())
        file.write((const char*)data.data(), data.size());
}