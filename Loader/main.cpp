#include <Process/Process.h>
#include <cstdio>

using namespace nemesis;

int main()
{
    setvbuf( stdout, nullptr, _IONBF, 0 );

    Process proc;

    // 1) Запускаем целевой процесс БЕЗ suspend, чтобы у него были живые потоки для hijack
    NTSTATUS status = proc.CreateAndAttach( L"D:\\SteamLibrary\\steamapps\\common\\Counter-Strike Global Offensive\\game\\bin\\win64\\cs2.exe", false /* not suspended */ );
    if (!NT_SUCCESS( status ))
    {
        printf( "CreateAndAttach failed, status = 0x%X\n", status );
        return 1;
    }

    printf( "Target started, PID = %lu\n", proc.pid() );

    // 2) Даём процессу прогреться (запустить main, войти в Sleep)
    Sleep( 500 );

    printf( "Calling MapImage with NoThreads (Thread Hijack)...\n" );

    // 3) Manual Map + Thread Hijack (NoThreads = не создавать новый поток, угнать существующий)
    auto result = proc.mmap().MapImage( L"NemesisLoader.dll", NoThreads );
    if (!result.success())
    {
        printf( "MapImage failed, status = 0x%X\n", result.status );
        proc.Terminate( 1 );
        return 1;
    }

    printf( "NemesisLoader.dll mapped successfully\n" );

    // 4) Дать DLL отработать DllMain и открыть cmd
    printf( "DLL injected. Closing target in 5 seconds (cmd останется открытым).\n" );
    Sleep( 5000 );
    proc.Terminate( 0 );
    return 0;
}
