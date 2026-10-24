@echo off
rem One-command development shortcuts. Run with no argument for the list.
rem   dev build | dev full | dev go | dev run | dev test
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\dev.ps1" %*
exit /b %ERRORLEVEL%
