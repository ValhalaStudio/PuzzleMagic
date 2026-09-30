@echo off
rem Launches the game in a 1280x720 landscape window at the main menu.
rem In-game: drag pieces from the tray, drop one on HOLD to keep it; relic buttons (Relics option) sit in the bottom corners.
rem Keys: P opens the menu (take over from the demo there), R retries, Esc/right-click cancels Holy Light targeting.
call "%~dp0Tools\FindUE.bat" || (pause & exit /b 1)
start "" "%UE_EDITOR%" "%~dp0PuzzleGame5x5.uproject" -game -windowed -resx=1280 -resy=720
