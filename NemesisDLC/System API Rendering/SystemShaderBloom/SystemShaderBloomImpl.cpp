#include "SystemShaderBloom.hpp"
#include "BloomShaders.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <d3dcompiler.h>
#include <directxmath.h>
#include <algorithm>
#include <cmath>
#include <string_view>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Nemesis::SystemShaderBloom
{
struct BloomCB
{
float threshold;
float knee;
float padding2;
};

struct BloomIntensityCB
{
DirectX::XMFLOAT2 texelSize;
float radius;
float padding0;
};

struct ColorCB
{
DirectX::XMFLOAT4 colorTint;
float intensity;
float padding0;
float padding1;
float padding2;
};

static float ClampDownscale(float downscale)
{
return std::clamp(downscale, 0.125f, 1.0f);
}

static float ClampFinite(float value, float minValue, float maxValue, float fallback)
{
return std::isfinite(value) ? std::clamp(value, minValue, maxValue) : fallback;
}

BloomEffect::BloomEffect() = default;

BloomEffect::~BloomEffect()
{
Release();
}

bool BloomEffect::Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
 uint32_t width, uint32_t height)
{
if (!device || !context)
{
NWARN("BloomEffect::Initialize - invalid device or context");
return false;
}

if (width == 0 || height == 0)
{
NWARN("BloomEffect::Initialize - invalid size %ux%u", width, height);
return false;
}

m_device = device;
m_context = context;
m_width = width;
m_height = height;

if (!CompileShaders() || !CreateTextures(m_defaultParams.downscale) || !CreateSamplers())
{
NWARN("BloomEffect::Initialize - initialization failed");
Release();
return false;
}

m_initialized = true;
NLOG("BloomEffect initialized (%ux%u)", width, height);
return true;
}

void BloomEffect::Release()
{
if (m_psThreshold) { m_psThreshold->Release(); m_psThreshold = nullptr; }
if (m_psBlurH) { m_psBlurH->Release(); m_psBlurH = nullptr; }
if (m_psBlurV) { m_psBlurV->Release(); m_psBlurV = nullptr; }
if (m_psCompose) { m_psCompose->Release(); m_psCompose = nullptr; }
if (m_vsFullscreen) { m_vsFullscreen->Release(); m_vsFullscreen = nullptr; }
if (m_inputLayout) { m_inputLayout->Release(); m_inputLayout = nullptr; }
if (m_cbBloom) { m_cbBloom->Release(); m_cbBloom = nullptr; }
if (m_cbIntensity) { m_cbIntensity->Release(); m_cbIntensity = nullptr; }
if (m_cbColor) { m_cbColor->Release(); m_cbColor = nullptr; }
ReleaseTextures();
if (m_samplerLinear) { m_samplerLinear->Release(); m_samplerLinear = nullptr; }
m_initialized = false;
}

void BloomEffect::ReleaseTextures()
{
if (m_rtvBright) { m_rtvBright->Release(); m_rtvBright = nullptr; }
if (m_srvBright) { m_srvBright->Release(); m_srvBright = nullptr; }
if (m_texBright) { m_texBright->Release(); m_texBright = nullptr; }
if (m_rtvBlurH) { m_rtvBlurH->Release(); m_rtvBlurH = nullptr; }
if (m_srvBlurH) { m_srvBlurH->Release(); m_srvBlurH = nullptr; }
if (m_texBlurH) { m_texBlurH->Release(); m_texBlurH = nullptr; }
if (m_rtvBlurV) { m_rtvBlurV->Release(); m_rtvBlurV = nullptr; }
if (m_srvBlurV) { m_srvBlurV->Release(); m_srvBlurV = nullptr; }
if (m_texBlurV) { m_texBlurV->Release(); m_texBlurV = nullptr; }
if (m_rtvFinal) { m_rtvFinal->Release(); m_rtvFinal = nullptr; }
if (m_srvFinal) { m_srvFinal->Release(); m_srvFinal = nullptr; }
if (m_texFinal) { m_texFinal->Release(); m_texFinal = nullptr; }
m_bloomWidth = 0;
m_bloomHeight = 0;
m_textureDownscale = 0.0f;
}

