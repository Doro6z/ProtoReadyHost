@echo off
setlocal

:: CONFIGURATION
set UE_VERSION=5.5
set ENGINE_PATH=C:\Program Files\Epic Games\UE_%UE_VERSION%
set UAT_PATH="%ENGINE_PATH%\Engine\Build\BatchFiles\RunUAT.bat"

set PLUGIN_FILE=PR_Footstep.uplugin
set OUTPUT_DIR=Packaged

:: PATHS
set SCRIPT_DIR=%~dp0
set PLUGIN_PATH=%SCRIPT_DIR%%PLUGIN_FILE%
set PACKAGE_Path=%SCRIPT_DIR%%OUTPUT_DIR%

echo [ACC] Starting Package Process for %PLUGIN_FILE%...
echo [ACC] Engine Version: %UE_VERSION%
echo [ACC] Plugin Path: %PLUGIN_PATH%
echo [ACC] Output: %PACKAGE_Path%

if not exist %UAT_PATH% (
    echo [ERROR] RunUAT.bat not found at: %UAT_PATH%
    echo Please edit this script to set the correct ENGINE_PATH.
    pause
    exit /b 1
)

echo [ACC] Cleaning Output Directory...
if exist "%PACKAGE_Path%" (
    rmdir /s /q "%PACKAGE_Path%"
)

echo [ACC] Running RunUAT...
call %UAT_PATH% BuildPlugin -Plugin="%PLUGIN_PATH%" -Package="%PACKAGE_Path%" -Rocket

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Packaging Failed!
    pause
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Plugin Packaged successfully in: %OUTPUT_DIR%
pause
endlocal
