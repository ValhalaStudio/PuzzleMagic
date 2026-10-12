<#
Builds the test packages: compiles the editor target, cooks and packages Windows and/or Android, and makes the Windows zip.

  powershell -File Tools\build_packages.ps1 [-Win] [-Android] [-Ue "C:\Epic Games\UE_5.8"] [-Out "D:\PuzzleMagic_Builds"]

With neither -Win nor -Android, both are built. Defaults: the engine named by the .uproject's EngineAssociation (looked up in the
registry, then under the usual folders) and the folder PuzzleMagic_Builds next to the repo. Android needs the Android SDK (adb and
the NDK 27.2.12479018) and JDK 21; set ANDROID_HOME and JAVA_HOME if they are not in their usual places.
Close the editor and any running game first, and never run two builds at once. The cook sometimes dies at teardown with an engine
ensure after "Done!"; each package is retried once.
#>
param(
  [switch]$Win, [switch]$Android, [switch]$SkipCompile, [switch]$DryRun,
  [string]$Ue = "",
  [string]$Out = ""
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$uproject = Join-Path $repo "PuzzleGame5x5.uproject"
if (-not ($Win -or $Android)) { $Win = $true; $Android = $true }
if (-not $Out) { $Out = Join-Path (Split-Path -Parent $repo) "PuzzleMagic_Builds" }
New-Item -ItemType Directory -Force $Out | Out-Null

# --- the engine
if (-not $Ue) {
  $assoc = (Get-Content $uproject -Raw | ConvertFrom-Json).EngineAssociation
  foreach ($c in @("HKCU:\Software\Epic Games\Unreal Engine\Builds", "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc")) {
    if (Test-Path $c) {
      $p = Get-ItemProperty $c
      $v = $p.$assoc; if (-not $v) { $v = $p.InstalledDirectory }
      if ($v -and (Test-Path $v)) { $Ue = $v; break }
    }
  }
  if (-not $Ue) { foreach ($d in "C:\Program Files\Epic Games\UE_$assoc", "D:\Epic\UE_$assoc", "K:\Epic\UE_$assoc") { if (Test-Path $d) { $Ue = $d; break } } }
}
if (-not $Ue -or -not (Test-Path (Join-Path $Ue "Engine\Build\BatchFiles\RunUAT.bat"))) { throw "Unreal Engine not found: pass -Ue <install folder>" }
$uat = Join-Path $Ue "Engine\Build\BatchFiles\RunUAT.bat"
$buildBat = Join-Path $Ue "Engine\Build\BatchFiles\Build.bat"
Write-Host "engine: $Ue  output: $Out"
if ($DryRun) { Write-Host "dry run: would build win=$Win android=$Android compile=$(-not $SkipCompile)"; return }

function Stop-Leftovers { Get-Process UnrealEditor, UnrealEditor-Cmd, dotnet -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 3 }
function Run-Uat($packageArgs, $log) {
  foreach ($try in 1..2) {
    & $uat @packageArgs *> $log
    if ($LASTEXITCODE -eq 0) { return }
    Write-Host "package attempt $try failed (exit $LASTEXITCODE), see $log"
    Stop-Leftovers
  }
  throw "packaging failed twice: $log"
}

Stop-Leftovers
if (-not $SkipCompile) {
  & $buildBat PuzzleGame5x5Editor Win64 Development "-Project=$uproject" -WaitMutex
  if ($LASTEXITCODE -ne 0) { throw "compile failed" }
  Get-Process dotnet -ErrorAction SilentlyContinue | Stop-Process -Force
}

$common = @("BuildCookRun", "-project=$uproject", "-noP4", "-clientconfig=Development", "-build", "-cook", "-stage", "-pak", "-archive", "-utf8output", "-unattended", "-nocompileeditor")

if ($Win) {
  $dir = Join-Path $Out "WindowsPortable"
  Run-Uat ($common + @("-platform=Win64", "-archivedirectory=$dir")) (Join-Path $Out "win_portable_build.log")
  python (Join-Path $repo "Tools\package_windows.py") $dir (Join-Path $Out "PuzzleMagic_Windows_Portable.zip")
}

if ($Android) {
  if (-not $env:ANDROID_HOME) { $env:ANDROID_HOME = Join-Path $env:LOCALAPPDATA "Android\Sdk" }
  $env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
  if (-not $env:NDKROOT) { $env:NDKROOT = Join-Path $env:ANDROID_HOME "ndk\27.2.12479018" }
  $env:NDK_ROOT = $env:NDKROOT
  if (-not $env:JAVA_HOME) { $env:JAVA_HOME = "C:\Program Files\Java\jdk-21" }
  $dir = Join-Path $Out "AndroidDev"
  Run-Uat ($common + @("-platform=Android", "-cookflavor=ASTC", "-package", "-archivedirectory=$dir")) (Join-Path $Out "android_dev_build.log")
  Copy-Item (Join-Path $dir "PuzzleGame5x5-arm64.apk") (Join-Path $Out "PuzzleMagic_Android_test.apk") -Force
  Write-Host "APK: $(Join-Path $Out 'PuzzleMagic_Android_test.apk')"
}
Write-Host "done"
