#include "Visibility.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../LogsSystem/LogsSystem.hpp"

#include <cstdint>
#include <Windows.h>

namespace Nemesis::Visibility
{
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr std::uintptr_t kTraceMgr    = 0x20459C0;
        constexpr std::uintptr_t kFnTrace     = 0x9913A0;
        constexpr std::uintptr_t kRayVtable   = 0x193D358;
        constexpr std::uintptr_t kFnBuildFilt = 0x21A6E0;
        constexpr std::ptrdiff_t kFraction    = 0xAC;   // CGameTrace.fraction
        constexpr std::ptrdiff_t kBoneArray   = 0x1D0;
        constexpr int            kHeadBone    = 6;

        using TraceFn       = char (__fastcall*)(void*, void*, void*, void*, void*, void*);
        using BuildFilterFn = void* (__fastcall*)(void*, void*);

        std::uintptr_t EntByIndex(std::uintptr_t base, int index)
        {
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list)
                return 0;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (static_cast<std::uint32_t>(index) >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk)
                return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (static_cast<std::uint32_t>(index) & EntityList::kSlotMask));
        }

        bool TargetHead(std::uintptr_t base, int pawnIndex, float out[3])
        {
            const std::uintptr_t pawn = EntByIndex(base, pawnIndex);
            if (!pawn)
                return false;
            const std::uintptr_t scene = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (!scene)
                return false;
            const std::uintptr_t bones = Mem::Read<std::uintptr_t>(scene + kBoneArray);
            if (bones < 0x10000)
                return false;
            const std::uintptr_t b = bones + kHeadBone * 0x20;
            out[0] = Mem::Read<float>(b + 0);
            out[1] = Mem::Read<float>(b + 4);
            out[2] = Mem::Read<float>(b + 8);
            return out[0] != 0.0f || out[1] != 0.0f || out[2] != 0.0f;
        }

        // Возвращает fraction [0..1], или -1 при ошибке.
        float TraceFraction(std::uintptr_t base, float start[3], float end[3])
        {
            __try
            {
                const std::uintptr_t mgr = Mem::Read<std::uintptr_t>(base + kTraceMgr);
                if (!mgr)
                    return -1.0f;

                alignas(16) unsigned char ray[0x140] = {};
                *reinterpret_cast<std::uintptr_t*>(ray) = base + kRayVtable;

                alignas(16) unsigned char filter[0x140] = {};
                float tmpVec[3] = { 0.0f, 0.0f, 0.0f };
                auto buildFilter = reinterpret_cast<BuildFilterFn>(base + kFnBuildFilt);
                buildFilter(filter, tmpVec);

                alignas(16) unsigned char trace[0x400] = {};

                auto traceFn = reinterpret_cast<TraceFn>(base + kFnTrace);
                traceFn(reinterpret_cast<void*>(mgr), filter, start, end, ray, trace);

                return *reinterpret_cast<const float*>(trace + kFraction);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -1.0f;
            }
        }

        bool CrosshairVisible(std::uintptr_t localPawn, int targetPawnIndex)
        {
            const int id = Mem::Read<int>(localPawn + Schema::m_iIDEntIndex, -1);
            return id == targetPawnIndex;
        }
    }

    bool VisibleToLocal(std::uintptr_t clientBase, std::uintptr_t localPawn, int targetPawnIndex)
    {
        if (!localPawn || targetPawnIndex < 0)
            return false;

        const std::uintptr_t scene = Mem::Read<std::uintptr_t>(localPawn + Schema::m_pGameSceneNode);
        float head[3];
        if (!scene || !TargetHead(clientBase, targetPawnIndex, head))
            return CrosshairVisible(localPawn, targetPawnIndex);

        float eye[3];
        eye[0] = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 0);
        eye[1] = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 4);
        eye[2] = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 8)
               + Mem::Read<float>(localPawn + Schema::m_vecViewOffset + 8);

        const float frac = TraceFraction(clientBase, eye, head);
        if (frac < 0.0f)
            return CrosshairVisible(localPawn, targetPawnIndex);

        static bool logged = false;
        if (!logged) { logged = true; NLOG("[trace] live frac=%.3f", frac); }

        return frac >= 0.95f;
    }

    bool VisiblePoint(std::uintptr_t clientBase, std::uintptr_t localPawn, const float target[3])
    {
        if (!localPawn || !target)
            return false;

        const std::uintptr_t scene = Mem::Read<std::uintptr_t>(localPawn + Schema::m_pGameSceneNode);
        if (!scene)
            return false;

        float eye[3];
        eye[0] = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 0);
        eye[1] = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 4);
        eye[2] = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 8)
               + Mem::Read<float>(localPawn + Schema::m_vecViewOffset + 8);

        float end[3] = { target[0], target[1], target[2] };
        const float frac = TraceFraction(clientBase, eye, end);
        if (frac < 0.0f)
            return false;
        return frac >= 0.95f;
    }
}
