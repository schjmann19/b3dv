@echo off
REM Copyright (c) 2026 Jimena Neumann
REM SPDX-License-Identifier: BSD-3-Clause
REM B3DV Windows Build Script
REM Usage: build-windows.bat [--run]

setlocal enabledelayedexpansion

set YELLOW=[YELLOW]
set GREEN=[GREEN]
set RED=[RED]
set BLUE=[BLUE]
set NC=[NC]

echo.
echo %YELLOW%=== B3DV Windows Build System ===%NC%
echo.

set RUN_AFTER_BUILD=0
set LOCAL_RAYLIB_DIR=external\raylib

REM Parse arguments
:parse_args
if "%1"=="" goto :check_deps
if "%1"=="--run" set RUN_AFTER_BUILD=1
shift
goto :parse_args

:check_deps
echo %YELLOW%Checking dependencies...%NC%

REM Check for gcc
where gcc >nul 2>nul
if errorlevel 1 (
    echo %RED%ERROR: gcc not found in PATH%NC%
    echo.
    echo Install MinGW-w64: https://www.mingw-w64.org/
    echo Or use MSYS2: https://www.msys2.org/
    echo.
    pause
    exit /b 1
)

REM Check for make
where make >nul 2>nul
if errorlevel 1 (
    echo %RED%ERROR: make not found in PATH%NC%
    echo.
    echo Install via MinGW-w64 or MSYS2
    echo.
    pause
    exit /b 1
)

echo %GREEN%✓ Build tools found%NC%
echo.

REM Build raylib if needed
if not exist "%LOCAL_RAYLIB_DIR%\src\libraylib.a" (
    echo %YELLOW%Building raylib...%NC%
    cd %LOCAL_RAYLIB_DIR%\src
    make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
    if errorlevel 1 (
        cd ..\..
        echo %RED%Raylib compilation failed%NC%
        pause
        exit /b 1
    )
    cd ..\..
    echo %GREEN%✓ Raylib built%NC%
    echo.
) else (
    echo %GREEN%✓ Raylib already built%NC%
    echo.
)

:clean_build
echo %YELLOW%Cleaning...%NC%
if exist b3dv.exe del b3dv.exe
if exist *.o del *.o 2>nul
echo %GREEN%✓ Cleaned%NC%
echo.

:build
echo %YELLOW%Building B3DV for Windows...%NC%
gcc src\main.c src\world_generation.c src\vec_math.c src\rendering.c src\utils.c ^
    -o b3dv.exe ^
    -Wall -Wextra -O2 ^
    -I%LOCAL_RAYLIB_DIR%\src ^
    -L%LOCAL_RAYLIB_DIR%\src ^
    -l:libraylib.a ^
    -lopengl32 -lgdi32 -lwinmm -lm

if errorlevel 1 (
    echo %RED%✗ Build failed%NC%
    pause
    exit /b 1
)

echo %GREEN%✓ Build successful!%NC%
echo %GREEN%Output: b3dv.exe%NC%
echo.

:run_check
if %RUN_AFTER_BUILD%==1 (
    echo %YELLOW%Launching B3DV...%NC%
    echo.
    start "" b3dv.exe
) else (
    echo %GREEN%✓ Executable ready: b3dv.exe%NC%
    echo.
    echo %YELLOW%Usage:%NC%
    echo   b3dv.exe            :: Run directly
    echo   build-windows.bat --run :: Build and run
    echo.
)

echo.
pause
exit /b 0