struct D3DStateBackup
{
ID3D11DeviceContext* context = nullptr;
D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
UINT viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
ID3D11DepthStencilView* dsv = nullptr;
ID3D11VertexShader* vs = nullptr;
ID3D11PixelShader* ps = nullptr;
ID3D11InputLayout* inputLayout = nullptr;
D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
ID3D11ShaderResourceView* srvs[2] = {};
ID3D11SamplerState* sampler = nullptr;
ID3D11Buffer* psCB = nullptr;

explicit D3DStateBackup(ID3D11DeviceContext* ctx) : context(ctx)
{
context->RSGetViewports(&viewportCount, viewports);
context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);
context->VSGetShader(&vs, nullptr, nullptr);
context->PSGetShader(&ps, nullptr, nullptr);
context->IAGetInputLayout(&inputLayout);
context->IAGetPrimitiveTopology(&topology);
context->PSGetShaderResources(0, 2, srvs);
context->PSGetSamplers(0, 1, &sampler);
context->PSGetConstantBuffers(0, 1, &psCB);
}

~D3DStateBackup()
{
context->RSSetViewports(viewportCount, viewports);
context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
context->VSSetShader(vs, nullptr, 0);
context->PSSetShader(ps, nullptr, 0);
context->IASetInputLayout(inputLayout);
context->IASetPrimitiveTopology(topology);
context->PSSetShaderResources(0, 2, srvs);
context->PSSetSamplers(0, 1, &sampler);
context->PSSetConstantBuffers(0, 1, &psCB);
for (ID3D11RenderTargetView* rtv : rtvs)
{
if (rtv) rtv->Release();
}
if (dsv) dsv->Release();
if (vs) vs->Release();
if (ps) ps->Release();
if (inputLayout) inputLayout->Release();
for (ID3D11ShaderResourceView* srv : srvs)
{
if (srv) srv->Release();
}
if (sampler) sampler->Release();
if (psCB) psCB->Release();
}
};

bool BloomEffect::Render(ID3D11ShaderResourceView* sourceTexture,
 const BloomColor& color)
{
return Render(sourceTexture, color, m_defaultParams);
}

bool BloomEffect::Render(ID3D11ShaderResourceView* sourceTexture,
 const BloomColor& color,
 const BloomParams& params)
{
if (!m_initialized || !sourceTexture)
return false;

const float threshold = ClampFinite(params.threshold, 0.0f, 32.0f, m_defaultParams.threshold);
const float radius = ClampFinite(params.blurRadius, 0.25f, 16.0f, m_defaultParams.blurRadius);
const float intensity = ClampFinite(params.intensity, 0.0f, 16.0f, m_defaultParams.intensity);
const float downscale = ClampDownscale(params.downscale);

if (!EnsureTextures(downscale))
return false;

D3DStateBackup backup(m_context);

D3D11_VIEWPORT vpBloom = {};
vpBloom.Width = static_cast<float>(m_bloomWidth);
vpBloom.Height = static_cast<float>(m_bloomHeight);
vpBloom.MinDepth = 0.0f;
vpBloom.MaxDepth = 1.0f;
m_context->RSSetViewports(1, &vpBloom);
m_context->IASetInputLayout(nullptr);
m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

if (!ApplyThreshold(sourceTexture, threshold, m_rtvBright) ||
!ApplyBlurH(m_srvBright, radius, m_rtvBlurH) ||
!ApplyBlurV(m_srvBlurH, radius, m_rtvBlurV))
return false;

D3D11_VIEWPORT vpFull = {};
vpFull.Width = static_cast<float>(m_width);
vpFull.Height = static_cast<float>(m_height);
vpFull.MinDepth = 0.0f;
vpFull.MaxDepth = 1.0f;
m_context->RSSetViewports(1, &vpFull);

return ApplyCompose(sourceTexture, m_srvBlurV, color, intensity, m_rtvFinal);
}

ID3D11ShaderResourceView* BloomEffect::GetOutputTexture() const
{
return m_srvFinal;
}

void BloomEffect::SetDefaultParams(const BloomParams& params)
{
m_defaultParams = params;
}

bool BloomEffect::CompileShaders()
{
ID3DBlob* blob = nullptr;
ID3DBlob* errorBlob = nullptr;

auto CreatePS = [&](const std::string_view source, const char* name, ID3D11PixelShader** ps) -> bool
{
if (FAILED(D3DCompile(source.data(), source.size(), name, nullptr, nullptr, "main", "ps_4_0", 0, 0, &blob, &errorBlob)))
{
if (errorBlob)
{
NWARN("%s compile error: %s", name, static_cast<const char*>(errorBlob->GetBufferPointer()));
errorBlob->Release();
errorBlob = nullptr;
}
return false;
}
if (FAILED(m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ps)))
{
NWARN("CreatePixelShader failed for %s", name);
blob->Release();
return false;
}
blob->Release();
blob = nullptr;
return true;
};

