#include "blur.h"
#include <unordered_map>
#include <string>
#include <cstring>

// ============================================================
// Shader source — separable Gaussian blur (horizontal + vertical)
// ============================================================

static const char* s_blurVS = R"(
struct VS_INPUT {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.uv  = input.uv;
    return output;
}
)";

static const char* s_blurPS = R"(
cbuffer BlurCB : register(b0) {
    float2 texelDir;   // (1/w, 0) for horizontal, (0, 1/h) for vertical
    float  radius;
    float  _pad;
};

Texture2D    srcTex : register(t0);
SamplerState srcSam : register(s0);

// 13-tap Gaussian kernel (sigma ~= radius * 0.3)
static const int KERNEL_SIZE = 13;
static const float offsets[KERNEL_SIZE] = {
    -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6
};
static const float weights[KERNEL_SIZE] = {
    0.002216, 0.008764, 0.026995, 0.064759, 0.120985,
    0.176033, 0.199471,
    0.176033, 0.120985, 0.064759, 0.026995, 0.008764, 0.002216
};

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float4 color = float4(0, 0, 0, 0);
    float2 step = texelDir * radius;

    for (int i = 0; i < KERNEL_SIZE; i++) {
        float2 sampleUV = uv + step * offsets[i];
        color += srcTex.Sample(srcSam, sampleUV) * weights[i];
    }

    return color;
}
)";

// ============================================================
// Fullscreen quad vertex data
// ============================================================

struct QuadVertex {
    float x, y, u, v;
};

static const QuadVertex s_quadVerts[4] = {
    { -1.0f,  1.0f, 0.0f, 0.0f },
    {  1.0f,  1.0f, 1.0f, 0.0f },
    { -1.0f, -1.0f, 0.0f, 1.0f },
    {  1.0f, -1.0f, 1.0f, 1.0f },
};

// ============================================================
// Internal state
// ============================================================

static ID3D11Device* s_device = nullptr;
static ID3D11DeviceContext* s_context = nullptr;

static ID3D11VertexShader* s_vs = nullptr;
static ID3D11PixelShader* s_ps = nullptr;
static ID3D11InputLayout* s_inputLayout = nullptr;
static ID3D11Buffer* s_vertexBuffer = nullptr;
static ID3D11Buffer* s_constantBuffer = nullptr;
static ID3D11SamplerState* s_sampler = nullptr;

// Backbuffer capture
static ID3D11Texture2D* s_bbCopy = nullptr;
static ID3D11ShaderResourceView* s_bbCopySrv = nullptr;
static UINT                     s_bbWidth = 0;
static UINT                     s_bbHeight = 0;

struct alignas(16) BlurCB {
    float texelDirX, texelDirY;
    float radius;
    float _pad;
};

// ============================================================
// Blur target pair (ping-pong)
// ============================================================

struct BlurTarget {
    ID3D11Texture2D* texA = nullptr;
    ID3D11RenderTargetView* rtvA = nullptr;
    ID3D11ShaderResourceView* srvA = nullptr;

    ID3D11Texture2D* texB = nullptr;
    ID3D11RenderTargetView* rtvB = nullptr;
    ID3D11ShaderResourceView* srvB = nullptr;

    UINT width = 0;
    UINT height = 0;

    void Release() {
        if (srvA) { srvA->Release(); srvA = nullptr; }
        if (rtvA) { rtvA->Release(); rtvA = nullptr; }
        if (texA) { texA->Release(); texA = nullptr; }
        if (srvB) { srvB->Release(); srvB = nullptr; }
        if (rtvB) { rtvB->Release(); rtvB = nullptr; }
        if (texB) { texB->Release(); texB = nullptr; }
        width = height = 0;
    }

    bool EnsureSize(ID3D11Device* dev, UINT w, UINT h) {
        if (width == w && height == h && texA && texB)
            return true;
        Release();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(dev->CreateTexture2D(&desc, nullptr, &texA))) return false;
        if (FAILED(dev->CreateRenderTargetView(texA, nullptr, &rtvA))) return false;
        if (FAILED(dev->CreateShaderResourceView(texA, nullptr, &srvA))) return false;

