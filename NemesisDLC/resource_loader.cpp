#include "resource_loader.h"

#include <d3d11.h>
#include <windows.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

static HMODULE GetCurrentDllModule()
{
    HMODULE hModule = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetCurrentDllModule,
        &hModule
    );
    return hModule;
}

bool LoadTextureFromResource(int resource_id, ID3D11Device* device, ID3D11ShaderResourceView** out_srv)
{
    if (!out_srv || !device)
        return false;

    *out_srv = nullptr;

    HMODULE hModule = GetCurrentDllModule();
    HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(resource_id), (LPCWSTR)RT_RCDATA);
    if (!hResource) return false;

    HGLOBAL hMemory = LoadResource(hModule, hResource);
    if (!hMemory) return false;

    void* data = LockResource(hMemory);
    DWORD size = SizeofResource(hModule, hResource);
    if (!data || size == 0) return false;

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    IWICStream* stream = nullptr;
    factory->CreateStream(&stream);
    if (!stream) { factory->Release(); return false; }
    stream->InitializeFromMemory((BYTE*)data, size);

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { stream->Release(); factory->Release(); return false; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { decoder->Release(); stream->Release(); factory->Release(); return false; }

    IWICFormatConverter* converter = nullptr;
    factory->CreateFormatConverter(&converter);
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); stream->Release(); factory->Release(); return false; }

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);

    BYTE* pixels = new BYTE[width * height * 4];
    hr = converter->CopyPixels(nullptr, width * 4, width * height * 4, pixels);

    if (SUCCEEDED(hr))
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub = {};
        sub.pSysMem = pixels;
        sub.SysMemPitch = width * 4;

        ID3D11Texture2D* texture = nullptr;
        hr = device->CreateTexture2D(&desc, &sub, &texture);

        if (SUCCEEDED(hr))
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = desc.Format;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;

            hr = device->CreateShaderResourceView(texture, &srv_desc, out_srv);
            texture->Release();
        }
    }

    delete[] pixels;
    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();

    return SUCCEEDED(hr);
}

bool LoadResourceToMemory(int resource_id, std::vector<uint8_t>& out_data)
{
    HMODULE hModule = GetCurrentDllModule();
    HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(resource_id), (LPCWSTR)RT_RCDATA);
    if (!hResource) return false;

    HGLOBAL hMemory = LoadResource(hModule, hResource);
    if (!hMemory) return false;

    void* data = LockResource(hMemory);
    DWORD size = SizeofResource(hModule, hResource);
    if (!data || size == 0) return false;

    out_data.assign((uint8_t*)data, (uint8_t*)data + size);
    return true;
}