if (FAILED(D3DCompile(Shaders::FullscreenVS.data(), Shaders::FullscreenVS.size(), "FullscreenVS", nullptr, nullptr, "main", "vs_4_0", 0, 0, &blob, &errorBlob)))
{
if (errorBlob)
{
NWARN("VS compile error: %s", static_cast<const char*>(errorBlob->GetBufferPointer()));
errorBlob->Release();
}
return false;
}
if (FAILED(m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_vsFullscreen)))
{
NWARN("CreateVertexShader failed");
blob->Release();
return false;
}
blob->Release();
blob = nullptr;

return CreatePS(Shaders::ThresholdPS, "ThresholdPS", &m_psThreshold) &&
   CreatePS(Shaders::BlurHPS, "BlurHPS", &m_psBlurH) &&
   CreatePS(Shaders::BlurVPS, "BlurVPS", &m_psBlurV) &&
   CreatePS(Shaders::ComposePS, "ComposePS", &m_psCompose);
}

bool BloomEffect::CreateTextures(float downscale)
{
ReleaseTextures();

D3D11_TEXTURE2D_DESC texDesc = {};
m_textureDownscale = ClampDownscale(downscale);
m_bloomWidth = std::max<uint32_t>(1, static_cast<uint32_t>(std::round(m_width * m_textureDownscale)));
m_bloomHeight = std::max<uint32_t>(1, static_cast<uint32_t>(std::round(m_height * m_textureDownscale)));

texDesc.Width = m_bloomWidth;
texDesc.Height = m_bloomHeight;
texDesc.MipLevels = 1;
texDesc.ArraySize = 1;
texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
texDesc.SampleDesc.Count = 1;
texDesc.Usage = D3D11_USAGE_DEFAULT;
texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

auto CreateTextureResources = [&](ID3D11Texture2D** tex, ID3D11RenderTargetView** rtv, ID3D11ShaderResourceView** srv) -> bool
{
if (FAILED(m_device->CreateTexture2D(&texDesc, nullptr, tex)) ||
FAILED(m_device->CreateRenderTargetView(*tex, nullptr, rtv)) ||
FAILED(m_device->CreateShaderResourceView(*tex, nullptr, srv)))
{
NWARN("Texture resource creation failed");
if (*srv) { (*srv)->Release(); *srv = nullptr; }
if (*rtv) { (*rtv)->Release(); *rtv = nullptr; }
if (*tex) { (*tex)->Release(); *tex = nullptr; }
return false;
}
return true;
};

if (!CreateTextureResources(&m_texBright, &m_rtvBright, &m_srvBright) ||
!CreateTextureResources(&m_texBlurH, &m_rtvBlurH, &m_srvBlurH) ||
!CreateTextureResources(&m_texBlurV, &m_rtvBlurV, &m_srvBlurV))
{
ReleaseTextures();
return false;
}

texDesc.Width = m_width;
texDesc.Height = m_height;
if (!CreateTextureResources(&m_texFinal, &m_rtvFinal, &m_srvFinal))
{
ReleaseTextures();
return false;
}

D3D11_BUFFER_DESC cbDesc = {};
cbDesc.Usage = D3D11_USAGE_DYNAMIC;
cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

cbDesc.ByteWidth = sizeof(BloomCB);
if (!m_cbBloom && FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbBloom)))
return false;

cbDesc.ByteWidth = sizeof(BloomIntensityCB);
if (!m_cbIntensity && FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbIntensity)))
return false;

cbDesc.ByteWidth = sizeof(ColorCB);
if (!m_cbColor && FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbColor)))
return false;

return true;
}

bool BloomEffect::EnsureTextures(float downscale)
{
const float wantedDownscale = ClampDownscale(downscale);
const uint32_t wantedWidth = std::max<uint32_t>(1, static_cast<uint32_t>(std::round(m_width * wantedDownscale)));
const uint32_t wantedHeight = std::max<uint32_t>(1, static_cast<uint32_t>(std::round(m_height * wantedDownscale)));
if (m_texBright && m_texFinal && m_bloomWidth == wantedWidth && m_bloomHeight == wantedHeight)
return true;
return CreateTextures(wantedDownscale);
}

bool BloomEffect::CreateSamplers()
{
D3D11_SAMPLER_DESC sampDesc = {};
sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
sampDesc.MaxAnisotropy = 1;
sampDesc.MinLOD = 0;
sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

return SUCCEEDED(m_device->CreateSamplerState(&sampDesc, &m_samplerLinear));
}

