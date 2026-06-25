#pragma once
#include <windows.h>

namespace Audio
{
    // Must be called once from DllMain with the DLL's HMODULE so the player
    // can locate embedded RCDATA resources.
    void Init( HMODULE selfModule );

    // Play the configured kill sound (option = 1 or 2).
    // Non-blocking: returns immediately, sound plays on a system thread.
    void PlayKillSound( int option );
}
