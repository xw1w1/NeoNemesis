#include "file_utils.h"

#include <windows.h>
#include <wincodec.h>
#include <fstream>
#include <sstream>

bool LoadTextureFromFile(const wchar_t* filename, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    if (!out_srv || !device)
        return false;

    *out_srv = nullptr;
    if (out_width)  *out_width = 0;
    if (out_height) *out_height = 0;

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return false;

    hr = factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
    {
        factory->Release();
        return false;
    }

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
        decoder->Release();
        factory->Release();
        return false;
    }

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
    {
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    converter->GetSize(&width, &height);

    BYTE* pixels = new BYTE[width * height * 4];
    hr = converter->CopyPixels(nullptr, width * 4, width * height * 4, pixels);
    if (FAILED(hr))
    {
        delete[] pixels;
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = pixels;
    subResource.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    hr = device->CreateTexture2D(&desc, &subResource, &texture);
    delete[] pixels;

    if (FAILED(hr))
    {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture, &srvDesc, out_srv);
    texture->Release();
    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();

    if (FAILED(hr))
        return false;

    if (out_width)  *out_width = static_cast<int>(width);
    if (out_height) *out_height = static_cast<int>(height);

    return true;
}

void ReleaseTexture(ID3D11ShaderResourceView*& texture)
{
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
}

bool ReadFileToString(const std::string& path, std::string& out)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    std::stringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

bool FileExists(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirectoryExists(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::string> ListDirectories(const std::string& path)
{
    std::vector<std::string> result;
    std::string search = path + "\\*";

    WIN32_FIND_DATAA find_data;
    HANDLE h = FindFirstFileA(search.c_str(), &find_data);
    if (h == INVALID_HANDLE_VALUE) return result;

    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            std::string name = find_data.cFileName;
            if (name != "." && name != "..")
                result.push_back(name);
        }
    } while (FindNextFileA(h, &find_data));

    FindClose(h);
    return result;
}

std::string CombinePath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '\\' || last == '/') return a + b;
    return a + "\\" + b;
}

std::filesystem::path GetExecutableDirectory()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}