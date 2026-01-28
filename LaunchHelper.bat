@echo off
rem LaunchHelper.bat - ProtoReadyHost
rem Usage: LaunchHelper.bat [Arguments]

setlocal

rem --- CONFIGURATION ---
set UE_VERSION=5.7
set PROJECT_NAME=ProtoReadyHost
set PROJECT_PATH=%~dp0%PROJECT_NAME%.uproject
set ENGINE_PATH=C:\Program Files\Epic Games\UE_%UE_VERSION%
set EDITOR_EXE="%ENGINE_PATH%\Engine\Binaries\Win64\UnrealEditor.exe"

echo =======================================================
echo   LAUNCHING %PROJECT_NAME% (%UE_VERSION%)
echo =======================================================

rem Check Editor
if not exist %EDITOR_EXE% (
    echo [ERROR] UnrealEditor.exe not found at: %EDITOR_EXE%
    echo Please verify UE_VERSION variable in this script.
    pause
    exit /b 1
)

rem Launch
start "" %EDITOR_EXE% "%PROJECT_PATH%" %*

endlocal
exit /b 0
