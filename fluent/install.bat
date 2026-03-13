@echo off
setlocal enabledelayedexpansion

echo.
echo  FLUENT Language Installer
echo  -------------------------
echo.

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] g++ not found. Please install MinGW-w64 and add it to PATH.
    echo     Download: https://winlibs.com/
    pause
    exit /b 1
)

echo [1/2] Compiling FLUENT...

if not exist "%SCRIPT_DIR%\build" mkdir "%SCRIPT_DIR%\build"

g++ -std=c++17 -O2 -I"%SCRIPT_DIR%\include" ^
    "%SCRIPT_DIR%\src\main.cpp" ^
    "%SCRIPT_DIR%\src\value.cpp" ^
    "%SCRIPT_DIR%\src\core.cpp" ^
    "%SCRIPT_DIR%\src\exec.cpp" ^
    "%SCRIPT_DIR%\src\vars.cpp" ^
    "%SCRIPT_DIR%\src\control.cpp" ^
    "%SCRIPT_DIR%\src\io.cpp" ^
    "%SCRIPT_DIR%\src\system.cpp" ^
    -o "%SCRIPT_DIR%\build\fluent.exe" ^
    -lpthread -lws2_32 -static-libgcc -static-libstdc++

if %errorlevel% neq 0 (
    echo.
    echo [!] Compilation failed.
    pause
    exit /b 1
)

echo [2/2] Installing FLUENT...


copy /Y "%SCRIPT_DIR%\build\fluent.exe" "C:\Windows\System32\fluent.exe" >nul 2>&1
if %errorlevel% equ 0 (
    echo    Installed to C:\Windows\System32\fluent.exe
    echo    fluent is now available in ALL terminals immediately.
) else (
    
    copy /Y "%SCRIPT_DIR%\build\fluent.exe" "%SCRIPT_DIR%\fluent.exe" >nul
    echo    Installed to: %SCRIPT_DIR%\fluent.exe

    for /f "tokens=2,*" %%a in ('reg query "HKCU\Environment" /v PATH 2^>nul') do set "CURRENT_PATH=%%b"
    echo !CURRENT_PATH! | find /i "%SCRIPT_DIR%" >nul
    if !errorlevel! neq 0 (
        setx PATH "!CURRENT_PATH!;%SCRIPT_DIR%" >nul
        echo    Added %SCRIPT_DIR% to PATH.
        echo    NOTE: Open a NEW terminal for PATH to take effect.
    ) else (
        echo    PATH already contains %SCRIPT_DIR%.
        echo    NOTE: Open a NEW terminal if fluent is still not found.
    )
)

echo.
echo  Done! Run your scripts with:
echo    fluent script.fluent
echo.
pause
