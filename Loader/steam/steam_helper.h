#pragma once

#include <string>
#include <vector>
#include "steamacc.h"

namespace SteamHelper {
    std::string GetSteamInstallPath();

    std::vector<SteamAccount> GetAllAccounts();

    std::string GetAutoLoginUser();

    std::string GetCurrentLoggedInUser();

    bool SetAutoLoginUser(const std::string& accountName);

    bool LaunchSteamAs(const SteamAccount& account);

    bool IsSteamRunning();

    bool IsSteamReady();

    bool ShutdownSteam();
}