#pragma once

namespace Kill
{
    // Starts a background thread that polls m_iKills on the local controller
    // and triggers Audio::PlayKillSound(option) whenever the counter goes up.
    // Safe to call before the player connects — watcher waits patiently.
    void Start( int soundOption );
}
