@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set START=%TIME%

if not exist "%ROOT%\build\CMakeCache.txt" (
    echo Creating build directory...

    powershell -Command ^
        "cmake -S '%ROOT%' -B '%ROOT%\build' -G 'Visual Studio 18' -A x64"

    if errorlevel 1 exit /b 1
)

if not exist "%ROOT%\Test" (

    mkdir "%ROOT%\Test"

    powershell -Command ^
        "Copy-Item '%ROOT%\Resources' '%ROOT%\Test' -Recurse"

)

powershell -Command ^
"Remove-Item '%ROOT%\build\Infections' -Recurse -Force -ErrorAction SilentlyContinue; ^
 Remove-Item '%ROOT%\build\Loader' -Recurse -Force -ErrorAction SilentlyContinue; ^
 Remove-Item '%ROOT%\build\NemesisLoader' -Recurse -Force -ErrorAction SilentlyContinue; ^
 Remove-Item '%ROOT%\NemesisDLC\build\Release' -Recurse -Force -ErrorAction SilentlyContinue;"

powershell -Command ^
"cmake -S D:\Nemesis\NemesisDLC -B D:\Nemesis\NemesisDLC\build -A x64; ^
 cmake --build D:\Nemesis\NemesisDLC\build --config Release; ^
 cmake --build '%ROOT%\build' --config Release --target Nemesis NemesisLoader Loader;"

if errorlevel 1 (
powershell -Command ^
	"Error"
)

copy /Y "%ROOT%\build\Infections\src\Nemesis\Release\Nemesis.lib" "%ROOT%\Test\" >nul
copy /Y "%ROOT%\build\Loader\Release\Loader.exe" "%ROOT%\Test\" >nul
copy /Y "%ROOT%\build\NemesisLoader\Release\NemesisLoader.dll" "%ROOT%\Test\" >nul
copy /Y "%ROOT%\NemesisDLC\build\Release\NemesisLoader.dll" "%ROOT%\Test\" >nul

powershell -Command ^
"Copy-Item '%ROOT%\Resources' '%ROOT%\Test' -Recurse -Force"

powershell -Command ^
"$start=[datetime]::Parse('%START%'); ^
 $elapsed=(Get-Date)-$start; ^
 Write-Host ('Build successful in {0}ms.' -f [int]$elapsed.TotalMilliseconds)"

pause