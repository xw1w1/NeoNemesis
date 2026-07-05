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
	float knee;
	float2 padding;
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
	float soft = max(knee, 0.0001f);
	float contribution = saturate((lum - threshold + soft) / soft);
	contribution *= contribution;
	float scale = max(lum - threshold, 0.0f) / max(lum, 0.0001f);
	scale = max(scale, contribution * 0.25f);

	return float4(color.rgb * scale, color.a * scale);
}
)hlsl";

	constexpr std::string_view BlurHPS = R"hlsl(
Texture2D txInput : register(t0);
SamplerState sampLinear : register(s0);

cbuffer BloomParams : register(b0)
{
	float2 texelSize;
	float radius;
	float padding;
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
	float stepWidth = max(radius, 0.25f) * texelSize.x;

	float4 result = txInput.Sample(sampLinear, input.tex) * weights[0];
	result += txInput.Sample(sampLinear, input.tex + float2(stepWidth, 0.0f)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex - float2(stepWidth, 0.0f)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex + float2(stepWidth * 2.0f, 0.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex - float2(stepWidth * 2.0f, 0.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex + float2(stepWidth * 3.0f, 0.0f)) * weights[3];
	result += txInput.Sample(sampLinear, input.tex - float2(stepWidth * 3.0f, 0.0f)) * weights[3];

	return result;
}
)hlsl";

	constexpr std::string_view BlurVPS = R"hlsl(
Texture2D txInput : register(t0);
SamplerState sampLinear : register(s0);

cbuffer BloomParams : register(b0)
{
	float2 texelSize;
	float radius;
	float padding;
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
	float stepHeight = max(radius, 0.25f) * texelSize.y;

	float4 result = txInput.Sample(sampLinear, input.tex) * weights[0];
	result += txInput.Sample(sampLinear, input.tex + float2(0.0f, stepHeight)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex - float2(0.0f, stepHeight)) * weights[1];
	result += txInput.Sample(sampLinear, input.tex + float2(0.0f, stepHeight * 2.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex - float2(0.0f, stepHeight * 2.0f)) * weights[2];
	result += txInput.Sample(sampLinear, input.tex + float2(0.0f, stepHeight * 3.0f)) * weights[3];
	result += txInput.Sample(sampLinear, input.tex - float2(0.0f, stepHeight * 3.0f)) * weights[3];

	return result;
}
)hlsl";

	constexpr std::string_view ComposePS = R"hlsl(
Texture2D txSource : register(t0);
Texture2D txBlurred : register(t1);
SamplerState sampLinear : register(s0);

cbuffer BloomColor : register(b0)
{
	float4 colorTint;
	float intensity;
	float3 padding;
};

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
	float4 source = txSource.Sample(sampLinear, input.tex);
	float4 blurred = txBlurred.Sample(sampLinear, input.tex);
	float3 bloom = blurred.rgb * colorTint.rgb * intensity * colorTint.a;
	return float4(source.rgb + bloom, source.a);
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
