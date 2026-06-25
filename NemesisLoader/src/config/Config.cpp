#include "Config.hpp"

#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Cfg
{
    static const wchar_t* kDefaultJson =
        L"{\r\n"
        L"  \"logging_enabled\": false,\r\n"
        L"  \"sound_enabled\": true,\r\n"
        L"  \"sound_hook_enabled\": false,\r\n"
        L"  \"sound_option\": 1,\r\n"
        L"  \"_comment_sound_option\": \"1 - classic, 2 - modern\",\r\n"
        L"  \"_comment_sound_hook\": \"true = suppress game kill sound via hook (RISK if CS2 was patched). false = only play our sound on top.\"\r\n"
        L"}\r\n";

    static std::wstring AppDataDir()
    {
        wchar_t buf[MAX_PATH] = {};
        if (FAILED( SHGetFolderPathW( nullptr, CSIDL_APPDATA, nullptr, 0, buf ) ))
            return L"";
        std::wstring p = buf;
        p += L"\\Nemesis";
        return p;
    }

    std::wstring GetConfigPath()
    {
        auto dir = AppDataDir();
        if (dir.empty()) return L"";
        return dir + L"\\Nemesis.json";
    }

    static void EnsureDir( const std::wstring& dir )
    {
        if (dir.empty()) return;
        CreateDirectoryW( dir.c_str(), nullptr );
    }

    static bool ReadAll( const std::wstring& path, std::string& out )
    {
        HANDLE h = CreateFileW( path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
        if (h == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER sz{};
        GetFileSizeEx( h, &sz );
        if (sz.QuadPart > (1 << 20)) { CloseHandle( h ); return false; } // 1 MB cap
        out.resize( (size_t)sz.QuadPart );
        DWORD read = 0;
        BOOL ok = ReadFile( h, out.data(), (DWORD)out.size(), &read, nullptr );
        CloseHandle( h );
        return ok && read == out.size();
    }

    static void WriteDefault( const std::wstring& path )
    {
        HANDLE h = CreateFileW( path.c_str(), GENERIC_WRITE, 0,
                                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
        if (h == INVALID_HANDLE_VALUE) return;
        // Write as UTF-8 (the literal is ASCII-only, so direct cast is safe).
        std::string utf8;
        for (const wchar_t* p = kDefaultJson; *p; ++p) utf8.push_back( (char)*p );
        DWORD w = 0;
        WriteFile( h, utf8.data(), (DWORD)utf8.size(), &w, nullptr );
        CloseHandle( h );
    }

    // --- Tiny JSON parser (only what we need: top-level object with bool/int) ---
    static void Skip( const std::string& s, size_t& i )
    {
        while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='\r'||s[i]=='\n'||s[i]==','||s[i]=='{'||s[i]=='}')) ++i;
    }

    static bool FindKey( const std::string& s, const char* key, size_t& valStart )
    {
        std::string needle = std::string( "\"" ) + key + "\"";
        size_t pos = s.find( needle );
        if (pos == std::string::npos) return false;
        pos += needle.size();
        while (pos < s.size() && (s[pos]==' '||s[pos]==':'||s[pos]=='\t')) ++pos;
        valStart = pos;
        return true;
    }

    static bool ParseBool( const std::string& s, const char* key, bool& out )
    {
        size_t p;
        if (!FindKey( s, key, p )) return false;
        if (s.compare( p, 4, "true" ) == 0) { out = true;  return true; }
        if (s.compare( p, 5, "false" ) == 0) { out = false; return true; }
        return false;
    }

    static bool ParseInt( const std::string& s, const char* key, int& out )
    {
        size_t p;
        if (!FindKey( s, key, p )) return false;
        int sign = 1;
        if (p < s.size() && s[p] == '-') { sign = -1; ++p; }
        if (p >= s.size() || s[p] < '0' || s[p] > '9') return false;
        int v = 0;
        while (p < s.size() && s[p] >= '0' && s[p] <= '9') { v = v*10 + (s[p]-'0'); ++p; }
        out = v * sign;
        return true;
    }

    Config Load()
    {
        Config c{};
        auto dir  = AppDataDir();
        auto path = GetConfigPath();
        if (path.empty()) return c;

        EnsureDir( dir );

        std::string body;
        if (!ReadAll( path, body ))
        {
            WriteDefault( path );
            if (!ReadAll( path, body )) return c;
        }

        ParseBool( body, "logging_enabled",    c.logging_enabled    );
        ParseBool( body, "sound_enabled",      c.sound_enabled      );
        ParseBool( body, "sound_hook_enabled", c.sound_hook_enabled );
        ParseInt ( body, "sound_option",       c.sound_option       );
        if (c.sound_option != 1 && c.sound_option != 2) c.sound_option = 1;
        return c;
    }
}
