#pragma once

#include "LocalHookBase.h"

namespace nemesis
{
    class NEMESIS_API NoClass { };

    template<typename Fn, class C>
    struct HookHandler;
}

#include "HookHandlerCdecl.h"

#ifndef USE64
#include "HookHandlerStdcall.h"
#include "HookHandlerThiscall.h"
#include "HookHandlerFastcall.h"
#endif