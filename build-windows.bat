@echo off
REM Copyright (c) 2026 Jimena Neumann
REM SPDX-License-Identifier: BSD-3-Clause
REM B3DV Windows Build Script with automatic raylib download
REM Usage: build-windows.bat [--run] [--local-raylib]

setlocal enabledelayedexpansion

chcp 65001 >nul 2>&1

set YELLOW=[YELLOW]
set GREEN=[GREEN]
set RED=[RED]
set BLUE=[BLUE]
set NC=[NC]

echo.
echo %YELLOW%=== B3DV Windows Build System ===%NC%
echo.

set RUN_AFTER_BUILD=0
set FORCE_LOCAL_RAYLIB=0
set LOCAL_RAYLIB_DIR=external\raylib

REM Parse arguments
:parse_args
if "%1"=="" goto :check_deps
if "%1"=="--run" set RUN_AFTER_BUILD=1
if "%1"=="--local-raylib" set FORCE_LOCAL_RAYLIB=1
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

REM Check for raylib
set RAYLIB_FOUND=0
set USE_LOCAL_RAYLIB=0

if %FORCE_LOCAL_RAYLIB%==1 (
    echo %YELLOW%Using local raylib (forced)%NC%
    set USE_LOCAL_RAYLIB=1
    goto :check_local_raylib
)

REM Try to find system raylib
pkg-config --exists raylib >nul 2>nul
if not errorlevel 1 (
    echo %GREEN%✓ System raylib found%NC%
    set RAYLIB_FOUND=1
) else (
    REM Check MinGW include directory
    if exist "C:\mingw64\include\raylib.h" (
        echo %GREEN%✓ System raylib found%NC%
        set RAYLIB_FOUND=1
    )
    if exist "%MINGW_PREFIX%\include\raylib.h" (
        echo %GREEN%✓ System raylib found%NC%
        set RAYLIB_FOUND=1
    )
)

if %RAYLIB_FOUND%==0 (
    echo %YELLOW%⚠ System raylib not found%NC%
    echo %BLUE%→ Will build raylib locally%NC%
    set USE_LOCAL_RAYLIB=1
)

echo.

:check_local_raylib
if %USE_LOCAL_RAYLIB%==1 (
    if not exist "%LOCAL_RAYLIB_DIR%\src\libraylib.a" (
        call :build_local_raylib
        if errorlevel 1 (
            echo %RED%✗ Failed to build local raylib%NC%
            echo.
            echo %YELLOW%Try installing raylib system-wide:%NC%
            echo   - MSYS2: pacman -S mingw-w64-x86_64-raylib
            echo   - Or download from: https://github.com/raysan5/raylib/releases
            echo.
            pause
            exit /b 1
        )
    ) else (
        echo %GREEN%✓ Local raylib already built%NC%
    )
)

:clean_build
echo.
echo %YELLOW%Cleaning...%NC%
if exist world.exe del world.exe
if exist *.o del *.o 2>nul
echo %GREEN%✓ Cleaned%NC%
echo.

:build
echo %YELLOW%Building B3DV for Windows...%NC%

if %USE_LOCAL_RAYLIB%==1 (
    echo %BLUE%Using local raylib%NC%
    gcc src\main.c src\world_generation.c src\vec_math.c src\rendering.c src\utils.c ^
        -o world.exe ^
        -Wall -Wextra -O2 ^
        -I%LOCAL_RAYLIB_DIR%\src ^
        -L%LOCAL_RAYLIB_DIR%\src ^
        -l:libraylib.a ^
        -lopengl32 -lgdi32 -lwinmm -lm
) else (
    echo %BLUE%Using system raylib%NC%
    gcc src\main.c src\world_generation.c src\vec_math.c src\rendering.c src\utils.c ^
        -o world.exe ^
        -Wall -Wextra -O2 ^
        -lraylib -lopengl32 -lgdi32 -lwinmm -lm
)

if errorlevel 1 (
    echo %RED%✗ Build failed%NC%
    pause
    exit /b 1
)

echo %GREEN%✓ Build successful!%NC%
echo %GREEN%Output: world.exe%NC%
echo.

:run_check
if %RUN_AFTER_BUILD%==1 (
    echo %YELLOW%Launching B3DV...%NC%
    echo.
    start "" world.exe
) else (
    echo %GREEN%✓ Executable ready: world.exe%NC%
    echo.
    echo %YELLOW%Usage:%NC%
    echo   world.exe                      :: Run directly
    echo   build-windows.bat --run        :: Build and run
    echo   build-windows.bat --local-raylib :: Force local raylib
    echo.
)

echo.
pause
exit /b 0

REM ===== Subroutine: Build local raylib =====
:build_local_raylib
echo %YELLOW%Building raylib locally...%NC%

if not exist external mkdir external

if not exist "%LOCAL_RAYLIB_DIR%" (
    echo %BLUE%Downloading raylib...%NC%

    REM Try git first
    where git >nul 2>nul
    if not errorlevel 1 (
        git clone --depth 1 --branch 5.0 https://github.com/raysan5/raylib.git %LOCAL_RAYLIB_DIR%
        if errorlevel 1 (
            echo %RED%Git clone failed%NC%
            goto :download_fallback
        )
        goto :compile_raylib
    )

    :download_fallback
    REM Try curl
    where curl >nul 2>nul
    if not errorlevel 1 (
        echo %BLUE%Downloading via curl...%NC%
        curl -L -o raylib.zip https://github.com/raysan5/raylib/archive/refs/tags/5.0.zip
        if errorlevel 1 (
            echo %RED%Download failed%NC%
            exit /b 1
        )

        REM Extract (requires tar or 7z)
        where tar >nul 2>nul
        if not errorlevel 1 (
            tar -xf raylib.zip -C external
            move external\raylib-5.0 %LOCAL_RAYLIB_DIR%
            del raylib.zip
            goto :compile_raylib
        )

        echo %RED%No extraction tool found (need tar or 7z)%NC%
        exit /b 1
    )

    echo %RED%No download tool found (need git or curl)%NC%
    exit /b 1
)

:compile_raylib
echo %BLUE%Compiling raylib...%NC%
cd %LOCAL_RAYLIB_DIR%\src
make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
if errorlevel 1 (
    cd ..\..
    echo %RED%Raylib compilation failed%NC%
    exit /b 1
)
cd ..\..

echo %GREEN%✓ Local raylib built successfully%NC%
exit /b 0