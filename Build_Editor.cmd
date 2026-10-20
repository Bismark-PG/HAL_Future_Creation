@echo off
setlocal
title HAL Future Creation - Build Editor
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\BuildEditor.ps1" %*
set "HAL_BUILD_EXIT=%ERRORLEVEL%"
echo.
echo Build tool exit code: %HAL_BUILD_EXIT%
pause
exit /b %HAL_BUILD_EXIT%
