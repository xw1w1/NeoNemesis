#pragma once
#include <string>

namespace Cfg
{
    struct Config
    {
        bool logging_enabled    = false; // NemesisLoader.log on/off
        bool sound_enabled      = true;  // play our kill sound at all
        bool sound_hook_enabled = false; // hook soundsystem.dll to suppress orig (disabled by default = SAFE)
        int  sound_option       = 1;     // 1 = option 1, 2 = option 2
    };

    // Loads %APPDATA%\Nemesis\Nemesis.json. Creates a default file if missing.
    // Always returns a Config (defaults on any failure).
    Config Load();

    // Absolute path to the resolved config file (for logging/debug).
    std::wstring GetConfigPath();
}
