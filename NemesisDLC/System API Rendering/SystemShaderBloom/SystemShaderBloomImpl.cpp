#include "SystemShaderBloom.hpp"
#include "BloomShaders.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <d3dcompiler.h>
#include <directxmath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Nemesis::SystemShaderBloom
{
struct BloomCB
{
float threshold;
float padding0;
float padding1;
float padding2;
};

struct BloomIntensityCB
{
float intensity;
float padding0;
float padding1;
float padding2;
};

struct ColorCB
{
DirectX::XMFLOAT4 colorTint;
};

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

m_device = device;
m_context = context;
m_width = width;
m_height = height;

if (!CompileShaders() || !CreateTextures() || !CreateSamplers())
{
NWARN("BloomEffect::Initialize - initialization failed");
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
if (m_texBright) { m_texBright->Release(); m_texBright = nullptr; }
if (m_srvBright) { m_srvBright->Release(); m_srvBright = nullptr; }
if (m_rtvBright) { m_rtvBright->Release(); m_rtvBright = nullptr; }
if (m_texBlurH) { m_texBlurH->Release(); m_texBlurH = nullptr; }
if (m_srvBlurH) { m_srvBlurH->Release(); m_srvBlurH = nullptr; }
if (m_rtvBlurH) { m_rtvBlurH->Release(); m_rtvBlurH = nullptr; }
if (m_texBlurV) { m_texBlurV->Release(); m_texBlurV = nullptr; }
if (m_srvBlurV) { m_srvBlurV->Release(); m_srvBlurV = nullptr; }
if (m_rtvBlurV) { m_rtvBlurV->Release(); m_rtvBlurV = nullptr; }
if (m_texFinal) { m_texFinal->Release(); m_texFinal = nullptr; }
if (m_srvFinal) { m_srvFinal->Release(); m_srvFinal = nullptr; }
if (m_rtvFinal) { m_rtvFinal->Release(); m_rtvFinal = nullptr; }
if (m_samplerLinear) { m_samplerLinear->Release(); m_samplerLinear = nullptr; }
m_initialized = false;
}

bool BloomEffect::Render(ID3D11ShaderResourceView* sourceTexture,
 const BloomColor& color,
 const BloomParams& params)
{
if (!m_initialized || !sourceTexture)
return false;

D3D11_VIEWPORT oldViewport;
uint32_t vpCount = 1;
m_context->RSGetViewports(&vpCount, &oldViewport);

D3D11_VIEWPORT vpBright;
vpBright.TopLeftX = 0.0f;
vpBright.TopLeftY = 0.0f;
vpBright.Width = static_cast<float>(m_width * params.downscale);
vpBright.Height = static_cast<float>(m_height * params.downscale);
vpBright.MinDepth = 0.0f;
vpBright.MaxDepth = 1.0f;
m_context->RSSetViewports(1, &vpBright);

if (!ApplyThreshold(sourceTexture, params.threshold, m_rtvBright) ||
!ApplyBlurH(m_srvBright, params.intensity, m_rtvBlurH) ||
!ApplyBlurV(m_srvBlurH, params.intensity, m_rtvBlurV))
{
m_context->RSSetViewports(1, &oldViewport);
return false;
}

m_context->RSSetViewports(1, &oldViewport);
return ApplyCompose(m_srvBlurV, color, m_rtvFinal);
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

bool BloomEffect::CreateTextures()
{
D3D11_TEXTURE2D_DESC texDesc = {};
uint32_t bloomWidth = static_cast<uint32_t>(m_width * m_defaultParams.downscale);
uint32_t bloomHeight = static_cast<uint32_t>(m_height * m_defaultParams.downscale);

texDesc.Width = bloomWidth;
texDesc.Height = bloomHeight;
texDesc.MipLevels = 1;
texDesc.ArraySize = 1;
texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
return false;
}
return true;
};

if (!CreateTextureResources(&m_texBright, &m_rtvBright, &m_srvBright) ||
!CreateTextureResources(&m_texBlurH, &m_rtvBlurH, &m_srvBlurH) ||
!CreateTextureResources(&m_texBlurV, &m_rtvBlurV, &m_srvBlurV))
return false;

texDesc.Width = m_width;
texDesc.Height = m_height;
if (!CreateTextureResources(&m_texFinal, &m_rtvFinal, &m_srvFinal))
return false;

D3D11_BUFFER_DESC cbDesc = {};
cbDesc.Usage = D3D11_USAGE_DYNAMIC;
cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

cbDesc.ByteWidth = sizeof(BloomCB);
if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbBloom)))
return false;

