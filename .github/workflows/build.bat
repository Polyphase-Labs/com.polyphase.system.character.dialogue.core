@echo off
REM Native Addon Build Script for Windows
REM Run this from the root of your addon folder (where package.json is)
REM
REM Usage: build.bat [binary_name] [config]
REM   binary_name - Optional. Defaults to folder name if not specified.
REM   config      - Optional. "Debug", "Release", or "Both" (default: Both)
REM
REM Requirements:
REM   - Visual Studio with C++ tools installed
REM   - Run from a "Developer Command Prompt" or ensure cl.exe is in PATH
REM
REM Output:
REM   build\Windows\x64\Release\<binary_name>.dll
REM   build\Windows\x64\Debug\<binary_name>.dll

setlocal enabledelayedexpansion

REM Get addon folder name as default binary name
for %%I in (.) do set "FOLDER_NAME=%%~nxI"

REM Use argument or default to folder name
if "%~1"=="" (
    set "ADDON_NAME=%FOLDER_NAME%"
) else (
    set "ADDON_NAME=%~1"
)

REM Config: Debug, Release, or Both (default)
if "%~2"=="" (
    set "BUILD_CONFIG=Both"
) else (
    set "BUILD_CONFIG=%~2"
)

echo.
echo ========================================
echo  Building Native Addon: %ADDON_NAME%
echo  Configuration: %BUILD_CONFIG%
echo ========================================
echo.

REM Check for Source directory
if not exist "Source" (
    echo ERROR: Source directory not found!
    echo Make sure you're running this from the addon root folder.
    exit /b 1
)

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

set "BUILD_FAILED=0"

REM Build Release if requested
if /i "%BUILD_CONFIG%"=="Release" goto :BuildRelease
if /i "%BUILD_CONFIG%"=="Both" goto :BuildRelease
goto :CheckDebug

:BuildRelease
echo ----------------------------------------
echo Building Release configuration...
echo ----------------------------------------
echo.

if not exist "build\Windows\x64\Release" mkdir "build\Windows\x64\Release"
pushd build\Windows\x64\Release

cl /nologo /EHsc /O2 /MD /LD ^
    /I"..\..\..\..\Source" ^
    /Fe:"%ADDON_NAME%.dll" ^
    /Fo:"%ADDON_NAME%_" ^
    /D "OCTAVE_PLUGIN_EXPORT" ^
    /D "NDEBUG" ^
    /D "PLATFORM_WINDOWS=1" ^
    !SOURCES! ^
    /link /DLL /MACHINE:X64

if errorlevel 1 (
    popd
    echo Release build FAILED!
    set "BUILD_FAILED=1"
    goto :CheckDebug
)

popd
echo Release build succeeded: build\Windows\x64\Release\%ADDON_NAME%.dll

REM Generate Release checksum
certutil -hashfile "build\Windows\x64\Release\%ADDON_NAME%.dll" SHA256 > "build\Windows\x64\Release\%ADDON_NAME%-Windows-x64-Release.sha256" 2>nul
echo.

:CheckDebug
REM Build Debug if requested
if /i "%BUILD_CONFIG%"=="Debug" goto :BuildDebug
if /i "%BUILD_CONFIG%"=="Both" goto :BuildDebug
goto :Summary

:BuildDebug
echo ----------------------------------------
echo Building Debug configuration...
echo ----------------------------------------
echo.

if not exist "build\Windows\x64\Debug" mkdir "build\Windows\x64\Debug"
pushd build\Windows\x64\Debug

cl /nologo /EHsc /Od /MDd /LD /Zi ^
    /I"..\..\..\..\Source" ^
    /Fe:"%ADDON_NAME%.dll" ^
    /Fo:"%ADDON_NAME%_" ^
    /Fd:"%ADDON_NAME%.pdb" ^
    /D "OCTAVE_PLUGIN_EXPORT" ^
    /D "_DEBUG" ^
    /D "PLATFORM_WINDOWS=1" ^
    !SOURCES! ^
    /link /DLL /MACHINE:X64 /DEBUG

if errorlevel 1 (
    popd
    echo Debug build FAILED!
    set "BUILD_FAILED=1"
    goto :Summary
)

popd
echo Debug build succeeded: build\Windows\x64\Debug\%ADDON_NAME%.dll

REM Generate Debug checksum
certutil -hashfile "build\Windows\x64\Debug\%ADDON_NAME%.dll" SHA256 > "build\Windows\x64\Debug\%ADDON_NAME%-Windows-x64-Debug.sha256" 2>nul
echo.

:Summary
echo.
if "%BUILD_FAILED%"=="1" (
    echo ========================================
    echo  BUILD COMPLETED WITH ERRORS
    echo ========================================
) else (
    echo ========================================
    echo  Build Succeeded!
    echo ========================================

    REM Auto-update package.json with binary descriptors
    if exist "package.json" (
        echo.
        echo Updating package.json with binary descriptors...
        powershell -NoProfile -ExecutionPolicy Bypass -Command ^
            "$pkg = Get-Content 'package.json' -Raw | ConvertFrom-Json; " ^
            "$binaries = @(); " ^
            "if (Test-Path 'build\Windows\x64\Release\%ADDON_NAME%.dll') { " ^
            "  $binaries += @{platform='Windows'; arch='x64'; config='Release'; type='releaseAsset'; value='%ADDON_NAME%-Windows-x64-Release.dll'} " ^
            "}; " ^
            "if (Test-Path 'build\Windows\x64\Debug\%ADDON_NAME%.dll') { " ^
            "  $binaries += @{platform='Windows'; arch='x64'; config='Debug'; type='releaseAsset'; value='%ADDON_NAME%-Windows-x64-Debug.dll'} " ^
            "}; " ^
            "if ($binaries.Count -gt 0) { " ^
            "  if (-not $pkg.PSObject.Properties['binaries']) { " ^
            "    $pkg | Add-Member -NotePropertyName 'binaries' -NotePropertyValue @() " ^
            "  }; " ^
            "  foreach ($b in $binaries) { " ^
            "    $exists = $pkg.binaries | Where-Object { $_.platform -eq $b.platform -and $_.arch -eq $b.arch -and $_.config -eq $b.config }; " ^
            "    if (-not $exists) { $pkg.binaries += [PSCustomObject]$b } " ^
            "  }; " ^
            "  $pkg | ConvertTo-Json -Depth 10 | Set-Content 'package.json' -Encoding UTF8; " ^
            "  Write-Host '  Added Windows binary descriptors to package.json' " ^
            "}"
    )
)
echo.
echo Output directory: build\Windows\x64\
if /i "%BUILD_CONFIG%"=="Both" (
    echo   Release\%ADDON_NAME%.dll
    echo   Debug\%ADDON_NAME%.dll
) else (
    echo   %BUILD_CONFIG%\%ADDON_NAME%.dll
)
echo.
echo ----------------------------------------
echo To test in Polyphase:
echo   1. Copy the appropriate .dll to your project's
echo      Intermediate\Plugins\%ADDON_NAME%\Synced\ folder
echo   2. Set the addon to Binary mode in the Addons window
echo   3. Click Reload to load the binary
echo ----------------------------------------
echo.

if "%BUILD_FAILED%"=="1" exit /b 1
endlocal
