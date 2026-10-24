# Builds PuzzleGame5x5.exe from launcher.c with the Visual Studio C++ tools already needed for the game.
#   powershell -File Tools\DemoLauncher\build_launcher.ps1 [-Out <exe path>]
param([string]$Out = "")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Out) { $Out = Join-Path $here "PuzzleGame5x5.exe" }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere not found: install the Visual Studio C++ build tools" }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "no Visual Studio with the C++ x64 tools found" }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$obj = Join-Path $env:TEMP "puzzle_launcher_obj"
New-Item -ItemType Directory -Force $obj | Out-Null
$objFile = Join-Path $obj "launcher.obj"
$source = Join-Path $here "launcher.c"
# vcvars64.bat looks for vswhere on the PATH. A batch file keeps the quoting simple: no path ends in a backslash before a quote.
$vswhereDir = Split-Path -Parent $vswhere
$batch = Join-Path $obj "build.cmd"
@(
  "@echo off",
  "set PATH=%PATH%;$vswhereDir",
  "call `"$vcvars`" >nul",
  "cl /nologo /O1 /W3 /DUNICODE /D_UNICODE /Fo`"$objFile`" `"$source`" /Fe:`"$Out`" user32.lib /link /SUBSYSTEM:WINDOWS"
) | Set-Content $batch -Encoding ascii
cmd /c "`"$batch`"" | Out-Host
if (-not (Test-Path $Out)) { throw "launcher build failed" }
"launcher: $Out ($([math]::Round((Get-Item $Out).Length/1KB)) KB)"
