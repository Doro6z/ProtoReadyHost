@echo off
rem BuildHelper.bat - ProtoReadyHost
rem Usage: BuildHelper.bat [Debug|Development|Shipping] [-LiveCoding]

setlocal

rem --- CONFIGURATION ---
set UE_VERSION=5.7
set PROJECT_NAME=ProtoReadyHost
set PROJECT_PATH=%~dp0%PROJECT_NAME%.uproject
set ENGINE_PATH=C:\Program Files\Epic Games\UE_%UE_VERSION%
set UBT_PATH="%ENGINE_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"

rem --- ARGUMENTS ---
set CONFIGURATION=Development
if not "%1"=="" set CONFIGURATION=%1
set ARGS=%2 %3 %4

echo =======================================================
echo   BUILDING %PROJECT_NAME% (%CONFIGURATION% Editor)
echo =======================================================

rem Check UBT
if not exist %UBT_PATH% (
    echo [ERROR] UnrealBuildTool not found at: %UBT_PATH%
    echo Please verify UE_VERSION variable in this script.
    pause
    exit /b 1
)

rem Run UBT
%UBT_PATH% %PROJECT_NAME%Editor Win64 %CONFIGURATION% -Project="%PROJECT_PATH%" -WaitMutex -FromMsBuild %ARGS%

if %ERRORLEVEL% NEQ 0 (
    echo [FAILURE] Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build completed.
endlocal
exit /b 0
