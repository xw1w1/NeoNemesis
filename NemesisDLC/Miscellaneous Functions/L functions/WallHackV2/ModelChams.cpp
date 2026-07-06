#include "ModelChams.hpp"
#include "WallHackV2Config.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <MinHook.h>

#include <atomic>
#include <cstdio>

#include "kiero.hpp"
#include "kiero_d3d11.hpp"

namespace Nemesis::ModelChams
{
    namespace Cfg = Nemesis::WallHackV2::Config;

    namespace
    {
        using DrawIndexedFn = void(__stdcall*)(ID3D11DeviceContext*, UINT, UINT, INT);

        DrawIndexedFn            g_origDrawIndexed = nullptr;
        void*                    g_target = nullptr;
        std::atomic<bool>        g_enabled{ true };

        ID3D11PixelShader*       g_ps = nullptr;
        ID3D11DepthStencilState* g_depth = nullptr;
        bool                     g_resTried = false;

        bool EnsureResources(ID3D11DeviceContext* ctx)
        {
            if (g_ps && g_depth)
                return true;
            if (g_resTried)
                return false;
            g_resTried = true;

            ID3D11Device* device = nullptr;
            ctx->GetDevice(&device);
            if (!device)
                return false;

            char src[256];
            std::snprintf(src, sizeof(src),
                "float4 main() : SV_TARGET { return float4(%f, %f, %f, 1.0f); }",
                Cfg::kChamsR, Cfg::kChamsG, Cfg::kChamsB);

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            if (FAILED(D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
                                  "main", "ps_4_0", 0, 0, &blob, &err)))
            {
                if (err) { NWARN("[chams] PS compile: %s", (const char*)err->GetBufferPointer()); err->Release(); }
                device->Release();
                return false;
            }
            if (err) err->Release();

            const HRESULT hr = device->CreatePixelShader(
                blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &g_ps);
            blob->Release();
            if (FAILED(hr))
            {
                device->Release();
                return false;
            }

            D3D11_DEPTH_STENCIL_DESC dd{};
            dd.DepthEnable = TRUE;
            dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
            if (FAILED(device->CreateDepthStencilState(&dd, &g_depth)))
            {
                g_ps->Release(); g_ps = nullptr;
                device->Release();
                return false;
            }

            device->Release();
            NLOG("[chams] resources ready");
            return true;
        }

        bool LooksLikeModel(ID3D11DeviceContext* ctx, UINT indexCount)
        {
            if (indexCount < Cfg::kChamsMinIndex || indexCount > Cfg::kChamsMaxIndex)
                return false;

            D3D11_PRIMITIVE_TOPOLOGY topo;
            ctx->IAGetPrimitiveTopology(&topo);
            if (topo != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
                return false;

            ID3D11Buffer* vb[2] = {};
            UINT stride[2] = {};
            UINT offset[2] = {};
            ctx->IAGetVertexBuffers(0, 2, vb, stride, offset);
            if (vb[0]) vb[0]->Release();
            if (vb[1]) vb[1]->Release();
            if (!stride[1])
                return false;

            ID3D11ShaderResourceView* srv = nullptr;
            ctx->VSGetShaderResources(0, 1, &srv);
            const bool skinned = srv != nullptr;
            if (srv) srv->Release();
            return skinned;
        }

        void __stdcall hkDrawIndexed(ID3D11DeviceContext* ctx, UINT indexCount,
                                     UINT startIndex, INT baseVertex)
        {
            if (!g_enabled.load() || !LooksLikeModel(ctx, indexCount) || !EnsureResources(ctx))
            {
                g_origDrawIndexed(ctx, indexCount, startIndex, baseVertex);
                return;
            }

            ID3D11PixelShader* prevPs = nullptr;
            ID3D11ClassInstance* inst[8] = {};
            UINT instCount = 8;
            ctx->PSGetShader(&prevPs, inst, &instCount);

            ID3D11DepthStencilState* prevDepth = nullptr;
            UINT prevRef = 0;
            ctx->OMGetDepthStencilState(&prevDepth, &prevRef);

            ctx->PSSetShader(g_ps, nullptr, 0);
            ctx->OMSetDepthStencilState(g_depth, 0);
            g_origDrawIndexed(ctx, indexCount, startIndex, baseVertex);

            ctx->PSSetShader(prevPs, instCount ? inst : nullptr, instCount);
            ctx->OMSetDepthStencilState(prevDepth, prevRef);
            if (prevPs) prevPs->Release();
            if (prevDepth) prevDepth->Release();
            for (UINT i = 0; i < instCount; ++i)
                if (inst[i]) inst[i]->Release();

            g_origDrawIndexed(ctx, indexCount, startIndex, baseVertex);
        }
    }

    void Start()
    {
        kiero::D3D11Output output;
        if (kiero::locate<kiero::Implementation_D3D11>(nullptr, &output) != kiero::Error_Nil)
        {
            NWARN("[chams] D3D11 locate failed");
            return;
        }
        if (output.context_methods.size() <= 12)
        {
            NWARN("[chams] context methods missing");
            return;
        }

        g_target = output.context_methods[12];
        if (MH_CreateHook(g_target, reinterpret_cast<void*>(&hkDrawIndexed),
                          reinterpret_cast<void**>(&g_origDrawIndexed)) == MH_OK &&
            MH_EnableHook(g_target) == MH_OK)
        {
            NLOG("[chams] DrawIndexed hooked");
        }
        else
        {
            NWARN("[chams] hook failed");
            g_target = nullptr;
        }
    }

    void Stop()
    {
        if (g_target)
            MH_DisableHook(g_target);
        if (g_ps) { g_ps->Release(); g_ps = nullptr; }
        if (g_depth) { g_depth->Release(); g_depth = nullptr; }
    }
}
