@echo off
rem ===========================================================
rem  Lumio Photo - build script for Qt + MinGW (Windows)
rem  Result:
rem    LumioPhoto-Setup.exe  - one-file installer (install or portable)
rem    dist\LumioPhoto.exe   - ready-to-run program folder
rem ===========================================================
setlocal
cd /d "%~dp0"

rem ---- If auto-detection fails, uncomment and edit these two lines ----
rem set "QT_DIR=C:\Qt\6.12.0\mingw_64"
rem set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"

if not defined QT_DIR (
    for /d %%V in ("C:\Qt\5.15*" "C:\Qt\6.*") do (
        for /d %%K in ("%%~V\mingw*") do set "QT_DIR=%%~K"
    )
)
if not defined MINGW_DIR (
    for /d %%M in ("C:\Qt\Tools\mingw*") do set "MINGW_DIR=%%~M"
)

if not defined QT_DIR (
    echo [ERROR] Qt for MinGW not found in C:\Qt
    echo         Open build.bat and set QT_DIR manually.
    pause
    exit /b 1
)

echo Qt:    %QT_DIR%
echo MinGW: %MINGW_DIR%
echo.

set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%"

rem Close a running copy so its files can be replaced
taskkill /im LumioPhoto.exe /f >nul 2>nul

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found. Install it with Qt Maintenance Tool ^(Developer and Designer Tools - CMake^).
    pause
    exit /b 1
)

echo === 1/3 Building Lumio Photo ===
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=%QT_DIR%"
if errorlevel 1 goto :fail
cmake --build build -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 goto :fail

echo.
echo === 2/3 Collecting DLLs into dist ===
if exist dist rmdir /s /q dist
mkdir dist
copy /y build\LumioPhoto.exe dist\ >nul
if errorlevel 1 goto :fail
windeployqt --release --no-translations --compiler-runtime --no-opengl-sw --no-system-d3d-compiler dist\LumioPhoto.exe
if errorlevel 1 goto :fail

echo.
echo === 3/3 Building installer LumioPhoto-Setup.exe ===
if exist build\payload.zip del /q build\payload.zip
pushd dist
cmake -E tar cf ..\build\payload.zip --format=zip .
if errorlevel 1 (popd & goto :fail)
popd
cmake -S installer -B build-installer -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release "-DPAYLOAD_ZIP=%CD%\build\payload.zip"
if errorlevel 1 goto :fail
cmake --build build-installer -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 goto :fail
copy /y build-installer\LumioPhoto-Setup.exe LumioPhoto-Setup.exe >nul
if errorlevel 1 goto :fail

echo.
echo ===========================================================
echo  DONE!
echo    LumioPhoto-Setup.exe  - run it to install Lumio Photo (or make a portable copy)
echo    dist\LumioPhoto.exe   - or run the program directly from here
echo ===========================================================
pause
exit /b 0

:fail
echo.
echo [ERROR] Build failed - see messages above.
pause
exit /b 1