        if (FAILED(dev->CreateTexture2D(&desc, nullptr, &texB))) return false;
        if (FAILED(dev->CreateRenderTargetView(texB, nullptr, &rtvB))) return false;
        if (FAILED(dev->CreateShaderResourceView(texB, nullptr, &srvB))) return false;

        width = w;
        height = h;
        return true;
    }
};

// ============================================================
// Cache
// ============================================================

struct CacheEntry {
    BlurTarget target;
    ID3D11ShaderResourceView* resultSrv = nullptr;
    bool valid = false;
};

static std::unordered_map<std::string, CacheEntry> s_cache;
static CacheEntry s_bbBlurCache;

// ============================================================
// Helper: blit source SRV into target texture (stretch/crop)
// ============================================================

static void BlitToTarget(
    ID3D11ShaderResourceView* srcSrv,
    ID3D11RenderTargetView* dstRtv,
    UINT dstW, UINT dstH)
{
    // Simple copy via draw fullscreen quad with no blur
    // We use the blur shader with radius=0 which effectively just samples center
    // Actually, let's just draw the quad with a simple pass-through.
    // Easiest: set blur radius to 0 — the kernel center weight dominates.

    float clearColor[4] = { 0, 0, 0, 1 };
    s_context->ClearRenderTargetView(dstRtv, clearColor);

    D3D11_VIEWPORT vp = {};
    vp.Width = (float)dstW;
    vp.Height = (float)dstH;
    vp.MaxDepth = 1.0f;

    s_context->RSSetViewports(1, &vp);
    s_context->OMSetRenderTargets(1, &dstRtv, nullptr);

    // Set CB for no-blur (radius = 0 effectively copies center sample)
    BlurCB cb;
    cb.texelDirX = 0.0f;
    cb.texelDirY = 0.0f;
    cb.radius = 0.0f;
    cb._pad = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(s_context->Map(s_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &cb, sizeof(cb));
        s_context->Unmap(s_constantBuffer, 0);
    }

    UINT stride = sizeof(QuadVertex), offset = 0;
    s_context->IASetVertexBuffers(0, 1, &s_vertexBuffer, &stride, &offset);
    s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    s_context->IASetInputLayout(s_inputLayout);
    s_context->VSSetShader(s_vs, nullptr, 0);
    s_context->PSSetShader(s_ps, nullptr, 0);
    s_context->PSSetConstantBuffers(0, 1, &s_constantBuffer);
    s_context->PSSetShaderResources(0, 1, &srcSrv);
    s_context->PSSetSamplers(0, 1, &s_sampler);

    s_context->Draw(4, 0);

    // Unbind
    ID3D11ShaderResourceView* nullSrv = nullptr;
    s_context->PSSetShaderResources(0, 1, &nullSrv);
    ID3D11RenderTargetView* nullRtv = nullptr;
    s_context->OMSetRenderTargets(1, &nullRtv, nullptr);
}

// ============================================================
// Helper: run blur passes on a BlurTarget
// ============================================================

