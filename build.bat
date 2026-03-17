@echo off
:: build.bat - Bootstrap build script for END (Windows)
:: Sets up MSVC environment then runs CMake + Ninja
:: Produces PDB debug symbols for whatdbg (dbgeng.dll-based DAP adapter)
::
:: Usage:
::   build.bat           - configure + build (Debug)
::   build.bat Release   - configure + build (Release)
::   build.bat clean     - delete Builds/Ninja and reconfigure

setlocal

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug
if "%CONFIG%"=="clean" (
    echo Cleaning Builds/Ninja...
    if exist "Builds\Ninja" rmdir /s /q "Builds\Ninja"
    set CONFIG=Debug
)

:: Find vcvarsall.bat via vswhere
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo ERROR: vswhere.exe not found. Is Visual Studio installed?
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -property installationPath`) do (
    set VS_PATH=%%i
)

set VCVARSALL="%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VCVARSALL% (
    echo ERROR: vcvarsall.bat not found at %VCVARSALL%
    exit /b 1
)

echo Setting up MSVC x64 environment...
call %VCVARSALL% x64

:: Use cl.exe (MSVC) — produces PDB symbols readable by whatdbg (dbgeng.dll)
set CC=cl
set CXX=cl
echo Using cl.exe with PDB debug symbols

:: Use VS-bundled cmake and ninja (MSYS2 cmake 4.x breaks RC compilation with MSVC)
set PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%

echo.
echo CMake: && cmake --version
echo.
echo Ninja: && ninja --version
echo.

:: Configure
if not exist "Builds\Ninja" (
    echo Configuring...
    cmake -S . -B Builds/Ninja -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_C_COMPILER="%CC%" -DCMAKE_CXX_COMPILER="%CXX%"
    if errorlevel 1 (
        echo CMake configure FAILED
        exit /b 1
    )
)

:: Build
echo Building (%CONFIG%)...
cmake --build Builds/Ninja --config %CONFIG% -- -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo Build FAILED
    exit /b 1
)

echo.
echo Build succeeded.
endlocal
