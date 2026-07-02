#include "steam_helper.h"
#include "../file_utils.h"
#include <windows.h>
#include <tlhelp32.h>
#include <sstream>
#include <algorithm>

namespace SteamHelper {

    std::string GetSteamInstallPath()
    {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        {
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
                if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
                    return "";
        }

        char buffer[MAX_PATH] = { 0 };
        DWORD size = sizeof(buffer);
        DWORD type;

        LONG res = RegQueryValueExA(hKey, "SteamPath", nullptr, &type, (LPBYTE)buffer, &size);
        if (res != ERROR_SUCCESS)
        {
            size = sizeof(buffer);
            res = RegQueryValueExA(hKey, "InstallPath", nullptr, &type, (LPBYTE)buffer, &size);
        }
        RegCloseKey(hKey);

        if (res != ERROR_SUCCESS) return "";

        std::string path = buffer;
        std::replace(path.begin(), path.end(), '/', '\\');
        return path;
    }

    // valve data file parsing

    struct VdfNode {
        std::string key;
        std::string value;
        std::vector<VdfNode> children;
        bool isSection = false;
    };

    static void SkipWhitespaceAndComments(const std::string& s, size_t& i)
    {
        while (i < s.size())
        {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                i++;
            }
            else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/')
            {
                while (i < s.size() && s[i] != '\n') i++;
            }
            else break;
        }
    }

    static std::string ParseQuotedString(const std::string& s, size_t& i)
    {
        if (i >= s.size() || s[i] != '"') return "";
        i++;
        std::string result;
        while (i < s.size() && s[i] != '"')
        {
            if (s[i] == '\\' && i + 1 < s.size())
            {
                char next = s[i + 1];
                if (next == 'n') result += '\n';
                else if (next == 't') result += '\t';
                else if (next == '\\') result += '\\';
                else if (next == '"') result += '"';
                else result += next;
                i += 2;
            }
            else
            {
                result += s[i++];
            }
        }
        if (i < s.size()) i++;
        return result;
    }

    static VdfNode ParseVdfNode(const std::string& s, size_t& i)
    {
        VdfNode node;
        SkipWhitespaceAndComments(s, i);
        node.key = ParseQuotedString(s, i);
        SkipWhitespaceAndComments(s, i);

        if (i < s.size() && s[i] == '{')
        {
            node.isSection = true;
            i++;
            SkipWhitespaceAndComments(s, i);
            while (i < s.size() && s[i] != '}')
            {
                node.children.push_back(ParseVdfNode(s, i));
                SkipWhitespaceAndComments(s, i);
            }
            if (i < s.size()) i++;
        }
        else if (i < s.size() && s[i] == '"')
        {
            node.value = ParseQuotedString(s, i);
        }
        return node;
    }

    static VdfNode ParseVdf(const std::string& content)
    {
        size_t i = 0;
        return ParseVdfNode(content, i);
    }

    std::vector<SteamAccount> GetAllAccounts()
    {
        std::vector<SteamAccount> accounts;

        std::string steamPath = GetSteamInstallPath();
        if (steamPath.empty()) return accounts;

        std::string loginusersPath = CombinePath(steamPath, "config\\loginusers.vdf");
        if (!FileExists(loginusersPath)) return accounts;

        std::string content;
        if (!ReadFileToString(loginusersPath, content)) return accounts;

        VdfNode root = ParseVdf(content);

        for (const auto& userNode : root.children)
        {
            if (!userNode.isSection) continue;

            SteamAccount acc;

            try {
                acc.SetSteamID64(std::stoull(userNode.key));
            }
            catch (...) {
                continue;
            }

            for (const auto& field : userNode.children)
            {
                if (field.isSection) continue;

                if (field.key == "AccountName")
                    acc.SetAccountName(field.value);
                else if (field.key == "PersonaName")
                    acc.SetPersonaName(field.value);
                else if (field.key == "RememberPassword")
                    acc.SetRememberPassword(field.value == "1");
                else if (field.key == "MostRecent" || field.key == "mostrecent")
                    acc.SetMostRecent(field.value == "1");
                else if (field.key == "Timestamp")
                {
                    try { acc.SetTimestamp(std::stoull(field.value)); }
                    catch (...) {}
                }
            }

            accounts.push_back(acc);
        }

        return accounts;
    }

    std::string GetAutoLoginUser()
    {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            return "";

        char buffer[256] = { 0 };
        DWORD size = sizeof(buffer);
        DWORD type;

        LONG res = RegQueryValueExA(hKey, "AutoLoginUser", nullptr, &type, (LPBYTE)buffer, &size);
        RegCloseKey(hKey);

        return (res == ERROR_SUCCESS) ? std::string(buffer) : "";
    }

    std::string GetCurrentLoggedInUser()
    {
        if (!IsSteamRunning()) return "";

        auto accounts = GetAllAccounts();
        for (const auto& acc : accounts)
        {
            if (acc.IsMostRecent())
                return acc.GetAccountName();
        }
        return "";
    }

    bool SetAutoLoginUser(const std::string& accountName)
    {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
            return false;

        LONG res = RegSetValueExA(hKey, "AutoLoginUser", 0, REG_SZ,
            (const BYTE*)accountName.c_str(), (DWORD)(accountName.size() + 1));

        DWORD one = 1;
        RegSetValueExA(hKey, "RememberPassword", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));

        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    bool IsSteamRunning()
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe = { sizeof(pe) };
        bool found = false;

        if (Process32FirstW(snap, &pe))
        {
            do {
                if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0)
                {
                    found = true;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return found;
    }

    bool IsSteamReady()
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        bool has_steam = false, has_webhelper = false;
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe))
        {
            do {
                if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) has_steam = true;
                else if (_wcsicmp(pe.szExeFile, L"steamwebhelper.exe") == 0) has_webhelper = true;
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);

        return has_steam && has_webhelper;
    }

    bool ShutdownSteam()
    {
        std::string steamPath = GetSteamInstallPath();
        if (steamPath.empty()) return false;

        std::string steamExe = CombinePath(steamPath, "steam.exe");

        // Steam поддерживает флаг -shutdown для мягкого закрытия
        std::string cmd = "\"" + steamExe + "\" -shutdown";

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

        if (!ok) return false;

        // Ждём завершения запуска команды (не самого Steam!)
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // Даём Steam время закрыться
        for (int i = 0; i < 30 && IsSteamRunning(); i++)
            Sleep(200);

        return !IsSteamRunning();
    }

    bool LaunchSteamAs(const SteamAccount& account)
    {
        std::string steamPath = GetSteamInstallPath();
        if (steamPath.empty()) return false;

        std::string steamExe = CombinePath(steamPath, "steam.exe");
        if (!FileExists(steamExe)) return false;

        SetAutoLoginUser(account.GetAccountName());

        std::string cmd = "\"" + steamExe + "\" -login " + account.GetAccountName();

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
            0, nullptr, nullptr, &si, &pi);

        if (ok)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }
        return false;
    }
}