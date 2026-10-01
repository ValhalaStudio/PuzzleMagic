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

	// The course (board size) and the play options picked in the menus.
	UPROPERTY()
	int32 CourseWidth = 8;

	UPROPERTY()
	int32 CourseHeight = 8;

	UPROPERTY()
	bool bOptionRelics = false;

	UPROPERTY()
	bool bOptionBonusTiles = false;

	// The "How to play" pages open by themselves on the very first launch only.
	UPROPERTY()
	bool bSeenTutorial = false;

	static UPuzzleSaveGame* LoadOrCreate();
	void Save();
};
