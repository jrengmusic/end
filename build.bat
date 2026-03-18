@echo off
:: build.bat - Bootstrap build script for END (Windows)
:: Sets up MSVC environment then runs CMake + Ninja
:: Produces PDB debug symbols for whatdbg (dbgeng.dll-based DAP adapter)
::
:: Usage:
::   build.bat             - configure + build (Debug)
::   build.bat Release     - configure + build (Release)
::   build.bat clean       - delete Builds/Ninja and reconfigure (Debug)
::   build.bat clean Release - delete Builds/Ninja and reconfigure (Release)

:: Guard against re-entry (vcvarsall.bat can cause cmd.exe to re-enter this script)
if defined _END_BUILD_RUNNING exit /b 0
set _END_BUILD_RUNNING=1

setlocal enabledelayedexpansion

:: Parse args: "clean" triggers a wipe, config is Debug/Release
set CLEAN=0
set CONFIG=

for %%A in (%*) do (
    if /i "%%A"=="clean" (
        set CLEAN=1
    ) else (
        set CONFIG=%%A
    )
)

if "!CONFIG!"=="" set CONFIG=Debug

if !CLEAN!==1 (
    echo Cleaning Builds/Ninja...
    if exist "Builds\Ninja" rmdir /s /q "Builds\Ninja"
)

:: Find vcvarsall.bat via vswhere
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist !VSWHERE! (
    echo ERROR: vswhere.exe not found. Is Visual Studio installed?
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`!VSWHERE! -latest -property installationPath`) do (
    set VS_PATH=%%i
)

set VCVARSALL="!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat"
if not exist !VCVARSALL! (
    echo ERROR: vcvarsall.bat not found at !VCVARSALL!
    exit /b 1
)

echo Setting up MSVC x64 environment...
call !VCVARSALL! x64

:: Use cl.exe (MSVC) -- produces PDB symbols readable by whatdbg (dbgeng.dll)
set CC=cl
set CXX=cl
echo Using cl.exe with PDB debug symbols

:: Use VS-bundled cmake and ninja (MSYS2 cmake 4.x breaks RC compilation with MSVC)
set "PATH=!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;!VS_PATH!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;!PATH!"

echo.
echo CMake: && cmake --version
echo.
echo Ninja: && ninja --version
echo.

:: Configure (Ninja is single-config -- build type is baked at configure time)
:: Reconfigure automatically when the requested config differs from the existing one.
:: A marker file (Builds/Ninja/.build_config) stores the active config type.
set NEEDS_CONFIGURE=0

if not exist "Builds\Ninja" (
    set NEEDS_CONFIGURE=1
) else (
    set EXISTING_CONFIG=
    if exist "Builds\Ninja\.build_config" (
        set /p EXISTING_CONFIG=<"Builds\Ninja\.build_config"
    )
    if /i not "!EXISTING_CONFIG!"=="!CONFIG!" (
        echo Config changed [!EXISTING_CONFIG!] -^> [!CONFIG!], reconfiguring...
        rmdir /s /q "Builds\Ninja"
        set NEEDS_CONFIGURE=1
    )
)

if !NEEDS_CONFIGURE!==1 (
    echo Configuring [!CONFIG!]...
    cmake -S . -B Builds/Ninja -G Ninja -DCMAKE_BUILD_TYPE=!CONFIG! -DCMAKE_C_COMPILER="!CC!" -DCMAKE_CXX_COMPILER="!CXX!"
    if errorlevel 1 (
        echo CMake configure FAILED
        exit /b 1
    )
    echo !CONFIG!>"Builds\Ninja\.build_config"
)

:: Build
echo Building [!CONFIG!]...
cmake --build Builds/Ninja -- -j!NUMBER_OF_PROCESSORS!
if errorlevel 1 (
    echo Build FAILED
    exit /b 1
)

echo.
echo Build succeeded.
endlocal
set _END_BUILD_RUNNING=