static void RunBlurPasses(
    BlurTarget& bt,
    int passes,
    float blurRadius)
{
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)bt.width;
    vp.Height = (float)bt.height;
    vp.MaxDepth = 1.0f;

    UINT stride = sizeof(QuadVertex), offset = 0;

    s_context->IASetVertexBuffers(0, 1, &s_vertexBuffer, &stride, &offset);
    s_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    s_context->IASetInputLayout(s_inputLayout);
    s_context->VSSetShader(s_vs, nullptr, 0);
    s_context->PSSetShader(s_ps, nullptr, 0);
    s_context->PSSetSamplers(0, 1, &s_sampler);
    s_context->RSSetViewports(1, &vp);

    for (int p = 0; p < passes; p++)
    {
        // Horizontal: A -> B
        {
            BlurCB cb;
            cb.texelDirX = 1.0f / (float)bt.width;
            cb.texelDirY = 0.0f;
            cb.radius = blurRadius;
            cb._pad = 0.0f;

            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(s_context->Map(s_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &cb, sizeof(cb));
                s_context->Unmap(s_constantBuffer, 0);
            }

            s_context->PSSetConstantBuffers(0, 1, &s_constantBuffer);
            s_context->OMSetRenderTargets(1, &bt.rtvB, nullptr);
            s_context->PSSetShaderResources(0, 1, &bt.srvA);
            s_context->Draw(4, 0);

            ID3D11ShaderResourceView* nullSrv = nullptr;
            s_context->PSSetShaderResources(0, 1, &nullSrv);
        }

        // Vertical: B -> A
        {
            BlurCB cb;
            cb.texelDirX = 0.0f;
            cb.texelDirY = 1.0f / (float)bt.height;
            cb.radius = blurRadius;
            cb._pad = 0.0f;

            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(s_context->Map(s_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &cb, sizeof(cb));
                s_context->Unmap(s_constantBuffer, 0);
            }

            s_context->PSSetConstantBuffers(0, 1, &s_constantBuffer);
            s_context->OMSetRenderTargets(1, &bt.rtvA, nullptr);
            s_context->PSSetShaderResources(0, 1, &bt.srvB);
            s_context->Draw(4, 0);

            ID3D11ShaderResourceView* nullSrv = nullptr;
            s_context->PSSetShaderResources(0, 1, &nullSrv);
        }
    }

    // Unbind
    ID3D11RenderTargetView* nullRtv = nullptr;
    s_context->OMSetRenderTargets(1, &nullRtv, nullptr);
}

// ============================================================
// Helper: save and restore D3D11 state
// ============================================================

struct D3D11SavedState {
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    UINT numViewports;
    ID3D11RenderTargetView* renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView* depthStencil;
    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
    ID3D11InputLayout* il;
    ID3D11Buffer* vb;
    UINT vbStride, vbOffset;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    ID3D11ShaderResourceView* psResources[1];
    ID3D11SamplerState* psSamplers[1];
    ID3D11Buffer* psCBuffers[1];
    ID3D11RasterizerState* rs;
    ID3D11BlendState* bs;
    float bsFactor[4];
    UINT bsMask;
    ID3D11DepthStencilState* dss;
    UINT dssRef;
};

static D3D11SavedState s_savedState;

static void SaveState()
{
    auto& st = s_savedState;
    st.numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    s_context->RSGetViewports(&st.numViewports, st.viewports);
    s_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, st.renderTargets, &st.depthStencil);
    s_context->VSGetShader(&st.vs, nullptr, nullptr);
    s_context->PSGetShader(&st.ps, nullptr, nullptr);
    s_context->IAGetInputLayout(&st.il);
    s_context->IAGetVertexBuffers(0, 1, &st.vb, &st.vbStride, &st.vbOffset);
    s_context->IAGetPrimitiveTopology(&st.topology);
    s_context->PSGetShaderResources(0, 1, st.psResources);
    s_context->PSGetSamplers(0, 1, st.psSamplers);
    s_context->PSGetConstantBuffers(0, 1, st.psCBuffers);
    s_context->RSGetState(&st.rs);
    s_context->OMGetBlendState(&st.bs, st.bsFactor, &st.bsMask);
    s_context->OMGetDepthStencilState(&st.dss, &st.dssRef);
}

static void RestoreState()
{
    auto& st = s_savedState;
    s_context->RSSetViewports(st.numViewports, st.viewports);
    s_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, st.renderTargets, st.depthStencil);
    s_context->VSSetShader(st.vs, nullptr, 0);
    s_context->PSSetShader(st.ps, nullptr, 0);
    s_context->IASetInputLayout(st.il);
    s_context->IASetVertexBuffers(0, 1, &st.vb, &st.vbStride, &st.vbOffset);
    s_context->IASetPrimitiveTopology(st.topology);
    s_context->PSSetShaderResources(0, 1, st.psResources);
    s_context->PSSetSamplers(0, 1, st.psSamplers);
    s_context->PSSetConstantBuffers(0, 1, st.psCBuffers);
    s_context->RSSetState(st.rs);
    s_context->OMSetBlendState(st.bs, st.bsFactor, st.bsMask);
    s_context->OMSetDepthStencilState(st.dss, st.dssRef);

    // Release refs from Get calls
    for (auto& rt : st.renderTargets) { if (rt) { rt->Release(); rt = nullptr; } }
    if (st.depthStencil) { st.depthStencil->Release(); st.depthStencil = nullptr; }
    if (st.vs) { st.vs->Release(); st.vs = nullptr; }
    if (st.ps) { st.ps->Release(); st.ps = nullptr; }
    if (st.il) { st.il->Release(); st.il = nullptr; }
    if (st.vb) { st.vb->Release(); st.vb = nullptr; }
    if (st.psResources[0]) { st.psResources[0]->Release(); st.psResources[0] = nullptr; }
    if (st.psSamplers[0]) { st.psSamplers[0]->Release(); st.psSamplers[0] = nullptr; }
    if (st.psCBuffers[0]) { st.psCBuffers[0]->Release(); st.psCBuffers[0] = nullptr; }
    if (st.rs) { st.rs->Release(); st.rs = nullptr; }
    if (st.bs) { st.bs->Release(); st.bs = nullptr; }
    if (st.dss) { st.dss->Release(); st.dss = nullptr; }
}