bool BloomEffect::ApplyThreshold(ID3D11ShaderResourceView* source, float threshold, ID3D11RenderTargetView* target)
{
m_context->OMSetRenderTargets(1, &target, nullptr);
float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
m_context->ClearRenderTargetView(target, clearColor);

m_context->VSSetShader(m_vsFullscreen, nullptr, 0);
m_context->PSSetShader(m_psThreshold, nullptr, 0);

D3D11_MAPPED_SUBRESOURCE mapped;
if (SUCCEEDED(m_context->Map(m_cbBloom, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
{
BloomCB* cb = static_cast<BloomCB*>(mapped.pData);
cb->threshold = threshold;
cb->knee = std::max(0.01f, threshold * 0.35f);
m_context->Unmap(m_cbBloom, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbBloom);
m_context->PSSetShaderResources(0, 1, &source);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(3, 0);

ID3D11ShaderResourceView* nullSRV = nullptr;
m_context->PSSetShaderResources(0, 1, &nullSRV);
return true;
}

bool BloomEffect::ApplyBlurH(ID3D11ShaderResourceView* source, float radius, ID3D11RenderTargetView* target)
{
m_context->OMSetRenderTargets(1, &target, nullptr);
float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
m_context->ClearRenderTargetView(target, clearColor);

m_context->VSSetShader(m_vsFullscreen, nullptr, 0);
m_context->PSSetShader(m_psBlurH, nullptr, 0);

D3D11_MAPPED_SUBRESOURCE mapped;
if (SUCCEEDED(m_context->Map(m_cbIntensity, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
{
BloomIntensityCB* cb = static_cast<BloomIntensityCB*>(mapped.pData);
cb->texelSize = DirectX::XMFLOAT2(1.0f / static_cast<float>(m_bloomWidth), 1.0f / static_cast<float>(m_bloomHeight));
cb->radius = radius;
m_context->Unmap(m_cbIntensity, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbIntensity);
m_context->PSSetShaderResources(0, 1, &source);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(3, 0);

ID3D11ShaderResourceView* nullSRV = nullptr;
m_context->PSSetShaderResources(0, 1, &nullSRV);
return true;
}

bool BloomEffect::ApplyBlurV(ID3D11ShaderResourceView* source, float radius, ID3D11RenderTargetView* target)
{
m_context->OMSetRenderTargets(1, &target, nullptr);
float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
m_context->ClearRenderTargetView(target, clearColor);

m_context->VSSetShader(m_vsFullscreen, nullptr, 0);
m_context->PSSetShader(m_psBlurV, nullptr, 0);

D3D11_MAPPED_SUBRESOURCE mapped;
if (SUCCEEDED(m_context->Map(m_cbIntensity, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
{
BloomIntensityCB* cb = static_cast<BloomIntensityCB*>(mapped.pData);
cb->texelSize = DirectX::XMFLOAT2(1.0f / static_cast<float>(m_bloomWidth), 1.0f / static_cast<float>(m_bloomHeight));
cb->radius = radius;
m_context->Unmap(m_cbIntensity, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbIntensity);
m_context->PSSetShaderResources(0, 1, &source);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(3, 0);

ID3D11ShaderResourceView* nullSRV = nullptr;
m_context->PSSetShaderResources(0, 1, &nullSRV);
return true;
}

bool BloomEffect::ApplyCompose(ID3D11ShaderResourceView* source, ID3D11ShaderResourceView* blurred, const BloomColor& color, float intensity, ID3D11RenderTargetView* target)
{
m_context->OMSetRenderTargets(1, &target, nullptr);
float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
m_context->ClearRenderTargetView(target, clearColor);

m_context->VSSetShader(m_vsFullscreen, nullptr, 0);
m_context->PSSetShader(m_psCompose, nullptr, 0);

D3D11_MAPPED_SUBRESOURCE mapped;
if (SUCCEEDED(m_context->Map(m_cbColor, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
{
ColorCB* cb = static_cast<ColorCB*>(mapped.pData);
cb->colorTint = color.ToXMFLOAT4();
cb->intensity = intensity;
m_context->Unmap(m_cbColor, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbColor);
ID3D11ShaderResourceView* srvs[2] = { source, blurred };
m_context->PSSetShaderResources(0, 2, srvs);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(3, 0);

ID3D11ShaderResourceView* nullSRVs[2] = {};
m_context->PSSetShaderResources(0, 2, nullSRVs);
return true;
}

std::unique_ptr<BloomEffect> CreateBloomEffect(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height)
{
auto effect = std::make_unique<BloomEffect>();
return effect->Initialize(device, context, width, height) ? std::move(effect) : nullptr;
}
}
