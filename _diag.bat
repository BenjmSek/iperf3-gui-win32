@echo off
setlocal
set "GIT=C:\Program Files\Git\bin\git.exe"
set "PATH=C:\Program Files\Git\bin;C:\Program Files\Git\cmd;C:\Program Files\GitHub CLI;%PATH%"
set "REPO=%~dp0"
set "TMPF=%TEMP%\iperf3gui_sync.tmp"
cd /d "%REPO%"
echo D1-start
"%GIT%" rev-parse --is-inside-work-tree >nul 2>&1
echo D1b-revparse-rc=%errorlevel%
if /i "%~1"=="push" goto :dopush
if not "%~1"=="" goto :commit
echo D1c-no-arg
exit /b 0

:commit
echo D2-in-commit
if not "%~1"=="" set "MSG=%~1"
echo D2b-msg=[%MSG%]
if not defined MSG (
    echo D2c-no-msg
    exit /b 1
)
echo D3-before-add
"%GIT%" add -A
echo D3b-add-rc=%errorlevel%
echo D4-before-commit
"%GIT%" commit -m "%MSG%"
echo D4b-commit-rc=%errorlevel%
if errorlevel 1 (
    echo D4c-commit-failed
    exit /b 1
)
echo D5-before-push
"%GIT%" push
echo D5b-push-rc=%errorlevel%
exit /b 0

:dopush
echo D6-in-dopush
"%GIT%" push
echo D6b-push-rc=%errorlevel%
exit /b 0