// ============================================================
// Public API
// ============================================================

bool NemesisBlur::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    s_device = device;
    s_context = context;

    // Compile vertex shader
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errBlob = nullptr;
    if (FAILED(D3DCompile(s_blurVS, strlen(s_blurVS), "BlurVS", nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errBlob)))
    {
        if (errBlob) errBlob->Release();
        return false;
    }

    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &s_vs)))
    {
        vsBlob->Release();
        return false;
    }

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,                 D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, sizeof(float) * 2, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (FAILED(device->CreateInputLayout(layoutDesc, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_inputLayout)))
    {
        vsBlob->Release();
        return false;
    }
    vsBlob->Release();

    // Compile pixel shader
    ID3DBlob* psBlob = nullptr;
    if (FAILED(D3DCompile(s_blurPS, strlen(s_blurPS), "BlurPS", nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errBlob)))
    {
        if (errBlob) errBlob->Release();
        return false;
    }

    if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &s_ps)))
    {
        psBlob->Release();
        return false;
    }
    psBlob->Release();

    // Vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(s_quadVerts);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = s_quadVerts;

    if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &s_vertexBuffer)))
        return false;

    // Constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(BlurCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &s_constantBuffer)))
        return false;

    // Sampler
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(device->CreateSamplerState(&sampDesc, &s_sampler)))
        return false;

    return true;
}

void NemesisBlur::Shutdown()
{
    for (auto& [key, entry] : s_cache)
        entry.target.Release();
    s_cache.clear();

    s_bbBlurCache.target.Release();

    if (s_bbCopySrv) { s_bbCopySrv->Release(); s_bbCopySrv = nullptr; }
    if (s_bbCopy) { s_bbCopy->Release(); s_bbCopy = nullptr; }

    if (s_sampler) { s_sampler->Release(); s_sampler = nullptr; }
    if (s_constantBuffer) { s_constantBuffer->Release(); s_constantBuffer = nullptr; }
    if (s_vertexBuffer) { s_vertexBuffer->Release(); s_vertexBuffer = nullptr; }
    if (s_inputLayout) { s_inputLayout->Release(); s_inputLayout = nullptr; }
    if (s_ps) { s_ps->Release(); s_ps = nullptr; }
    if (s_vs) { s_vs->Release(); s_vs = nullptr; }

    s_device = nullptr;
    s_context = nullptr;
}

void NemesisBlur::CaptureBackBuffer(IDXGISwapChain* swapChain)
{
    if (!swapChain || !s_device || !s_context)
        return;

    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        return;

    D3D11_TEXTURE2D_DESC desc = {};
    backBuffer->GetDesc(&desc);

    // Recreate copy texture if size changed
    if (!s_bbCopy || s_bbWidth != desc.Width || s_bbHeight != desc.Height)
    {
        if (s_bbCopySrv) { s_bbCopySrv->Release(); s_bbCopySrv = nullptr; }
        if (s_bbCopy) { s_bbCopy->Release(); s_bbCopy = nullptr; }

        D3D11_TEXTURE2D_DESC copyDesc = desc;
        copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        copyDesc.Usage = D3D11_USAGE_DEFAULT;
        copyDesc.CPUAccessFlags = 0;
        copyDesc.MiscFlags = 0;

        if (SUCCEEDED(s_device->CreateTexture2D(&copyDesc, nullptr, &s_bbCopy)))
        {
            s_device->CreateShaderResourceView(s_bbCopy, nullptr, &s_bbCopySrv);
            s_bbWidth = desc.Width;
            s_bbHeight = desc.Height;
        }
    }

    if (s_bbCopy)
        s_context->CopyResource(s_bbCopy, backBuffer);

    backBuffer->Release();

    // Mark backbuffer blur as dirty every frame
    s_bbBlurCache.valid = false;
}

