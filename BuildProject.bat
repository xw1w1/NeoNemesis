@echo off
setlocal enabledelayedexpansion

:: Force UTF-8 output in the console so non-ASCII chars don't break
chcp 65001 >nul 2>&1

:: =============================================================
::  BuildProject.bat
::  Root: <...>/NeoNemesis
::  - Runs CMake configure into build\
::  - Builds targets NemesisLoader and NemesisClient (Release, x64)
::  - Copies NemesisLoader.exe and NemesisClient.dll into .\test
::  - Pauses on error AND on success so you can read the output
:: =============================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "TEST_DIR=%ROOT%\test"

echo ===========================================================
echo   NeoNemesis build script
echo   Root      : %ROOT%
echo   Build dir : %BUILD_DIR%
echo   Test dir  : %TEST_DIR%
echo ===========================================================
echo.

:: --- 0. Check for cmake ---
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH. Install CMake and add it to PATH.
    goto :error
)

:: --- 1. Create build/ and test/ folders ---
if not exist "%BUILD_DIR%" (
    echo [INFO] Creating build directory...
    mkdir "%BUILD_DIR%"
)
if not exist "%TEST_DIR%" (
    echo [INFO] Creating test directory...
    mkdir "%TEST_DIR%"
)
echo.

:: --- 2. CMake configure ---
echo [STEP 1/3] Configuring project (Release, x64)...
pushd "%BUILD_DIR%"
cmake .. -DCMAKE_BUILD_TYPE=Release -A x64
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    popd
    goto :error
)
popd
echo.

:: --- 3. Build ---
echo [STEP 2/3] Building NemesisLoader...
cmake --build "%BUILD_DIR%" --config Release --target NemesisLoader
if errorlevel 1 (
    echo [ERROR] Build failed for target: NemesisLoader
    goto :error
)

echo.
echo [STEP 2/3] Building NemesisClient...
cmake --build "%BUILD_DIR%" --config Release --target NemesisClient
if errorlevel 1 (
    echo [ERROR] Build failed for target: NemesisClient
    goto :error
)
echo.

:: --- 4. Copy artifacts to test\ ---
echo [STEP 3/3] Copying artifacts to "%TEST_DIR%"...

set "LOADER_EXE_FOUND="
set "CLIENT_DLL_FOUND="

:: Common output locations (MSVC puts them under <target>/Release/,
:: MinGW/Ninja put them directly under <target>/, etc.)
for %%F in (
    "%BUILD_DIR%\NemesisLoader\Release\NemesisLoader.exe"
    "%BUILD_DIR%\NemesisLoader\NemesisLoader.exe"
    "%BUILD_DIR%\Release\NemesisLoader.exe"
    "%BUILD_DIR%\bin\Release\NemesisLoader.exe"
    "%BUILD_DIR%\bin\NemesisLoader.exe"
) do (
    if exist "%%~F" (
        copy /Y "%%~F" "%TEST_DIR%\NemesisLoader.exe" >nul
        echo  [OK] NemesisLoader.exe : %%~F -^> %TEST_DIR%\
        set "LOADER_EXE_FOUND=1"
    )
)

for %%F in (
    "%BUILD_DIR%\NemesisClient\Release\NemesisClient.dll"
    "%BUILD_DIR%\NemesisClient\NemesisClient.dll"
    "%BUILD_DIR%\Release\NemesisClient.dll"
    "%BUILD_DIR%\bin\Release\NemesisClient.dll"
    "%BUILD_DIR%\bin\NemesisClient.dll"
) do (
    if exist "%%~F" (
        copy /Y "%%~F" "%TEST_DIR%\NemesisClient.dll" >nul
        echo  [OK] NemesisClient.dll : %%~F -^> %TEST_DIR%\
        set "CLIENT_DLL_FOUND=1"
    )
)

echo.
if not defined LOADER_EXE_FOUND (
    echo [WARN] NemesisLoader.exe was not found in the usual output paths.
    echo         Check your CMAKE_RUNTIME_OUTPUT_DIRECTORY and add the path
    echo         to the search list above.
)
if not defined CLIENT_DLL_FOUND (
    echo [WARN] NemesisClient.dll was not found in the usual output paths.
    echo         Check your CMAKE_RUNTIME_OUTPUT_DIRECTORY and add the path
    echo         to the search list above.
)

echo.
echo ===========================================================
echo   Build finished successfully.
echo   %TEST_DIR%\
echo ===========================================================
echo.
echo Press any key to exit...
pause >nul
endlocal
exit /b 0

:error
echo.
echo ===========================================================
echo   AN ERROR OCCURRED! Build aborted.
echo   Read the error message above for details.
echo ===========================================================
echo.
echo Press any key to exit...
pause >nul
endlocal
exit /b 1
