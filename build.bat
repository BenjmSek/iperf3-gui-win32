@echo off
REM Build script for iperf3 GUI V1.1 (Win32 API + GDI+)
REM Uses MinGW32 (C:\mingw32) - GCC 16.2.0 i686
REM chart.cpp is C++ (GDI+ classes), so we use g++ for everything

set MINGW=C:\mingw32\bin
set WINDRES=%MINGW%\windres.exe
set GPP=%MINGW%\g++.exe
set PROJECT_DIR=%~dp0
set OUTPUT=iperf3gui.exe

echo === Building iperf3 GUI V1.1 (Win32 + GDI+) ===
echo Compiler: %GPP%

REM Step 1: Compile resource file (.rc -> .res)
echo.
echo Compiling resources...
%WINDRES% src\app.rc -O coff -o src\app.res
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo === RESOURCE COMPILATION FAILED ===
    exit /b 1
)

REM Step 2: Compile and link source files + resource
echo.
echo Compiling...
%GPP% -O2 -Wall ^
    -DUNICODE -D_UNICODE ^
    -DWIN32_LEAN_AND_MEAN ^
    src\main.c src\iperf3.c src\chart.cpp src\replay.c src\lang.c src\license_text.c src\help_text.c src\app.res ^
    -o %OUTPUT% ^
    -lgdiplus -lcomctl32 -lcomdlg32 -lshlwapi -liphlpapi ^
    -luser32 -lkernel32 -lgdi32 -lshell32 -lole32 ^
    -mwindows -municode ^
    -static -static-libgcc

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo === BUILD FAILED ===
    exit /b 1
)

echo.
echo === BUILD SUCCEEDED ===
echo Output: %PROJECT_DIR%%OUTPUT%

REM Show file size
for %%A in (%OUTPUT%) do echo Size: %%~zA bytes

echo.
echo Done.