ID3D11ShaderResourceView* NemesisBlur::GetBlurredTexture(
    const char* cacheKey,
    ID3D11ShaderResourceView* sourceSrv,
    UINT dstWidth,
    UINT dstHeight,
    int passes,
    float blurRadius,
    bool dirty)
{
    if (!s_device || !s_context || !sourceSrv)
        return nullptr;

    std::string key(cacheKey);
    auto& entry = s_cache[key];

    // Check if we need to rebuild
    bool needRebuild = dirty || !entry.valid;
    if (entry.target.width != dstWidth || entry.target.height != dstHeight)
        needRebuild = true;

    if (!needRebuild && entry.resultSrv)
        return entry.resultSrv;

    // Ensure targets
    if (!entry.target.EnsureSize(s_device, dstWidth, dstHeight))
        return nullptr;

    SaveState();

    // Blit source into target A
    BlitToTarget(sourceSrv, entry.target.rtvA, dstWidth, dstHeight);

    // Run blur passes
    if (passes > 0)
        RunBlurPasses(entry.target, passes, blurRadius);

    RestoreState();

    entry.resultSrv = entry.target.srvA;
    entry.valid = true;

    return entry.resultSrv;
}

ID3D11ShaderResourceView* NemesisBlur::GetBlurredBackBuffer(
    int passes,
    float blurRadius,
    bool dirty)
{
    if (!s_bbCopySrv)
        return nullptr;

    // Use half resolution for performance
    UINT dstW = s_bbWidth / 2;
    UINT dstH = s_bbHeight / 2;
    if (dstW == 0) dstW = 1;
    if (dstH == 0) dstH = 1;

    bool needRebuild = dirty || !s_bbBlurCache.valid;
    if (s_bbBlurCache.target.width != dstW || s_bbBlurCache.target.height != dstH)
        needRebuild = true;

    if (!needRebuild && s_bbBlurCache.resultSrv)
        return s_bbBlurCache.resultSrv;

    if (!s_bbBlurCache.target.EnsureSize(s_device, dstW, dstH))
        return nullptr;

    SaveState();

    BlitToTarget(s_bbCopySrv, s_bbBlurCache.target.rtvA, dstW, dstH);

    if (passes > 0)
        RunBlurPasses(s_bbBlurCache.target, passes, blurRadius);

    RestoreState();

    s_bbBlurCache.resultSrv = s_bbBlurCache.target.srvA;
    s_bbBlurCache.valid = true;

    return s_bbBlurCache.resultSrv;
}

void NemesisBlur::DrawBlurredBackBufferRect(
    ImDrawList* drawList,
    const ImVec2& min,
    const ImVec2& max,
    float rounding,
    ImDrawFlags flags,
    ImU32 tint,
    int passes,
    float blurRadius,
    bool dirty)
{
    if (!drawList)
        return;

    ID3D11ShaderResourceView* blurred = GetBlurredBackBuffer(passes, blurRadius, dirty);
    if (!blurred)
        return;

    ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x <= 0.0f || display.y <= 0.0f)
        return;

    ImVec2 uv0(min.x / display.x, min.y / display.y);
    ImVec2 uv1(max.x / display.x, max.y / display.y);

    drawList->AddImageRounded(
        (ImTextureID)blurred,
        min,
        max,
        uv0,
        uv1,
        tint,
        rounding,
        flags
    );
}

void NemesisBlur::InvalidateAll()
{
    for (auto& [key, entry] : s_cache)
        entry.valid = false;
    s_bbBlurCache.valid = false;
}