#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PuzzleSaveGame.generated.h"

UCLASS()
class PUZZLEGAME5X5_API UPuzzleSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("PuzzleProgress");

	UPROPERTY()
	int32 BestScore = 0;

	// The demo build keeps a best score for each of its two modes (BestScore is the one of the mode being played).
	UPROPERTY()
	int32 DemoBest1 = 0;

	UPROPERTY()
	int32 DemoBest2 = 0;

	// The course (board size) and the play options picked in the menus.
	UPROPERTY()
	int32 CourseWidth = 8;

	UPROPERTY()
	int32 CourseHeight = 8;

	UPROPERTY()
	bool bOptionRelics = false;

	UPROPERTY()
	bool bOptionBonusTiles = false;

	// Volume sliders in the options card (0 to 1).
	UPROPERTY()
	float MusicVolume = 0.7f;

	UPROPERTY()
	float SfxVolume = 0.8f;

	// Options card: Graphics Low (the lightweight scene) on a PC, and the window mode the player chose
	// (-1 not chosen yet: the engine's own setting stays; 0 windowed, 1 fullscreen).
	UPROPERTY()
	bool bLowGraphics = false;

	UPROPERTY()
	int32 DisplayMode = -1;

	// The theme (EPuzzleTheme as a number: 0 Desk, 1 Halloween, 2 Classic), chosen in Play options.
	UPROPERTY()
	uint8 Theme = 0;

	// Phones and tablets: hold the screen upright (portrait) instead of sideways (landscape, the default).
	UPROPERTY()
	bool bPortrait = false;

	// Arcade and tutorial: how many steps of the campaign are beaten in a row.
	UPROPERTY()
	int32 ArcadeProgress = 0;

	UPROPERTY()
	int32 TutorialProgress = 0;

	// The "How to play" pages open by themselves on the very first launch only.
	UPROPERTY()
	bool bSeenTutorial = false;

	static UPuzzleSaveGame* LoadOrCreate();
	void Save();
};
