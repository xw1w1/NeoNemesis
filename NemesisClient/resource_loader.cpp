#include "resource_loader.h"
#include "Resources/embedded_resources.h"

#include <d3d11.h>
#include <windows.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

const unsigned char* GetResourceBytes(const char* name, unsigned int* out_size)
{
    Nemesis::Res::Blob blob = Nemesis::Res::Get(name);
    if (out_size) *out_size = blob.size;
    return blob.data;
}

bool LoadTextureFromMemory(const unsigned char* data, unsigned int size,
    ID3D11Device* device, ID3D11ShaderResourceView** out_srv)
{
    if (!out_srv || !device || !data || size == 0)
        return false;

    *out_srv = nullptr;

    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    IWICStream* stream = nullptr;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr) || !stream)
    {
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    hr = stream->InitializeFromMemory((BYTE*)data, size);
    if (FAILED(hr))
    {
        stream->Release();
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
    {
        stream->Release();
        factory->Release();
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
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

    BYTE* pixels = new BYTE[(size_t)width * height * 4];
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

    if (SUCCEEDED(hr_com))
        CoUninitialize();

    return SUCCEEDED(hr);
}

bool LoadTextureByName(const char* name, ID3D11Device* device,
    ID3D11ShaderResourceView** out_srv)
{
    unsigned int size = 0;
    const unsigned char* data = GetResourceBytes(name, &size);
    return LoadTextureFromMemory(data, size, device, out_srv);
}
