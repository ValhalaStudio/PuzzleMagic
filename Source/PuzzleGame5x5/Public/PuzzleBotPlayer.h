#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PuzzleBotPlayer.generated.h"

class AGridManager;
class UPuzzleManager;

// Simple greedy heuristic bot: evaluates every (tray slot, board position)
// combination it could legally play and takes the highest-scoring one.
// Used to drive an "auto-play" demo mode rather than as a serious solver.
UCLASS(BlueprintType)
class PUZZLEGAME5X5_API UPuzzleBotPlayer : public UObject
{
	GENERATED_BODY()

public:
	void Init(AGridManager* InGridManager, UPuzzleManager* InPuzzleManager);

	// Attempts one placement. Returns false if no legal move exists (game over).
	UFUNCTION(BlueprintCallable, Category = "Puzzle|AI")
	bool TakeBestAction();

private:
	UPROPERTY()
	TObjectPtr<AGridManager> GridManager;

	UPROPERTY()
	TObjectPtr<UPuzzleManager> PuzzleManager;
};
