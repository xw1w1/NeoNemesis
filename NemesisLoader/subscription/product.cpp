#include "product.h"
#include "../api/json.h"
#include "../api/loader_api.h"

#include <windows.h>
#include <wincodec.h>

static bool DecodeImageWIC(const std::vector<uint8_t>&file_data,
    std::vector<uint8_t>&out_pixels, int& out_w, int& out_h)
{
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool need_uninit = SUCCEEDED(com);

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
    if (FAILED(hr)) { stream->Release(); factory->Release(); if (need_uninit) CoUninitialize(); return false; }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);
    if (!frame) { decoder->Release(); stream->Release(); factory->Release(); if (need_uninit) CoUninitialize(); return false; }

    IWICFormatConverter* converter = nullptr;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    converter->GetSize(&w, &h);

    out_pixels.resize(w * h * 4);
    hr = converter->CopyPixels(nullptr, w * 4, (UINT)out_pixels.size(), out_pixels.data());
    out_w = (int)w; out_h = (int)h;

    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();
    if (need_uninit) CoUninitialize();

    return SUCCEEDED(hr);
}

Product::Product(ID3D11Device* device, const std::string& product_hash)
    : device_(device)
    , hash_(product_hash)
{
}

Product::~Product()
{
    if (poster_.srv)          poster_.srv->Release();
    if (poster_disabled_.srv) poster_disabled_.srv->Release();
    if (icon_.srv)            icon_.srv->Release();
    if (icon_disabled_.srv)   icon_disabled_.srv->Release();
}

void Product::SyncFromCDN()
{
    LoadMetaAsync();
}

void Product::LoadMetaAsync()
{
    std::string path = "products/" + hash_ + ".json";

    NemesisAPI::DownloadCachedAsync(path, [this](bool ok, std::vector<uint8_t> data) {
        if (!ok || data.empty()) return;

        std::string text(data.begin(), data.end());
        Json json;
        if (!Json::Parse(text, json)) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            proc_name_ = json.GetString("proc");
            title_ = json.GetString("name");
            steam_id_ = (int)json.GetInt("steam_id");
            last_update_ = json.GetInt("last_update");
            status_ = (ServerStatus)json.GetInt("status", SS_Offline);
            def_inject_time_ = (int)json.GetInt("inject_time", 30);

            poster_.hash = json.GetString("poster");
            poster_disabled_.hash = json.GetString("poster_disabled");
            icon_.hash = json.GetString("icon");
            icon_disabled_.hash = json.GetString("icon_disabled");

            int len = MultiByteToWideChar(CP_UTF8, 0, proc_name_.c_str(), -1, nullptr, 0);
            if (len > 0) {
                proc_name_w_.resize(len - 1);
                MultiByteToWideChar(CP_UTF8, 0, proc_name_.c_str(), -1, proc_name_w_.data(), len);
            }
        }

        meta_loaded_ = true;

        if (!poster_.hash.empty())          LoadImageAsync(poster_);
        if (!poster_disabled_.hash.empty()) LoadImageAsync(poster_disabled_);
        if (!icon_.hash.empty())            LoadImageAsync(icon_);
        if (!icon_disabled_.hash.empty())   LoadImageAsync(icon_disabled_);
        });
}

void Product::LoadImageAsync(TextureSlot& slot)
{
    std::string path = "static/" + slot.hash + ".jpg";
    TextureSlot* slot_ptr = &slot;

    NemesisAPI::DownloadCachedAsync(path, [this, slot_ptr](bool ok, std::vector<uint8_t> data) {
        if (!ok || data.empty()) return;

        std::vector<uint8_t> pixels;
        int w, h;
        if (!DecodeImageWIC(data, pixels, w, h)) return;

        std::lock_guard<std::mutex> lock(mutex_);
        slot_ptr->pending_pixels = std::move(pixels);
        slot_ptr->pending_w = w;
        slot_ptr->pending_h = h;
        slot_ptr->pending = true;
        });
}

void Product::CreateTextureFromPixels(TextureSlot& slot)
{
    if (!slot.pending.load()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!slot.pending.load()) return;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = slot.pending_w;
    desc.Height = slot.pending_h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub = {};
    sub.pSysMem = slot.pending_pixels.data();
    sub.SysMemPitch = slot.pending_w * 4;

    ID3D11Texture2D* tex = nullptr;
    if (SUCCEEDED(device_->CreateTexture2D(&desc, &sub, &tex)))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;

        if (slot.srv) { slot.srv->Release(); slot.srv = nullptr; }
        device_->CreateShaderResourceView(tex, &srv_desc, &slot.srv);
        tex->Release();
    }

    slot.pending_pixels.clear();
    slot.pending = false;
}

void Product::ProcessCompletedTasks()
{
    CreateTextureFromPixels(poster_);
    CreateTextureFromPixels(poster_disabled_);
    CreateTextureFromPixels(icon_);
    CreateTextureFromPixels(icon_disabled_);
}