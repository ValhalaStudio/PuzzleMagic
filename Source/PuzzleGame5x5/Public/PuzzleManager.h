#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PuzzleTypes.h"
#include "GridManager.h"
#include "PuzzleManager.generated.h"

// Everything the UI needs to celebrate one clear.
struct FPuzzleClearEvent
{
	FClearResult Result;
	int32 Points = 0;
	int32 BonusMoves = 0;
	int32 Combo = 0;
	bool bHolyLight = false;
	FVector Centroid = FVector::ZeroVector;
};

// The rules: scoring, the combo window, move budget, relics, gargoyle stones, quest
// goals, luck and omens. The board itself (cells, tray, visuals) lives in AGridManager.
UCLASS(BlueprintType)
class PUZZLEGAME5X5_API UPuzzleManager : public UObject
{
	GENERATED_BODY()

public:
	// A combo breaks after this many placements in a row without a clear (so it survives one fewer).
	static constexpr int32 ComboWindowMoves = 3;
	// Every this many combo steps earns a relic.
	static constexpr int32 RelicComboStep = 3;
	static constexpr int32 MaxRelicCharges = 3;
	static constexpr int32 EndlessStartingMoves = 30;
	static constexpr int32 EndlessStoneInterval = 8;

	// Luck (0-100): gathered by growing a combo, spent by using relics (the combo's rewards).
	// When an omen strikes, luck is the chance (capped) that it is warded off.
	static constexpr int32 MaxLuck = 100;
	static constexpr int32 StartingLuck = 20;
	static constexpr int32 LuckPerComboStep = 6;
	static constexpr int32 HolyLightLuckCost = 20;
	static constexpr int32 RerollLuckCost = 12;
	// Warding an omen off burns some of the luck that did it.
	static constexpr int32 WardLuckCost = 10;
	static constexpr int32 MaxWardPercent = 90;
	static constexpr int32 HexMoveCost = 2;
	// Omens never come closer together than this many moves.
	static constexpr int32 OmenCooldownMoves = 3;
	// An omen strikes this long after the move that summoned it (once the clears have popped).
	static constexpr float OmenDelay = 1.1f;

	void BindToGrid(AGridManager* InGridManager);

	// Resets all rule state. Level is only used in Quest mode.
	void StartGame(EPlayMode InMode, const FQuestLevel& Level);

	// Places a tray piece (slot 0-2, or the reserve) and resolves clears, scoring,
	// combo, bonus moves, relics, stones and quest progress.
	bool TryPlacePiece(int32 TraySlot, int32 OriginX, int32 OriginY);

	// Moves a tray piece into the hold slot. Free: costs no move.
	bool ParkPiece(int32 TraySlot);

	bool UseHolyLight(int32 CenterX, int32 CenterY);
	bool UseReroll();

	bool IsGameOver() const;
	bool IsOutOfMoves() const { return MovesLeft <= 0; }
	bool IsFinished() const { return bFinished; }
	bool HasWon() const { return bWon; }
	bool IsStuck() const;

	int32 GetRelicCharges(ERelic Relic) const { return RelicCharges[static_cast<int32>(Relic)]; }
	int32 GetQuestProgress() const { return QuestProgress; }
	int32 ComputeStars() const;

	int32 GetLuck() const { return Luck; }
	// Chance (0-1) that the next omen is warded off.
	float GetWardChance() const { return FMath::Min(Luck, MaxWardPercent) / 100.f; }
	// Chance (0-1) that an omen strikes after a move, once the cooldown has passed.
	float GetOmenChance() const;

	// -omenrate=N (0-1) on the command line: fixed omen chance per move, for testing and demos.
	float OmenChanceOverride = -1.f;

	EPlayMode Mode = EPlayMode::Endless;
	FQuestLevel Quest;

	int32 Score = 0;
	int32 ComboStreak = 0;
	// Placements left before the combo breaks (shown as pips).
	int32 ComboWindow = 0;
	int32 LastRoundScore = 0;
	int32 StartingMoves = EndlessStartingMoves;
	int32 MovesLeft = 0;
	int32 MovesMade = 0;
	int32 LastBonusMoves = 0;

	DECLARE_MULTICAST_DELEGATE(FOnPiecePlaced);
	FOnPiecePlaced OnPiecePlaced;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCleared, const FPuzzleClearEvent&);
	FOnCleared OnCleared;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnComboBroken, int32 /*LostCombo*/);
	FOnComboBroken OnComboBroken;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRelicGained, ERelic);
	FOnRelicGained OnRelicGained;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStoneSpawned, FIntPoint);
	FOnStoneSpawned OnStoneSpawned;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFinished, bool /*bWon*/);
	FOnFinished OnFinished;

	// Cell is the struck cell for Lightning (the board centre for Hex).
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnOmen, EOmen, bool /*bWarded*/, FIntPoint /*Cell*/);
	FOnOmen OnOmen;

private:
	UPROPERTY()
	TObjectPtr<AGridManager> GridManager;

	int32 RelicCharges[2] = { 0, 0 };
	int32 NextRelicCombo = RelicComboStep;
	ERelic NextRelic = ERelic::HolyLight;
	int32 QuestProgress = 0;
	bool bFinished = false;
	bool bWon = false;
	int32 Luck = StartingLuck;
	int32 MovesSinceOmen = 0;

	int32 StoneInterval() const;
	void AddLuck(int32 Delta);
	void RollOmen();
	void AddQuestProgress(const FClearResult& Result);
	void GrantRelics();
	void CheckForEnd();
};
