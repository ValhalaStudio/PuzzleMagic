@echo off
rem Sets UE_EDITOR to UnrealEditor.exe of an Unreal Engine 5.8 install. Called by the launchers in the project folder.
rem Search order: the UE_ROOT environment variable, the Epic Launcher's 5.8 install,
rem any source/custom build registered in the current user's Builds key, then the default launcher folder.

if defined UE_ROOT goto found

for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%B"
if defined UE_ROOT goto found

for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Epic Games\Unreal Engine\Builds" 2^>nul ^| find "REG_SZ"') do (
	if not defined UE_ROOT findstr /c:"\"MinorVersion\": 8," "%%B\Engine\Build\Build.version" >nul 2>&1 && findstr /c:"\"MajorVersion\": 5," "%%B\Engine\Build\Build.version" >nul 2>&1 && set "UE_ROOT=%%B"
)
if defined UE_ROOT goto found

set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"

:found
set "UE_EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
if exist "%UE_EDITOR%" exit /b 0
echo Unreal Engine 5.8 not found: "%UE_EDITOR%" does not exist.
echo Set UE_ROOT to the engine folder, for example:  set UE_ROOT=D:\Epic\UE_5.8
exit /b 1
