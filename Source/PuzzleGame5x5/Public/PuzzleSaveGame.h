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
	int32 HighestUnlockedLevel = 1;

	// Best star rating per quest level (index = level - 1).
	UPROPERTY()
	TArray<int32> LevelStars;

	UPROPERTY()
	int32 BestEndlessScore = 0;

	// The "How to play" pages open by themselves on the very first launch only.
	UPROPERTY()
	bool bSeenTutorial = false;

	int32 GetStars(int32 Level) const { return LevelStars.IsValidIndex(Level - 1) ? LevelStars[Level - 1] : 0; }

	void RecordStars(int32 Level, int32 Stars)
	{
		if (LevelStars.Num() < Level)
		{
			LevelStars.SetNumZeroed(Level);
		}
		LevelStars[Level - 1] = FMath::Max(LevelStars[Level - 1], Stars);
	}

	static UPuzzleSaveGame* LoadOrCreate();
	void Save();
};
