#pragma once

#include <string_view>

namespace Nemesis::SystemShaderBloom::Shaders
{
	constexpr std::string_view FullscreenVS = R"hlsl(
struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
	VS_OUTPUT output;
	output.tex = float2((vertexID << 1) & 2, vertexID & 2);
	output.pos = float4(output.tex * 2.0f - 1.0f, 0.0f, 1.0f);
	output.pos.y = -output.pos.y;
	return output;
}
)hlsl";

	constexpr std::string_view ThresholdPS = R"hlsl(
Texture2D txInput : register(t0);
SamplerState sampLinear : register(s0);

cbuffer BloomParams : register(b0)
{
	float threshold;
	float3 padding;
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 color = txInput.Sample(sampLinear, input.tex);
	float lum = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));

	if (lum > threshold)
		return float4(color.rgb, 1.0f);
	else
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
)hlsl";

	constexpr std::string_view BlurHPS = R"hlsl(
Texture2D txInput : register(t0);
SamplerState sampLinear : register(s0);

cbuffer BloomParams : register(b0)
{
	float intensity;
	float3 padding;
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

static const float weights[4] = 
{
	0.40261994f,
	0.24342268f,
	0.05387911f,
	0.00393785f
};

float4 main(PS_INPUT input) : SV_TARGET
{
	uint width, height;
	txInput.GetDimensions(width, height);
	float texelWidth = 1.0f / width;

	float4 result = txInput.Sample(sampLinear, input.tex) * weights[0];
	result += txInput.Sample(sampLinear, input.tex + float2(texelWidth, 0.0f)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex - float2(texelWidth, 0.0f)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex + float2(texelWidth * 2.0f, 0.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex - float2(texelWidth * 2.0f, 0.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex + float2(texelWidth * 3.0f, 0.0f)) * weights[3];
	result += txInput.Sample(sampLinear, input.tex - float2(texelWidth * 3.0f, 0.0f)) * weights[3];

	return result * intensity;
}
)hlsl";

	constexpr std::string_view BlurVPS = R"hlsl(
Texture2D txInput : register(t0);
SamplerState sampLinear : register(s0);

cbuffer BloomParams : register(b0)
{
	float intensity;
	float3 padding;
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

static const float weights[4] = 
{
	0.40261994f,
	0.24342268f,
	0.05387911f,
	0.00393785f
};

float4 main(PS_INPUT input) : SV_TARGET
{
	uint width, height;
	txInput.GetDimensions(width, height);
	float texelHeight = 1.0f / height;

	float4 result = txInput.Sample(sampLinear, input.tex) * weights[0];
	result += txInput.Sample(sampLinear, input.tex + float2(0.0f, texelHeight)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex - float2(0.0f, texelHeight)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex + float2(0.0f, texelHeight * 2.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex - float2(0.0f, texelHeight * 2.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex + float2(0.0f, texelHeight * 3.0f)) * weights[3];
	result += txInput.Sample(sampLinear, input.tex - float2(0.0f, texelHeight * 3.0f)) * weights[3];

	return result * intensity;
}
)hlsl";

	constexpr std::string_view ComposePS = R"hlsl(
Texture2D txBlurred : register(t0);
SamplerState sampLinear : register(s0);

cbuffer BloomColor : register(b0)
{
	float4 colorTint;
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 blurred = txBlurred.Sample(sampLinear, input.tex);
	float3 result = blurred.rgb * colorTint.rgb;
	return float4(result, blurred.a);
}
)hlsl";

	constexpr std::string_view ComposeMultiplyPS = R"hlsl(
Texture2D txBlurred : register(t0);
SamplerState sampLinear : register(s0);

cbuffer BloomColor : register(b0)
{
	float4 colorTint;
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 blurred = txBlurred.Sample(sampLinear, input.tex);
	float3 tinted = blurred.rgb * colorTint.rgb;
	float intensity = dot(tinted, float3(0.299f, 0.587f, 0.114f));
	tinted = normalize(tinted + float3(0.01f)) * intensity;
	return float4(tinted, blurred.a);
}
)hlsl";
}
