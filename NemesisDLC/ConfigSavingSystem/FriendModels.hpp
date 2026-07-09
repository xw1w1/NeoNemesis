#pragma once

#include <cstdint>

namespace Nemesis::FriendModels
{
    // Returns the custom model path for a given account SteamID if it matches a
    // loaded friend config; otherwise nullptr. `token` receives the model's
    // distinctive substring for the "already applied" check.
    const char* MatchSteamID(std::uint64_t steamID, const char*& token);

    // /cstatusMod  - writes my SteamID + currently selected model to a .json config.
    bool SaveMyConfig(std::uintptr_t base);

    // /cstatusLoad <path> - reads a friend config and remembers it for this session.
    bool LoadConfig(const char* path);

    // Clears all loaded friend configs (each launch waits for a fresh /cstatusLoad).
    void Clear();

    int LoadedCount();
}