cbDesc.ByteWidth = sizeof(BloomIntensityCB);
if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbIntensity)))
return false;

cbDesc.ByteWidth = sizeof(ColorCB);
if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbColor)))
return false;

return true;
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
static_cast<BloomCB*>(mapped.pData)->threshold = threshold;
m_context->Unmap(m_cbBloom, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbBloom);
m_context->PSSetShaderResources(0, 1, &source);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(6, 0);

ID3D11ShaderResourceView* nullSRV = nullptr;
m_context->PSSetShaderResources(0, 1, &nullSRV);
return true;
}

bool BloomEffect::ApplyBlurH(ID3D11ShaderResourceView* source, float intensity, ID3D11RenderTargetView* target)
{
m_context->OMSetRenderTargets(1, &target, nullptr);
float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
m_context->ClearRenderTargetView(target, clearColor);

m_context->VSSetShader(m_vsFullscreen, nullptr, 0);
m_context->PSSetShader(m_psBlurH, nullptr, 0);

D3D11_MAPPED_SUBRESOURCE mapped;
if (SUCCEEDED(m_context->Map(m_cbIntensity, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
{
static_cast<BloomIntensityCB*>(mapped.pData)->intensity = intensity;
m_context->Unmap(m_cbIntensity, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbIntensity);
m_context->PSSetShaderResources(0, 1, &source);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(6, 0);

ID3D11ShaderResourceView* nullSRV = nullptr;
m_context->PSSetShaderResources(0, 1, &nullSRV);
return true;
}

bool BloomEffect::ApplyBlurV(ID3D11ShaderResourceView* source, float intensity, ID3D11RenderTargetView* target)
{
m_context->OMSetRenderTargets(1, &target, nullptr);
float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
m_context->ClearRenderTargetView(target, clearColor);

m_context->VSSetShader(m_vsFullscreen, nullptr, 0);
m_context->PSSetShader(m_psBlurV, nullptr, 0);

D3D11_MAPPED_SUBRESOURCE mapped;
if (SUCCEEDED(m_context->Map(m_cbIntensity, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
{
static_cast<BloomIntensityCB*>(mapped.pData)->intensity = intensity;
m_context->Unmap(m_cbIntensity, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbIntensity);
m_context->PSSetShaderResources(0, 1, &source);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(6, 0);

ID3D11ShaderResourceView* nullSRV = nullptr;
m_context->PSSetShaderResources(0, 1, &nullSRV);
return true;
}

bool BloomEffect::ApplyCompose(ID3D11ShaderResourceView* blurred, const BloomColor& color, ID3D11RenderTargetView* target)
{
m_context->OMSetRenderTargets(1, &target, nullptr);
float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
m_context->ClearRenderTargetView(target, clearColor);

m_context->VSSetShader(m_vsFullscreen, nullptr, 0);
m_context->PSSetShader(m_psCompose, nullptr, 0);

D3D11_MAPPED_SUBRESOURCE mapped;
if (SUCCEEDED(m_context->Map(m_cbColor, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
{
static_cast<ColorCB*>(mapped.pData)->colorTint = color.ToXMFLOAT4();
m_context->Unmap(m_cbColor, 0);
}

m_context->PSSetConstantBuffers(0, 1, &m_cbColor);
m_context->PSSetShaderResources(0, 1, &blurred);
m_context->PSSetSamplers(0, 1, &m_samplerLinear);
m_context->Draw(6, 0);

ID3D11ShaderResourceView* nullSRV = nullptr;
m_context->PSSetShaderResources(0, 1, &nullSRV);
return true;
}

std::unique_ptr<BloomEffect> CreateBloomEffect(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height)
{
auto effect = std::make_unique<BloomEffect>();
return effect->Initialize(device, context, width, height) ? std::move(effect) : nullptr;
}
}
