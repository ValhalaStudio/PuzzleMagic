@echo off
rem The computer plays through the quest levels from level 1 (intro card, play, star rating, next level).
rem Demo results are never saved to your progress.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0PuzzleGame5x5.uproject" -game -windowed -resx=440 -resy=950 -demoquest=1
