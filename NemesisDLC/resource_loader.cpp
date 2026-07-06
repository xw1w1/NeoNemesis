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
    if (!hModule)
    {
        OutputDebugStringA("[ResourceLoader] ERROR: hModule is null\n");
        return false;
    }

    // Выводим имя модуля для диагностики
    char module_name[MAX_PATH] = {};
    GetModuleFileNameA(hModule, module_name, MAX_PATH);
    char dbg[512];
    snprintf(dbg, sizeof(dbg), "[ResourceLoader] Module: %s, ResourceID: %d\n", module_name, resource_id);
    OutputDebugStringA(dbg);

    HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(resource_id), (LPCWSTR)RT_RCDATA);
    if (!hResource)
    {
        snprintf(dbg, sizeof(dbg), "[ResourceLoader] ERROR: FindResource failed for ID=%d, LastError=%lu\n",
            resource_id, GetLastError());
        OutputDebugStringA(dbg);
        return false;
    }

    HGLOBAL hMemory = LoadResource(hModule, hResource);
    if (!hMemory)
    {
        OutputDebugStringA("[ResourceLoader] ERROR: LoadResource failed\n");
        return false;
    }

    void* data = LockResource(hMemory);
    DWORD size = SizeofResource(hModule, hResource);
    if (!data || size == 0)
    {
        snprintf(dbg, sizeof(dbg), "[ResourceLoader] ERROR: data=%p size=%lu\n", data, size);
        OutputDebugStringA(dbg);
        return false;
    }

    snprintf(dbg, sizeof(dbg), "[ResourceLoader] Resource loaded OK: ID=%d, size=%lu\n", resource_id, size);
    OutputDebugStringA(dbg);

    // COM
    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        snprintf(dbg, sizeof(dbg), "[ResourceLoader] ERROR: CoCreateInstance failed, hr=0x%08X\n", hr);
        OutputDebugStringA(dbg);
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    IWICStream* stream = nullptr;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr) || !stream)
    {
        OutputDebugStringA("[ResourceLoader] ERROR: CreateStream failed\n");
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    hr = stream->InitializeFromMemory((BYTE*)data, size);
    if (FAILED(hr))
    {
        OutputDebugStringA("[ResourceLoader] ERROR: InitializeFromMemory failed\n");
        stream->Release();
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
    {
        snprintf(dbg, sizeof(dbg), "[ResourceLoader] ERROR: CreateDecoderFromStream failed, hr=0x%08X\n", hr);
        OutputDebugStringA(dbg);
        stream->Release();
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
        OutputDebugStringA("[ResourceLoader] ERROR: GetFrame failed\n");
        decoder->Release();
        stream->Release();
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter)
    {
        OutputDebugStringA("[ResourceLoader] ERROR: CreateFormatConverter failed\n");
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        OutputDebugStringA("[ResourceLoader] ERROR: Converter Initialize failed\n");
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);

    snprintf(dbg, sizeof(dbg), "[ResourceLoader] Image size: %ux%u\n", width, height);
    OutputDebugStringA(dbg);

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

            if (SUCCEEDED(hr))
                OutputDebugStringA("[ResourceLoader] Texture created successfully!\n");
            else
            {
                snprintf(dbg, sizeof(dbg), "[ResourceLoader] ERROR: CreateShaderResourceView failed, hr=0x%08X\n", hr);
                OutputDebugStringA(dbg);
            }
        }
        else
        {
            snprintf(dbg, sizeof(dbg), "[ResourceLoader] ERROR: CreateTexture2D failed, hr=0x%08X\n", hr);
            OutputDebugStringA(dbg);
        }
    }

    delete[] pixels;
    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();

    if (SUCCEEDED(hr_com))
        CoUninitialize();

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