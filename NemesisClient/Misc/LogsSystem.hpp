#pragma once

namespace Nemesis::Logs
{
    void SetEnabled(bool enabled);
    bool IsEnabled();
    void Write(const char* level, const char* fmt, ...);
}

#define NLOG(...)  ::Nemesis::Logs::Write("INFO", __VA_ARGS__)
#define NWARN(...) ::Nemesis::Logs::Write("WARN", __VA_ARGS__)
#define NERR(...)  ::Nemesis::Logs::Write("ERR",  __VA_ARGS__)
