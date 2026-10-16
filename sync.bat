@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem  iperf3-gui-win32 sync helper
rem
rem  Usage:
rem    sync.bat                show changes -> ask for message -> commit + push
rem    sync.bat "your message" commit with given message and push
rem    sync.bat status         show status only, change nothing
rem    sync.bat push           push only
rem    sync.bat help           show this help
rem
rem  NOTE: ASCII only on purpose. Do NOT add Chinese text or save as
rem        UTF-8 - cmd misparses those bytes and eats following commands.
rem ============================================================

set "GIT=C:\Program Files\Git\bin\git.exe"
set "GH=C:\Program Files\GitHub CLI\gh.exe"
set "REPO=%~dp0"
set "TMPF=%TEMP%\iperf3gui_sync.tmp"

if not exist "%GIT%" (
    echo [ERROR] git not found: %GIT%
    exit /b 1
)

cd /d "%REPO%"
if errorlevel 1 (
    echo [ERROR] cannot enter: %REPO%
    exit /b 1
)

"%GIT%" rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [ERROR] not a git repo: %REPO%
    exit /b 1
)

if "%~1"==""          goto :interactive
if /i "%~1"=="status" goto :showstatus
if /i "%~1"=="push"   goto :dopush
if /i "%~1"=="help"   goto :showhelp
if /i "%~1"=="-h"     goto :showhelp
if /i "%~1"=="--help" goto :showhelp
goto :withmsg


:showhelp
echo.
echo Usage:
echo   sync.bat                 show changes, ask for message, commit + push
echo   sync.bat "message"       commit with the given message and push
echo   sync.bat status          show status only
echo   sync.bat push            push only
echo   sync.bat help            this help
echo.
echo Repo : %REPO%
echo Branch:
"%GIT%" branch --show-current
echo.
echo To publish a new release:
echo   "%GH%" release create v1.1.1 "..\iperf3gui.exe" --title "iperf3 GUI V1.1.1" --notes-file notes.md
echo.
exit /b 0


:showstatus
echo === Branch ===
"%GIT%" branch --show-current
echo.
echo === Changes ===
"%GIT%" status --short
echo.
echo === Last 5 commits ===
"%GIT%" log --oneline -5
echo.
exit /b 0


:dopush
echo === Pushing ===
"%GIT%" push
if errorlevel 1 (
    echo [ERROR] push failed
    exit /b 1
)
echo Done.
echo https://github.com/BenjmSek/iperf3-gui-win32
exit /b 0


:withmsg
set "MSG=%~1"
goto :collect


:interactive
echo === Branch ===
"%GIT%" branch --show-current
echo.
echo === Changes ===
"%GIT%" status --short
echo.

"%GIT%" status --porcelain > "%TMPF%" 2>nul
set "SIZE=0"
for %%A in ("%TMPF%") do set "SIZE=%%~zA"
if "%SIZE%"=="0" (
    echo No changes to commit.
    del "%TMPF%" >nul 2>&1
    goto :dopush
)
del "%TMPF%" >nul 2>&1

echo.
set "MSG="
set /p MSG=Commit message: 
if not defined MSG (
    echo [ERROR] empty message, aborted.
    exit /b 1
)


:collect
echo.
echo === Committing ===
"%GIT%" add -A
if errorlevel 1 (
    echo [ERROR] git add failed
    exit /b 1
)

"%GIT%" commit -m "%MSG%"
if errorlevel 1 (
    echo [ERROR] commit failed (maybe nothing staged)
    exit /b 1
)

goto :dopush
