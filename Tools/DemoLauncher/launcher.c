/* The demo's PuzzleGame5x5.exe: starts the Shipping game next to it with -NotInstalled and any arguments it was given.
 *
 * A Shipping Unreal game saves its settings and progress to AppData unless it runs with -NotInstalled, and the engine's own launcher
 * exe cannot carry that argument on Windows. This one can, so double-clicking the exe keeps every file in the game's folder.
 * Built by build_launcher.ps1 and swapped into the demo zip by Tools/package_windows.py --demo.
 */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR cmdLine, int show)
{
	wchar_t self[MAX_PATH];
	if (!GetModuleFileNameW(NULL, self, MAX_PATH)) { return 1; }
	wchar_t* slash = wcsrchr(self, L'\\');
	if (slash) { *slash = 0; }

	wchar_t binDir[MAX_PATH * 2];
	wchar_t inner[MAX_PATH * 2];
	swprintf(binDir, MAX_PATH * 2, L"%ls\\PuzzleGame5x5\\Binaries\\Win64", self);
	swprintf(inner, MAX_PATH * 2, L"%ls\\PuzzleGame5x5-Win64-Shipping.exe", binDir);

	wchar_t line[8192];
	swprintf(line, 8192, L"\"%ls\" -NotInstalled %ls", inner, cmdLine ? cmdLine : L"");

	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcessW(inner, line, NULL, NULL, FALSE, 0, NULL, binDir, &si, &pi))
	{
		MessageBoxW(NULL, L"Could not start the game. Please unzip the whole folder first.", L"Puzzle Magic", MB_ICONERROR);
		return 1;
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return 0;
}
