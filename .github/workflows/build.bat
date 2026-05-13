@echo off
REM Native Addon Build Script for Windows
REM Run this from the root of your addon folder (where package.json is)
REM
REM Usage: build.bat [binary_name] [config]
REM   binary_name - Optional. Defaults to folder name if not specified.
REM   config      - Optional. "Debug", "Release", or "Both" (default: Both)
REM
REM Environment:
REM   POLYPHASE_PATH - Path to Polyphase engine installation (required for engine headers)
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

REM Determine addon root (script may be in .github/workflows/ or addon root)
set "ADDON_ROOT=."
if exist "..\..\Source" if exist "..\..\package.json" set "ADDON_ROOT=..\.."

REM Resolve ADDON_ROOT to an absolute path so include flags survive pushd into the build dir
for %%I in ("%ADDON_ROOT%") do set "ADDON_ROOT=%%~fI"

REM Resolve POLYPHASE_PATH to absolute (same reason)
if defined POLYPHASE_PATH for %%I in ("%POLYPHASE_PATH%") do set "POLYPHASE_PATH=%%~fI"

REM Check for Source directory
if not exist "%ADDON_ROOT%\Source" (
    echo ERROR: Source directory not found!
    echo Make sure you're running this from the addon root folder or .github\workflows\.
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
pushd "%ADDON_ROOT%"
for /r "Source" %%f in (*.cpp) do (
    set "SOURCES=!SOURCES! "%%f""
)
popd

if "!SOURCES!"=="" (
    echo ERROR: No .cpp files found in Source directory!
    exit /b 1
)

echo Found source files:
pushd "%ADDON_ROOT%"
for /r "Source" %%f in (*.cpp) do (
    echo   %%~nxf
)
popd
echo.

REM Build include paths
set "INCLUDE_FLAGS=/I"%ADDON_ROOT%\Source""
set "ENGINE_DEFINES="
set "ENGINE_LIBS_RELEASE="
set "ENGINE_LIBS_DEBUG="

if defined POLYPHASE_PATH (
    echo Using Polyphase engine at: %POLYPHASE_PATH%
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\Engine\Source""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\Engine\Source\Engine""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\Engine\Source\Editor""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\Engine\Source\Plugins""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External\Assimp""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External\Bullet""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External\Lua""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External\glm""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External\Imgui""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External\ImGuizmo""
    set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%POLYPHASE_PATH%\External\Vorbis""

    REM Add VULKAN_SDK if available
    if defined VULKAN_SDK set "INCLUDE_FLAGS=!INCLUDE_FLAGS! /I"%VULKAN_SDK%\Include""

    REM Add common engine defines
    set "ENGINE_DEFINES=/D EDITOR=1 /D LUA_ENABLED=1 /D GLM_FORCE_RADIANS /D API_VULKAN=1 /D NOMINMAX"

    REM Locate engine import libs. Without these the addon DLL cannot
    REM resolve any engine __imp_* symbol (LogWarning, Stream::*, ImGui::*,
    REM lua_*, etc.) and the linker fails with ~200+ LNK2019/LNK2001 errors.
    REM Three layouts are supported:
    REM   1. Installed engine (Inno Setup output staged by
    REM      Installers/stage_distribution.py). Libs at the install root:
    REM        <POLYPHASE_PATH>\Polyphase.lib
    REM        <POLYPHASE_PATH>\Lua.lib
    REM        <POLYPHASE_PATH>\PolyphaseEditor.lib  (DLL flavor, optional)
    REM      Default: link against Polyphase.lib (static editor flavor) —
    REM      matches the editor exe most end-user installs run today.
    REM      Set POLYPHASE_LINK_DLL=1 to link against PolyphaseEditor.lib
    REM      for addons that target a DLL-flavor editor build.
    REM   2. Polyphase SDK zip (downloaded by native-addon-release.yml from
    REM      the engine repo's GitHub release). Libs at:
    REM        Lib\Windows\x64\ReleaseEditor\Polyphase.lib
    REM        Lib\Windows\x64\ReleaseEditor\Lua.lib
    REM      Only Release-CRT libs are shipped today.
    REM   3. Local engine source tree built via MSBuild. Libs at:
    REM        Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.lib
    REM        External\Lua\Build\Windows\x64\ReleaseEditor\Lua.lib
    REM      Debug variants live under DebugEditor\ when the dev has built them.
    REM Sequential probes with an explicit flag — DO NOT use
    REM   if exist A if exist B (...) else if exist C (...)
    REM because cmd.exe binds the `else` to the INNER `if exist B`, so when
    REM A doesn't exist the entire chain (including C) is silently skipped.
    REM Symptom: CI links with no engine libs and gets ~200 LNK2019 errors
    REM even though the SDK was extracted to %POLYPHASE_PATH%.
    set "_LIBS_FOUND=0"

    REM Each ENGINE_LIBS_* assignment below uses `set VAR=...` (no outer
    REM quotes around VAR=value) on purpose. The values contain literal "
    REM characters around each path, and cmd's `set "VAR=..."` form would
    REM read from set's opening " to the first " after = — which is the
    REM quote that opens our first path — silently producing an EMPTY
    REM variable and treating the rest of the line as a stray command.
    REM Symptom: probe logs "Engine import libs: SDK layout" but the link
    REM still gets ~200 LNK2019 because ENGINE_LIBS_RELEASE expanded to "".

    REM 1. Installer layout (libs at %POLYPHASE_PATH% root)
    if exist "%POLYPHASE_PATH%\Polyphase.lib" (
        if exist "%POLYPHASE_PATH%\Lua.lib" (
            set "_INSTALLED_LIB=%POLYPHASE_PATH%\Polyphase.lib"
            if "%POLYPHASE_LINK_DLL%"=="1" (
                if exist "%POLYPHASE_PATH%\PolyphaseEditor.lib" (
                    set "_INSTALLED_LIB=%POLYPHASE_PATH%\PolyphaseEditor.lib"
                    echo Engine import libs: installed engine ^(DLL flavor — POLYPHASE_LINK_DLL=1^)
                ) else (
                    echo Engine import libs: installed engine ^(static flavor — PolyphaseEditor.lib not found despite POLYPHASE_LINK_DLL=1^)
                )
            ) else (
                echo Engine import libs: installed engine ^(static flavor^)
            )
            set ENGINE_LIBS_RELEASE="!_INSTALLED_LIB!" "%POLYPHASE_PATH%\Lua.lib"
            set "_LIBS_FOUND=1"
            REM No Debug-CRT libs ship in the installer today.
        )
    )

    REM 2. SDK zip layout (Lib\Windows\x64\ReleaseEditor\)
    if "!_LIBS_FOUND!"=="0" (
        if exist "%POLYPHASE_PATH%\Lib\Windows\x64\ReleaseEditor\Polyphase.lib" (
            echo Engine import libs: SDK layout
            set ENGINE_LIBS_RELEASE="%POLYPHASE_PATH%\Lib\Windows\x64\ReleaseEditor\Polyphase.lib" "%POLYPHASE_PATH%\Lib\Windows\x64\ReleaseEditor\Lua.lib"
            if exist "%POLYPHASE_PATH%\Lib\Windows\x64\DebugEditor\Polyphase.lib" (
                set ENGINE_LIBS_DEBUG="%POLYPHASE_PATH%\Lib\Windows\x64\DebugEditor\Polyphase.lib" "%POLYPHASE_PATH%\Lib\Windows\x64\DebugEditor\Lua.lib"
            )
            set "_LIBS_FOUND=1"
        )
    )

    REM 3. Local engine source-tree build (Standalone\Build\Windows\x64\ReleaseEditor\)
    if "!_LIBS_FOUND!"=="0" (
        if exist "%POLYPHASE_PATH%\Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.lib" (
            echo Engine import libs: local engine source-tree build
            set ENGINE_LIBS_RELEASE="%POLYPHASE_PATH%\Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.lib" "%POLYPHASE_PATH%\External\Lua\Build\Windows\x64\ReleaseEditor\Lua.lib"
            if exist "%POLYPHASE_PATH%\Standalone\Build\Windows\x64\DebugEditor\Polyphase.lib" (
                set ENGINE_LIBS_DEBUG="%POLYPHASE_PATH%\Standalone\Build\Windows\x64\DebugEditor\Polyphase.lib" "%POLYPHASE_PATH%\External\Lua\Build\Windows\x64\DebugEditor\Lua.lib"
            )
            set "_LIBS_FOUND=1"
        )
    )

    if "!_LIBS_FOUND!"=="0" (
        echo WARNING: No engine import lib found under %POLYPHASE_PATH%.
        echo          Expected one of:
        echo            %POLYPHASE_PATH%\Polyphase.lib                                            ^(installed engine^)
        echo            %POLYPHASE_PATH%\Lib\Windows\x64\ReleaseEditor\Polyphase.lib              ^(SDK layout^)
        echo            %POLYPHASE_PATH%\Standalone\Build\Windows\x64\ReleaseEditor\Polyphase.lib ^(source-tree build^)
        echo          Link will fail with unresolved engine symbols.
    )
    echo.
) else (
    echo Note: POLYPHASE_PATH not set. Only addon Source\ will be included.
    echo       Set POLYPHASE_PATH for addons that use engine headers.
    echo.
)

REM Set build output directory relative to addon root
set "BUILD_DIR=%ADDON_ROOT%\build"

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

if not exist "%BUILD_DIR%\Windows\x64\Release" mkdir "%BUILD_DIR%\Windows\x64\Release"
pushd "%BUILD_DIR%\Windows\x64\Release"

cl /nologo /EHsc /O2 /MD /LD ^
    !INCLUDE_FLAGS! ^
    !ENGINE_DEFINES! ^
    /Fe:"%ADDON_NAME%.dll" ^
    /D "OCTAVE_PLUGIN_EXPORT" ^
    /D "NDEBUG" ^
    /D "PLATFORM_WINDOWS=1" ^
    !SOURCES! ^
    /link /DLL /MACHINE:X64 !ENGINE_LIBS_RELEASE!

if errorlevel 1 (
    popd
    echo Release build FAILED!
    set "BUILD_FAILED=1"
    goto :CheckDebug
)

popd
echo Release build succeeded: %BUILD_DIR%\Windows\x64\Release\%ADDON_NAME%.dll

REM Generate Release checksum
certutil -hashfile "%BUILD_DIR%\Windows\x64\Release\%ADDON_NAME%.dll" SHA256 > "%BUILD_DIR%\Windows\x64\Release\%ADDON_NAME%-Windows-x64-Release.sha256" 2>nul
echo.

:CheckDebug
REM Build Debug if requested
if /i "%BUILD_CONFIG%"=="Debug" goto :CheckDebugLibs
if /i "%BUILD_CONFIG%"=="Both" goto :CheckDebugLibs
goto :Summary

:CheckDebugLibs
REM Skip Debug build cleanly when Debug-CRT engine import libs aren't
REM available. The Polyphase SDK zip only ships Release-CRT libs today,
REM so CI without a custom Debug-engine staging step would otherwise
REM produce hundreds of LNK2019 errors here. The editor's
REM NativeAddonManager already falls back to source-compile for Debug
REM addons when no Debug binary is shipped (#if defined(_DEBUG) path),
REM so end-users with Debug editors aren't blocked by this skip.
if defined POLYPHASE_PATH (
    if not defined ENGINE_LIBS_DEBUG (
        echo ----------------------------------------
        echo Skipping Debug build: no Debug-CRT engine import libs found.
        echo ----------------------------------------
        echo Looked for:
        echo   %POLYPHASE_PATH%\Lib\Windows\x64\DebugEditor\Polyphase.lib
        echo   %POLYPHASE_PATH%\Standalone\Build\Windows\x64\DebugEditor\Polyphase.lib
        echo The Polyphase SDK zip ships Release-CRT libs only. Debug-flavor
        echo editors will source-compile this addon at runtime via the
        echo NativeAddonManager Debug fallback.
        echo.
        goto :Summary
    )
)

:BuildDebug
echo ----------------------------------------
echo Building Debug configuration...
echo ----------------------------------------
echo.

if not exist "%BUILD_DIR%\Windows\x64\Debug" mkdir "%BUILD_DIR%\Windows\x64\Debug"
pushd "%BUILD_DIR%\Windows\x64\Debug"

cl /nologo /EHsc /Od /MDd /LD /Zi ^
    !INCLUDE_FLAGS! ^
    !ENGINE_DEFINES! ^
    /Fe:"%ADDON_NAME%.dll" ^
    /Fd:"%ADDON_NAME%.pdb" ^
    /D "OCTAVE_PLUGIN_EXPORT" ^
    /D "_DEBUG" ^
    /D "PLATFORM_WINDOWS=1" ^
    !SOURCES! ^
    /link /DLL /MACHINE:X64 /DEBUG !ENGINE_LIBS_DEBUG!

if errorlevel 1 (
    popd
    echo Debug build FAILED!
    set "BUILD_FAILED=1"
    goto :Summary
)

popd
echo Debug build succeeded: %BUILD_DIR%\Windows\x64\Debug\%ADDON_NAME%.dll

REM Generate Debug checksum
certutil -hashfile "%BUILD_DIR%\Windows\x64\Debug\%ADDON_NAME%.dll" SHA256 > "%BUILD_DIR%\Windows\x64\Debug\%ADDON_NAME%-Windows-x64-Debug.sha256" 2>nul
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
    if exist "%ADDON_ROOT%\package.json" (
        echo.
        echo Updating package.json with binary descriptors...
        pushd "%ADDON_ROOT%"
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
        popd
    )
)
echo.
echo Output directory: %BUILD_DIR%\Windows\x64\
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
