@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/k','\"%~f0\"' -Verb RunAs"
    exit /b
)

set START=%TIME%

if not exist "%ROOT%\build" (
    echo Creating build directory...

    powershell -Command ^
        "cmake -S '%ROOT%' -B '%ROOT%\build' -G 'Visual Studio 18' -A x64"

    if errorlevel 1 (
        echo.
        echo [ERROR] CMake configuration failed!
        echo.
        pause
        exit /b 1
    )
)

if not exist "%ROOT%\Test" (
    mkdir "%ROOT%\Test"
    powershell -Command ^
        "Copy-Item '%ROOT%\Resources' '%ROOT%\Test' -Recurse"
)

powershell -Command ^
    "Remove-Item '%ROOT%\build\Loader' -Recurse -Force -ErrorAction SilentlyContinue;"

echo.
echo === Building target Loader ===
echo.

powershell -Command ^
    "cmake --build '%ROOT%\build' --config Release --target Loader"

if errorlevel 1 (
    echo.
    echo ============================================
    echo [ERROR] Build failed! See errors above.
    echo ============================================
    echo.
    pause
    exit /b 1
)

copy /Y "%ROOT%\build\Loader\Release\Loader.exe" "%ROOT%\Test\" >nul

if errorlevel 1 (
    echo.
    echo [ERROR] Failed to copy Loader.exe to Test directory!
    echo.
    pause
    exit /b 1
)

echo.
powershell -Command ^
    "$start=[datetime]::Parse('%START%'); ^
     $elapsed=(Get-Date)-$start; ^
     Write-Host ('Build successful in {0}ms.' -f [int]$elapsed.TotalMilliseconds) -ForegroundColor Green"

echo.
pause