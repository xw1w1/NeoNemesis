#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace Nemesis::SystemShaderBloom
{
	class Color
	{
	public:
		union
		{
			struct
			{
				float r, g, b, a;
			};
			DirectX::XMFLOAT4 xmfloat;
		};

		Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}

		Color(float r, float g, float b, float a = 1.0f) 
			: r(r), g(g), b(b), a(a) {}

		explicit Color(const DirectX::XMFLOAT4& color)
			: xmfloat(color) {}

		static Color FromRGB8(uint8_t r, uint8_t g, uint8_t b)
		{
			return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
		}

		static Color FromRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
		{
			return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
		}

		static Color FromARGB32(uint32_t color)
		{
			uint8_t a = (color >> 24) & 0xFF;
			uint8_t r = (color >> 16) & 0xFF;
			uint8_t g = (color >> 8) & 0xFF;
			uint8_t b = color & 0xFF;
			return FromRGBA8(r, g, b, a);
		}

		static Color FromABGR32(uint32_t color)
		{
			uint8_t a = (color >> 24) & 0xFF;
			uint8_t b = (color >> 16) & 0xFF;
			uint8_t g = (color >> 8) & 0xFF;
			uint8_t r = color & 0xFF;
			return FromRGBA8(r, g, b, a);
		}

		DirectX::XMFLOAT4 ToXMFLOAT4() const
		{
			return DirectX::XMFLOAT4(r, g, b, a);
		}

		DirectX::XMVECTOR ToXMVector() const
		{
			return DirectX::XMLoadFloat4(&xmfloat);
		}

		uint32_t ToARGB32() const
		{
			uint8_t ar = static_cast<uint8_t>(a * 255.0f + 0.5f);
			uint8_t rr = static_cast<uint8_t>(r * 255.0f + 0.5f);
			uint8_t gr = static_cast<uint8_t>(g * 255.0f + 0.5f);
			uint8_t br = static_cast<uint8_t>(b * 255.0f + 0.5f);
			return (ar << 24) | (rr << 16) | (gr << 8) | br;
		}

		uint32_t ToABGR32() const
		{
			uint8_t ar = static_cast<uint8_t>(a * 255.0f + 0.5f);
			uint8_t br = static_cast<uint8_t>(b * 255.0f + 0.5f);
			uint8_t gr = static_cast<uint8_t>(g * 255.0f + 0.5f);
			uint8_t rr = static_cast<uint8_t>(r * 255.0f + 0.5f);
			return (ar << 24) | (br << 16) | (gr << 8) | rr;
		}

		Color operator*(float scalar) const
		{
			return Color(r * scalar, g * scalar, b * scalar, a * scalar);
		}

		Color operator+(const Color& other) const
		{
			return Color(r + other.r, g + other.g, b + other.b, a + other.a);
		}

		Color& operator*=(float scalar)
		{
			r *= scalar;
			g *= scalar;
			b *= scalar;
			a *= scalar;
			return *this;
		}

		Color& operator+=(const Color& other)
		{
			r += other.r;
			g += other.g;
			b += other.b;
			a += other.a;
			return *this;
		}

		float GetLuminance() const
		{
			return 0.299f * r + 0.587f * g + 0.114f * b;
		}

		static const Color White;
		static const Color Black;
		static const Color Red;
		static const Color Green;
		static const Color Blue;
		static const Color Yellow;
		static const Color Cyan;
		static const Color Magenta;
		static const Color Orange;
		static const Color Purple;
		static const Color Pink;
		static const Color Lime;
	};

	inline const Color Color::White(1.0f, 1.0f, 1.0f, 1.0f);
	inline const Color Color::Black(0.0f, 0.0f, 0.0f, 1.0f);
	inline const Color Color::Red(1.0f, 0.0f, 0.0f, 1.0f);
	inline const Color Color::Green(0.0f, 1.0f, 0.0f, 1.0f);
	inline const Color Color::Blue(0.0f, 0.0f, 1.0f, 1.0f);
	inline const Color Color::Yellow(1.0f, 1.0f, 0.0f, 1.0f);
	inline const Color Color::Cyan(0.0f, 1.0f, 1.0f, 1.0f);
	inline const Color Color::Magenta(1.0f, 0.0f, 1.0f, 1.0f);
	inline const Color Color::Orange(1.0f, 0.5f, 0.0f, 1.0f);
	inline const Color Color::Purple(0.5f, 0.0f, 0.5f, 1.0f);
	inline const Color Color::Pink(1.0f, 0.192f, 0.203f, 1.0f);
	inline const Color Color::Lime(0.5f, 1.0f, 0.0f, 1.0f);
}
