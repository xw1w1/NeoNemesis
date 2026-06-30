#include <filesystem>
#include <d3d11.h>
#include <wincodec.h>

class Product {
public:
    Product(ID3D11Device* device,
        const char* proc_name,
        int steam_id,
        const char* title = nullptr,
        const std::filesystem::path& picture_big = {},
        const std::filesystem::path& picture_icon = {},
        const bool avail = false
    )
        : device_(device)
        , proc_name_(proc_name)
        , steam_id_(steam_id)
        , title_(title)
        , picture_big_(picture_big)
        , picture_icon_(picture_icon)
        , texture_big_(nullptr)
        , texture_icon_(nullptr)
        , load_attempted_big_(false)
        , load_attempted_icon_(false)
        , avail_(avail){
        if (!proc_name_ || *proc_name_ == '\0') {
            throw std::invalid_argument("proc_name must not be null or empty");
        }
        if (!device_) {
            throw std::invalid_argument("device must not be null");
        }

        int len = MultiByteToWideChar(CP_UTF8, 0, proc_name_, -1, nullptr, 0);
        proc_name_w_.resize(len - 1);
        MultiByteToWideChar(CP_UTF8, 0, proc_name_, -1, proc_name_w_.data(), len);
    }

    Product(const Product&) = delete;
    Product& operator=(const Product&) = delete;

    ~Product() {
        if (texture_big_) texture_big_->Release();
        if (texture_icon_) texture_icon_->Release();
    }

    const char* GetTitle() const noexcept {
        if (title_ && *title_ != '\0') {
            return title_;
        }
        return proc_name_;
    }

    ID3D11ShaderResourceView* GetProductPicture() {
        if (!load_attempted_big_ && !picture_big_.empty()) {
            LoadTextureFromFile(picture_big_.c_str(), &texture_big_);
            load_attempted_big_ = true;
        }
        return texture_big_;
    }

    ID3D11ShaderResourceView* GetProductIcon() {
        if (!load_attempted_icon_ && !picture_icon_.empty()) {
            LoadTextureFromFile(picture_icon_.c_str(), &texture_icon_);
            load_attempted_icon_ = true;
        }
        return texture_icon_;
    }

    const char* GetProcName() const noexcept { return proc_name_; }
    const wchar_t* GetProcNameW() const noexcept { return proc_name_w_.c_str(); }

    bool Available() const noexcept { return avail_;  }
    int GetSteamId() const noexcept { return steam_id_; }
    const std::filesystem::path& GetPictureBigPath() const noexcept { return picture_big_; }
    const std::filesystem::path& GetPictureIconPath() const noexcept { return picture_icon_; }

private:
    ID3D11Device* device_;
    const char* proc_name_;
    std::wstring proc_name_w_;
    const char* title_;
    int steam_id_;
    std::filesystem::path picture_big_;
    std::filesystem::path picture_icon_;
    bool avail_;

    ID3D11ShaderResourceView* texture_big_;
    ID3D11ShaderResourceView* texture_icon_;
    bool load_attempted_big_;
    bool load_attempted_icon_;

    bool LoadTextureFromFile(const wchar_t* filename, ID3D11ShaderResourceView** out_srv)
    {
        if (!out_srv || !device_)
            return false;

        *out_srv = nullptr;

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
        hr = device_->CreateTexture2D(&desc, &subResource, &texture);
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

        hr = device_->CreateShaderResourceView(texture, &srvDesc, out_srv);
        texture->Release();
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();

        if (FAILED(hr))
            return false;

        return true;
    }
};