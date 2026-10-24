#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

// The demo build: the same game, trimmed. DEMO 1 (4x4, no bonus tiles, ends at 4000 points) and DEMO 2 (7x7, bonus tiles, ends at 6000)
// replace PLAY, TUTORIAL stays, and nothing else (no arcade, course choice, bot demo or theme toggle). The theme is always Desk and the chant music is left out.
// It is on when the packaged game has a DemoBuild.txt next to its Binaries folder (Tools/package_windows.py --demo writes it), or
// with the -demobuild launch flag (to try it from the editor build). Everything else is the normal game.
namespace PuzzleDemo
{
	// Two modes. Demo 1: a 4x4 board without bonus tiles, ending at 4000 points. Demo 2: a 7x7 board with bonus tiles, ending at 6000.
	// The mode is picked in the menu (APuzzleGameMode::StartDemoGame) and stays for PLAY AGAIN.
	inline int32 GMode = 2;
	inline int32 Mode() { return GMode; }
	inline int32 BoardSize(int32 InMode) { return InMode == 1 ? 4 : 7; }
	inline int32 BoardSize() { return BoardSize(GMode); }
	inline int32 ScoreCap(int32 InMode) { return InMode == 1 ? 4000 : 6000; }
	inline int32 ScoreCap() { return ScoreCap(GMode); }
	inline bool BonusTiles(int32 InMode) { return InMode != 1; }
	inline bool BonusTiles() { return BonusTiles(GMode); }

	inline bool GDemo = false;
	inline bool IsDemo() { return GDemo; }

	// Dev only: -captest brute-forces the cap. It implies the demo and takes its board and tray from the one-step campaign
	// Content/Campaigns/captest.json (a 4x4 board dealing only straight four-tile pieces, each of which completes a route), with the
	// bot playing and the cap applying to it.
	inline bool GCapTest = false;
	inline bool IsCapTest() { return GCapTest; }

	// Called once at startup, before the scenery is built.
	inline void Detect()
	{
		// -captest, or a CapTest.txt marker in the project or saved folder (a phone cannot take launch flags: adb push the file).
		// (the cap test is a 4x4 round without bonus tiles: Demo 1)
		GCapTest = FParse::Param(FCommandLine::Get(), TEXT("captest"))
			|| IFileManager::Get().FileExists(*FPaths::Combine(FPaths::ProjectDir(), TEXT("CapTest.txt")))
			|| IFileManager::Get().FileExists(*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CapTest.txt")));
#if defined(PUZZLE_DEMO_BUILD)
		constexpr bool bCompiledAsDemo = true; // built with PUZZLE_DEMO=1 (the demo APK)
#else
		constexpr bool bCompiledAsDemo = false;
#endif
		if (GCapTest)
		{
			GMode = 1;
		}
		GDemo = bCompiledAsDemo || GCapTest || FParse::Param(FCommandLine::Get(), TEXT("demobuild"))
			|| IFileManager::Get().FileExists(*FPaths::Combine(FPaths::ProjectDir(), TEXT("DemoBuild.txt")));
	}
}
