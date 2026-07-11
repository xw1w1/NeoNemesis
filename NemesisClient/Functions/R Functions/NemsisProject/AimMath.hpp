#pragma once


#include <cstdint>
#include <cmath>
#include <cstring>

namespace Nemesis::AimMath
{
    constexpr float kPi    = 3.14159265358979323846f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr float kDeg2Rad = kPi / 180.0f;
    constexpr float kRad2Deg = 180.0f / kPi;

    
    class ValveRNG
    {
        static constexpr int NTAB = 32;
        static constexpr int IA = 16807, IM = 2147483647, IQ = 127773, IR = 2836;
        static constexpr int NDIV = 1 + (IM - 1) / NTAB;
        static constexpr float AM = 1.0f / IM;

        int m_idum = 0, m_iy = 0, m_iv[NTAB] = {};

    public:
        void Seed(int seed)
        {
            m_idum = (seed < 1) ? 1 : seed;   
            for (int j = NTAB + 7; j >= 0; --j)
            {
                int k = m_idum / IQ;
                m_idum = IA * (m_idum - k * IQ) - IR * k;
                if (m_idum < 0) m_idum += IM;
                if (j < NTAB) m_iv[j] = m_idum;
            }
            m_iy = m_iv[0];
        }

        int NextInt()
        {
            int k = m_idum / IQ;
            m_idum = IA * (m_idum - k * IQ) - IR * k;
            if (m_idum < 0) m_idum += IM;
            int j = m_iy / NDIV;
            m_iy = m_iv[j];
            m_iv[j] = m_idum;
            return m_iy;
        }

        float Float(float lo, float hi)
        {
            float f = NextInt() * AM;
            if (f > 1.0f) f = 1.0f;
            return lo + f * (hi - lo);
        }
    };

    
    inline float NormalizeAngle(float a)
    {
        a = std::fmod(a, 360.0f);
        if (a > 180.0f)  a -= 360.0f;
        if (a < -180.0f) a += 360.0f;
        return a;
    }
    inline float Quantize(float a)
    {
        return std::roundf(NormalizeAngle(a) * 2.0f) * 0.5f;
    }

  
    namespace detail
    {
        inline std::uint32_t Rol(std::uint32_t v, int b)
        {
            return (v << b) | (v >> (32 - b));
        }
        inline void Sha1_12(const std::uint8_t* msg, std::uint8_t out[20])
        {
            std::uint8_t block[64] = {};
            for (int i = 0; i < 12; ++i) block[i] = msg[i];
            block[12] = 0x80;
            block[63] = 12 * 8;
            std::uint32_t H0 = 0x67452301, H1 = 0xEFCDAB89, H2 = 0x98BADCFE,
                          H3 = 0x10325476, H4 = 0xC3D2E1F0;
            std::uint32_t w[80];
            for (int i = 0; i < 16; ++i)
                w[i] = (static_cast<std::uint32_t>(block[i*4]) << 24) |
                       (static_cast<std::uint32_t>(block[i*4+1]) << 16) |
                       (static_cast<std::uint32_t>(block[i*4+2]) << 8) |
                       (static_cast<std::uint32_t>(block[i*4+3]));
            for (int i = 16; i < 80; ++i)
                w[i] = Rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
            std::uint32_t a = H0, b = H1, c = H2, d = H3, e = H4;
            for (int i = 0; i < 80; ++i)
            {
                std::uint32_t f, k;
                if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
                else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }
                std::uint32_t t = Rol(a, 5) + f + e + k + w[i];
                e = d; d = c; c = Rol(b, 30); b = a; a = t;
            }
            H0 += a; H1 += b; H2 += c; H3 += d; H4 += e;
            const std::uint32_t hh[5] = { H0, H1, H2, H3, H4 };
            for (int i = 0; i < 5; ++i)
            {
                out[i*4+0] = static_cast<std::uint8_t>(hh[i] >> 24);
                out[i*4+1] = static_cast<std::uint8_t>(hh[i] >> 16);
                out[i*4+2] = static_cast<std::uint8_t>(hh[i] >> 8);
                out[i*4+3] = static_cast<std::uint8_t>(hh[i] & 0xFF);
            }
        }
    }

    inline int GenerateSeed(float qPitch, float qYaw, std::uint32_t seedBase)
    {
        std::uint8_t buf[12];
        std::memcpy(buf + 0, &qPitch, 4);
        std::memcpy(buf + 4, &qYaw, 4);
        std::memcpy(buf + 8, &seedBase, 4);
        std::uint8_t dig[20];
        detail::Sha1_12(buf, dig);
        int seed;
        std::memcpy(&seed, dig, 4);
        return seed;
    }

    
   
    inline void PredictSpread0(int seed, float inaccuracy, float spread,
                               float& dx, float& dy)
    {
        ValveRNG rng;
        rng.Seed(seed);
   
        float r1 = rng.Float(0.0f, 1.0f);
        float t1 = rng.Float(0.0f, kTwoPi);
        r1 *= inaccuracy;
        float r2 = rng.Float(0.0f, 1.0f);
        float t2 = rng.Float(0.0f, kTwoPi);
        r2 *= spread;

        dx = r1 * std::cos(t1) + r2 * std::cos(t2);
        dy = r1 * std::sin(t1) + r2 * std::sin(t2);
    }

    
   
    inline void SolveWriteAngle(float aimPitch, float aimYaw,
                                float punchP, float punchY,
                                std::uint32_t seedBase, float inaccuracy, float spread,
                                float recoilScale,
                                float& outPitch, float& outYaw)
    {
        
        const float basePitch = aimPitch - punchP * recoilScale;
        const float baseYaw   = aimYaw   - punchY * recoilScale;

        
        float wp = basePitch, wy = baseYaw;
        for (int it = 0; it < 4; ++it)
        {
            const int seed = GenerateSeed(Quantize(wp), Quantize(wy), seedBase);
            float dx, dy;
            PredictSpread0(seed, inaccuracy, spread, dx, dy);

           
            const float dYawDeg   = std::atan(dx) * kRad2Deg;
            const float dPitchDeg = std::atan(dy) * kRad2Deg;

            const float nwp = basePitch - dPitchDeg;
            const float nwy = baseYaw   - dYawDeg;

            if (std::fabs(nwp - wp) < 1e-4f && std::fabs(nwy - wy) < 1e-4f)
            {
                wp = nwp; wy = nwy;
                break;
            }
            wp = nwp; wy = nwy;
        }
        outPitch = NormalizeAngle(wp);
        outYaw   = NormalizeAngle(wy);
    }
}
