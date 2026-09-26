@echo off
rem Launches the game in an iPhone-shaped portrait window with the computer playing (auto-play demo).
rem In-game: P toggles demo/manual play, R restarts.

echo Launching game and preparing to record...
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0PuzzleGame5x5.uproject" -game -windowed -resx=440 -resy=950 -demo

rem Wait 4 seconds to give the engine time to launch and initialize the window viewport
timeout /t 4 /nobreak

echo ==========================================================
echo  RECORDING STARTED! 
echo  When the demo finishes, click this window and press 'q' to stop.
echo ==========================================================

rem Captures the exact window using its exact title layout.
rem Adjust the resolution text or name if your local project title bar displays differently.
"C:\ffmpeg\bin\ffmpeg.exe" -f gdigrab -framerate 60 -i title="PuzzleGame5x5 (64-bit, PCD3D_SM6)" -c:v libx264 -pix_fmt yuv420p -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" "%~dp0DemoCapture.mp4"

echo Recording saved as DemoCapture.mp4 inside the project folder!
pause
