#pragma once

#include "SystemShaderBloom.hpp"
#include "BloomColor.hpp"

namespace Nemesis::SystemShaderBloom::Examples
{
void BasicBloomUsage(ID3D11Device* device, ID3D11DeviceContext* context)
{
std::unique_ptr<BloomEffect> bloom = CreateBloomEffect(device, context, 1920, 1080);

if (bloom && bloom->IsInitialized())
{
BloomColor color = BloomColor(1.0f, 1.0f, 1.0f, 1.0f);
BloomParams params;
params.threshold = 0.8f;
params.intensity = 1.5f;
params.blurRadius = 3.0f;

ID3D11ShaderResourceView* sourceTexture = nullptr;
bloom->Render(sourceTexture, color, params);

ID3D11ShaderResourceView* result = bloom->GetOutputTexture();
}
}

void RenderWithDifferentColors(ID3D11Device* device, ID3D11DeviceContext* context,
   ID3D11ShaderResourceView* sourceTexture)
{
auto bloom = CreateBloomEffect(device, context, 1280, 720);

BloomParams defaultParams;
defaultParams.threshold = 1.0f;
bloom->SetDefaultParams(defaultParams);

bloom->Render(sourceTexture, BloomColor(1.0f, 0.0f, 0.0f, 1.0f));
bloom->Render(sourceTexture, BloomColor(0.0f, 0.0f, 1.0f, 1.0f));
bloom->Render(sourceTexture, BloomColor(1.0f, 0.784f, 0.0f, 1.0f));
}

class BloomManager
{
private:
std::unique_ptr<BloomEffect> m_bloom;

public:
bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height)
{
m_bloom = CreateBloomEffect(device, context, width, height);
return m_bloom && m_bloom->IsInitialized();
}

void Render(ID3D11ShaderResourceView* source, const BloomColor& color)
{
if (m_bloom)
m_bloom->Render(source, color);
}

ID3D11ShaderResourceView* GetResult() const
{
return m_bloom ? m_bloom->GetOutputTexture() : nullptr;
}

void SetParams(const BloomParams& params)
{
if (m_bloom)
m_bloom->SetDefaultParams(params);
}
};

void ParameterTuning(ID3D11Device* device, ID3D11DeviceContext* context,
   ID3D11ShaderResourceView* source)
{
auto bloom = CreateBloomEffect(device, context, 1920, 1080);

BloomParams weakBloom;
weakBloom.threshold = 0.9f;
weakBloom.intensity = 0.5f;
weakBloom.blurRadius = 1.0f;
bloom->Render(source, BloomColor(1.0f, 1.0f, 1.0f, 1.0f), weakBloom);

BloomParams strongBloom;
strongBloom.threshold = 0.5f;
strongBloom.intensity = 2.0f;
strongBloom.blurRadius = 5.0f;
bloom->Render(source, BloomColor(1.0f, 1.0f, 1.0f, 1.0f), strongBloom);
}

void ColorConversions()
{
Color fromRGB = Color::FromRGB8(255, 128, 64);
Color fromRGBA = Color::FromRGBA8(255, 128, 64, 200);
Color fromARGB = Color::FromARGB32(0xFFFF8040);
Color fromABGR = Color::FromABGR32(0xFF4080FF);

uint32_t toARGB = fromRGB.ToARGB32();
uint32_t toABGR = fromRGB.ToABGR32();

DirectX::XMFLOAT4 xmfloat = fromRGB.ToXMFLOAT4();
DirectX::XMVECTOR xmvec = fromRGB.ToXMVector();
}

void ColorOperations()
{
Color base(1.0f, 0.5f, 0.25f, 1.0f);
Color scaled = base * 0.5f;
Color added = base + Color::Red;

base *= 2.0f;
base += Color::Blue;

float lum = base.GetLuminance();
}

void PresetColors()
{
Color white = Color::White;
Color black = Color::Black;
Color red = Color::Red;
Color green = Color::Green;
Color blue = Color::Blue;
Color yellow = Color::Yellow;
Color cyan = Color::Cyan;
Color magenta = Color::Magenta;
Color orange = Color::Orange;
Color purple = Color::Purple;
Color pink = Color::Pink;
Color lime = Color::Lime;
}
}
