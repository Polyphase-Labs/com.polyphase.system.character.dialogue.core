@echo off
REM Native Addon Build Script for Windows
REM Run this from the root of your addon folder (where package.json is)
REM
REM Usage: build.bat [binary_name]
REM   binary_name - Optional. Defaults to folder name if not specified.
REM
REM Requirements:
REM   - Visual Studio with C++ tools installed
REM   - Run from a "Developer Command Prompt" or ensure cl.exe is in PATH
REM
REM Output:
REM   build\Windows\x64\<binary_name>.dll
REM   build\Windows\x64\<binary_name>-Windows-x64.sha256

setlocal enabledelayedexpansion

REM Get addon folder name as default binary name
for %%I in (.) do set "FOLDER_NAME=%%~nxI"

REM Use argument or default to folder name
if "%~1"=="" (
    set "ADDON_NAME=%FOLDER_NAME%"
) else (
    set "ADDON_NAME=%~1"
)

echo.
echo ========================================
echo  Building Native Addon: %ADDON_NAME%
echo ========================================
echo.

REM Check for Source directory
if not exist "Source" (
    echo ERROR: Source directory not found!
    echo Make sure you're running this from the addon root folder.
    exit /b 1
)

REM Create build directory
if not exist "build" mkdir build
if not exist "build\Windows" mkdir build\Windows
if not exist "build\Windows\x64" mkdir build\Windows\x64

REM Check for cl.exe
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl.exe not found!
    echo Please run this from a Visual Studio Developer Command Prompt
    echo or run vcvarsall.bat first.
    echo.
    echo Try one of these:
    echo   "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    echo   "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    echo   "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    exit /b 1
)

REM Gather all .cpp files
set "SOURCES="
for /r "Source" %%f in (*.cpp) do (
    set "SOURCES=!SOURCES! "%%f""
)

if "!SOURCES!"=="" (
    echo ERROR: No .cpp files found in Source directory!
    exit /b 1
)

echo Found source files:
for /r "Source" %%f in (*.cpp) do (
    echo   %%~nxf
)
echo.

REM Build Release version
echo Building Release configuration...
echo.

pushd build\Windows\x64

cl /nologo /EHsc /O2 /MD /LD ^
    /I"..\..\..\Source" ^
    /Fe:"%ADDON_NAME%.dll" ^
    /Fo:"%ADDON_NAME%_" ^
    /D "OCTAVE_PLUGIN_EXPORT" ^
    /D "NDEBUG" ^
    /D "PLATFORM_WINDOWS=1" ^
    !SOURCES! ^
    /link /DLL /MACHINE:X64

if errorlevel 1 (
    popd
    echo.
    echo ========================================
    echo  BUILD FAILED!
    echo ========================================
    exit /b 1
)

popd

echo.
echo ========================================
echo  Build Succeeded!
echo ========================================
echo.
echo Output: build\Windows\x64\%ADDON_NAME%.dll
echo.

REM Generate checksum
echo Generating SHA256 checksum...
certutil -hashfile "build\Windows\x64\%ADDON_NAME%.dll" SHA256 > "build\Windows\x64\%ADDON_NAME%-Windows-x64.sha256" 2>nul
if exist "build\Windows\x64\%ADDON_NAME%-Windows-x64.sha256" (
    echo Checksum: build\Windows\x64\%ADDON_NAME%-Windows-x64.sha256
    type "build\Windows\x64\%ADDON_NAME%-Windows-x64.sha256"
)

echo.
echo ----------------------------------------
echo To test in Polyphase:
echo   1. Copy build\Windows\x64\%ADDON_NAME%.dll to your project's
echo      Intermediate\Plugins\%ADDON_NAME%\Synced\ folder
echo   2. Set the addon to Binary mode in the Addons window
echo   3. Click Reload to load the binary
echo ----------------------------------------
echo.

endlocal
