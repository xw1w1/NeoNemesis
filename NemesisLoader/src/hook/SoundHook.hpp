#pragma once

namespace SoundHook
{
    // Installs an inline hook on CSoundOpSystem::StartSoundEvent.
    // - Logs every event name to D:\Nemesis\Ready\soundnames.log
    // - Suppresses original sound for events matching the kill-sound pattern,
    //   and triggers Audio::PlayKillSound(option) instead.
    //
    // Returns true on success.
    bool Install( int soundOption, bool suppressOriginal );

    // Reverts the hook (restores the original prologue bytes).
    void Uninstall();
}
