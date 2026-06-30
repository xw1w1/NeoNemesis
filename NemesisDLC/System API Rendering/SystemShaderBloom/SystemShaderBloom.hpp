#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>

namespace Nemesis::SystemShaderBloom
{
	struct BloomParams
	{
		float threshold = 1.0f;
		float intensity = 1.0f;
		float blurRadius = 2.0f;
		float downscale = 0.5f;
	};

	struct BloomColor
	{
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;

		BloomColor() = default;
		BloomColor(float r, float g, float b, float a = 1.0f) 
			: r(r), g(g), b(b), a(a) {}

		static BloomColor FromRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
		{
			return BloomColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
		}

		static BloomColor FromARGB32(uint32_t color)
		{
			uint8_t a = (color >> 24) & 0xFF;
			uint8_t r = (color >> 16) & 0xFF;
			uint8_t g = (color >> 8) & 0xFF;
			uint8_t b = color & 0xFF;
			return FromRGBA8(r, g, b, a);
		}

		DirectX::XMFLOAT4 ToXMFLOAT4() const
		{
			return DirectX::XMFLOAT4(r, g, b, a);
		}
	};

	class BloomEffect
	{
	public:
		BloomEffect();
		~BloomEffect();

		bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, 
					   uint32_t width, uint32_t height);

		void Release();

		bool Render(ID3D11ShaderResourceView* sourceTexture, 
				   const BloomColor& color,
				   const BloomParams& params = BloomParams());

		ID3D11ShaderResourceView* GetOutputTexture() const;

		void SetDefaultParams(const BloomParams& params);

		bool IsInitialized() const { return m_initialized; }

	private:
		bool CompileShaders();
		bool CreateTextures();
		bool CreateSamplers();

		bool ApplyThreshold(ID3D11ShaderResourceView* source, float threshold, 
						   ID3D11RenderTargetView* target);

		bool ApplyBlurH(ID3D11ShaderResourceView* source, float intensity,
					   ID3D11RenderTargetView* target);

		bool ApplyBlurV(ID3D11ShaderResourceView* source, float intensity,
					   ID3D11RenderTargetView* target);

		bool ApplyCompose(ID3D11ShaderResourceView* blurred, const BloomColor& color,
						 ID3D11RenderTargetView* target);

		ID3D11Device*              m_device = nullptr;
		ID3D11DeviceContext*       m_context = nullptr;

		ID3D11PixelShader*         m_psThreshold = nullptr;
		ID3D11PixelShader*         m_psBlurH = nullptr;
		ID3D11PixelShader*         m_psBlurV = nullptr;
		ID3D11PixelShader*         m_psCompose = nullptr;
		ID3D11VertexShader*        m_vsFullscreen = nullptr;

		ID3D11InputLayout*         m_inputLayout = nullptr;

		ID3D11Buffer*              m_cbBloom = nullptr;
		ID3D11Buffer*              m_cbIntensity = nullptr;
		ID3D11Buffer*              m_cbColor = nullptr;

		ID3D11Texture2D*           m_texBright = nullptr;
		ID3D11ShaderResourceView*  m_srvBright = nullptr;
		ID3D11RenderTargetView*    m_rtvBright = nullptr;

		ID3D11Texture2D*           m_texBlurH = nullptr;
		ID3D11ShaderResourceView*  m_srvBlurH = nullptr;
		ID3D11RenderTargetView*    m_rtvBlurH = nullptr;

		ID3D11Texture2D*           m_texBlurV = nullptr;
		ID3D11ShaderResourceView*  m_srvBlurV = nullptr;
		ID3D11RenderTargetView*    m_rtvBlurV = nullptr;

		ID3D11Texture2D*           m_texFinal = nullptr;
		ID3D11ShaderResourceView*  m_srvFinal = nullptr;
		ID3D11RenderTargetView*    m_rtvFinal = nullptr;

		ID3D11SamplerState*        m_samplerLinear = nullptr;

		uint32_t                   m_width = 0;
		uint32_t                   m_height = 0;
		BloomParams                m_defaultParams;
		bool                       m_initialized = false;
	};

	std::unique_ptr<BloomEffect> CreateBloomEffect(ID3D11Device* device, 
												   ID3D11DeviceContext* context,
												   uint32_t width, uint32_t height);
}
