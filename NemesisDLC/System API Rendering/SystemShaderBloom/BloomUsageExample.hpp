#pragma once

#include "SystemShaderBloom.hpp"

namespace Nemesis::SystemShaderBloom::Examples
{

inline BloomParams SoftBloom()
{
return BloomParams::Soft();
}

inline BloomParams StrongBloom()
{
return BloomParams::Strong();
}

class SimpleBloom
{
public:
bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height)
{
m_bloom = CreateBloomEffect(device, context, width, height);
return m_bloom != nullptr;
}

bool Render(ID3D11ShaderResourceView* source)
{
return Render(source, BloomColor::White());
}

bool Render(ID3D11ShaderResourceView* source, const BloomColor& color)
{
return m_bloom && m_bloom->Render(source, color);
}

bool Render(ID3D11ShaderResourceView* source, const BloomColor& color, const BloomParams& params)
{
return m_bloom && m_bloom->Render(source, color, params);
}

ID3D11ShaderResourceView* Output() const
{
return m_bloom ? m_bloom->GetOutputTexture() : nullptr;
}

void SetDefault(const BloomParams& params)
{
if (m_bloom)
m_bloom->SetDefaultParams(params);
}

private:
std::unique_ptr<BloomEffect> m_bloom;
};

inline bool BasicUsage(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* source)
{
SimpleBloom bloom;
if (!bloom.Initialize(device, context, 1920, 1080))
return false;

if (!bloom.Render(source, BloomColor::White()))
return false;

ID3D11ShaderResourceView* result = bloom.Output();
return result != nullptr;
}

}
