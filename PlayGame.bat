@echo off
rem Launches the game in an iPhone-shaped portrait window at the main menu (Endless or Quest levels).
rem In-game: drag pieces from the tray, drop one on HOLD to keep it; relic buttons sit in the bottom corners.
rem Keys: P toggles demo/manual play, R retries, Esc/right-click cancels Holy Light targeting.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0PuzzleGame5x5.uproject" -game -windowed -resx=440 -resy=950
