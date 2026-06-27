#include "SvCheats.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <Windows.h>

namespace Nemesis::SvCheats
{
    using namespace Nemesis::Addresses;

    namespace
    {
        using ResolveConVarFn = void* (__fastcall*)(void* convarRef, int slot);

        std::atomic<bool> g_running{ false };
        std::thread       g_worker;

        void* SafeResolve(ResolveConVarFn fn, void* ref)
        {
            __try { return fn(ref, -1); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        }

        bool ForceAt(std::uintptr_t valueAddr)
        {
            if (!valueAddr)
                return false;

            const std::uint8_t cur = Mem::Read<std::uint8_t>(valueAddr, 0xFF);
            if (cur > 1)
                return false;

            if (cur == 0)
                Mem::Write<std::uint8_t>(valueAddr, 1);

            return true;
        }

        std::uintptr_t ResolveValue(std::uintptr_t base, ResolveConVarFn resolve, std::uintptr_t refRva)
        {
            return reinterpret_cast<std::uintptr_t>(
                SafeResolve(resolve, reinterpret_cast<void*>(base + refRva)));
        }

        std::uintptr_t FallbackValue(std::uintptr_t base)
        {
            const std::uintptr_t convar = Mem::Read<std::uintptr_t>(base + Client::dwSvCheatsConVar);
            if (!convar)
                return 0;
            return Mem::Read<std::uintptr_t>(convar + 0x8);
        }

        void Tick(std::uintptr_t base)
        {
            const auto resolve = reinterpret_cast<ResolveConVarFn>(base + Client::fnResolveConVarValue);

            ForceAt(ResolveValue(base, resolve, Client::dwSvCheatsRef2));
            ForceAt(ResolveValue(base, resolve, Client::dwSvCheatsRef));
            ForceAt(FallbackValue(base));
        }

        void Worker()
        {
            while (g_running.load())
            {
                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
                if (base)
                    Tick(base);

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void Start()
    {
        if (g_running.exchange(true))
            return;

        g_worker = std::thread(Worker);
    }

    void Stop()
    {
        if (!g_running.exchange(false))
            return;

        if (g_worker.joinable())
            g_worker.join();
    }
}
