#include "SvCheats.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <Windows.h>
#include <TlHelp32.h>

namespace Nemesis::SvCheats
{
    using namespace Nemesis::Addresses;

    namespace
    {
        using ResolveConVarFn = void* (__fastcall*)(void* convarRef, int slot);

        constexpr std::uint8_t kPinned = 1;

        constexpr DWORD64 kDr7Enable  = 0x1;
        constexpr DWORD64 kDr7RwMask  = 0xF0000;
        constexpr DWORD64 kDr7RwWrite = 0x10000;

        std::atomic<bool>           g_running{ false };
        std::atomic<std::uintptr_t> g_valueAddr{ 0 };
        std::atomic<std::uintptr_t> g_clientBase{ 0 };
        std::atomic<std::uint64_t>  g_blocked{ 0 };

        std::thread g_worker;
        PVOID       g_veh = nullptr;

        thread_local bool g_reentry = false;

        void* SafeResolve(ResolveConVarFn fn, void* ref)
        {
            __try { return fn(ref, -1); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        }

        std::uintptr_t ResolveValue(std::uintptr_t base)
        {
            const std::uintptr_t convar = Mem::Read<std::uintptr_t>(base + Client::dwSvCheatsConVar);
            if (!convar)
                return 0;
            return Mem::Read<std::uintptr_t>(convar + 0x8);
        }

        LONG CALLBACK GuardHandler(EXCEPTION_POINTERS* info)
        {
            if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
                return EXCEPTION_CONTINUE_SEARCH;

            CONTEXT* ctx = info->ContextRecord;
            if (!(ctx->Dr6 & 0x1))
                return EXCEPTION_CONTINUE_SEARCH;

            ctx->Dr6 = 0;

            if (g_reentry)
                return EXCEPTION_CONTINUE_EXECUTION;

            const std::uintptr_t addr = g_valueAddr.load();
            if (!addr)
                return EXCEPTION_CONTINUE_EXECUTION;

            g_reentry = true;

            const std::uint8_t before = *reinterpret_cast<volatile std::uint8_t*>(addr);
            if (before != kPinned)
                *reinterpret_cast<volatile std::uint8_t*>(addr) = kPinned;

            g_reentry = false;

            const std::uintptr_t who  = reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress);
            const std::uintptr_t base = g_clientBase.load();
            const std::uint64_t  n    = g_blocked.fetch_add(1) + 1;

            if (base && who > base)
                NLOG("sv_cheats blocked #%llu wrote=%u from client+0x%llX", n, before, (unsigned long long)(who - base));
            else
                NLOG("sv_cheats blocked #%llu wrote=%u from 0x%llX", n, before, (unsigned long long)who);

            return EXCEPTION_CONTINUE_EXECUTION;
        }

        void ArmThread(HANDLE thread, bool isCurrent, std::uintptr_t addr)
        {
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

            if (!isCurrent)
                SuspendThread(thread);

            if (GetThreadContext(thread, &ctx))
            {
                ctx.Dr0 = addr;
                ctx.Dr7 = (ctx.Dr7 & ~kDr7RwMask) | kDr7RwWrite | kDr7Enable;
                ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                SetThreadContext(thread, &ctx);
            }

            if (!isCurrent)
                ResumeThread(thread);
        }

        void ArmAllThreads(std::uintptr_t addr)
        {
            const DWORD pid     = GetCurrentProcessId();
            const DWORD selfTid = GetCurrentThreadId();

            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap == INVALID_HANDLE_VALUE)
                return;

            THREADENTRY32 te{};
            te.dwSize = sizeof(te);

            if (Thread32First(snap, &te))
            {
                do
                {
                    if (te.th32OwnerProcessID != pid)
                        continue;

                    if (te.th32ThreadID == selfTid)
                    {
                        ArmThread(GetCurrentThread(), true, addr);
                        continue;
                    }

                    HANDLE th = OpenThread(
                        THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
                        FALSE, te.th32ThreadID);
                    if (th)
                    {
                        ArmThread(th, false, addr);
                        CloseHandle(th);
                    }
                } while (Thread32Next(snap, &te));
            }

            CloseHandle(snap);
        }

        void Worker()
        {
            while (g_running.load())
            {
                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
                if (base)
                {
                    g_clientBase.store(base);

                    std::uintptr_t addr = g_valueAddr.load();
                    if (!addr)
                    {
                        addr = ResolveValue(base);
                        if (addr)
                        {
                            Mem::Write<std::uint8_t>(addr, kPinned);
                            g_valueAddr.store(addr);
                            NLOG("sv_cheats guard armed at 0x%llX", (unsigned long long)addr);
                        }
                    }

                    if (addr)
                    {
                        const std::uint8_t cur = Mem::Read<std::uint8_t>(addr, kPinned);
                        if (cur != kPinned)
                        {
                            Mem::Write<std::uint8_t>(addr, kPinned);
                            NWARN("sv_cheats leaked (was %u) - thread without guard, restored", cur);
                        }

                        ArmAllThreads(addr);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
    }

    void Start()
    {
        if (g_running.exchange(true))
            return;

        g_veh = AddVectoredExceptionHandler(1, GuardHandler);
        g_worker = std::thread(Worker);
    }

    void Stop()
    {
        if (!g_running.exchange(false))
            return;

        if (g_worker.joinable())
            g_worker.join();

        if (g_veh)
        {
            RemoveVectoredExceptionHandler(g_veh);
            g_veh = nullptr;
        }
    }
}