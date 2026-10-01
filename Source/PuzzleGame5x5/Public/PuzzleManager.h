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

// The rules: scoring, the combo window, move budget, relics and luck. The board itself (cells, tray, visuals) lives in AGridManager.
UCLASS(BlueprintType)
class PUZZLEGAME5X5_API UPuzzleManager : public UObject
{
	GENERATED_BODY()

public:
	// The move budget is off for the MVP: no move limit, a round ends when no piece fits.
	static constexpr bool bMoveBudgetEnabled = false;

	// Play options (menu > Play options). "Relics" switches combo, relics and luck on together; with
	// them off clears score one route at a time and luck stays at its starting value.
	bool bComboEnabled = false;
	bool bRelicsEnabled = false;
	bool bLuckEnabled = false;
	void SetExtrasEnabled(bool bOn) { bComboEnabled = bOn; bRelicsEnabled = bOn; bLuckEnabled = bOn; }

	// Play option "Bonus tiles". Stored here; the bonus tiles themselves come in a later step.
	bool bBonusTilesEnabled = false;

	// A combo breaks after this many placements in a row without a clear (so it survives one fewer).
	static constexpr int32 ComboWindowMoves = 3;
	// Every this many combo steps earns a relic.
	static constexpr int32 RelicComboStep = 3;
	static constexpr int32 MaxRelicCharges = 3;
	static constexpr int32 StartingMovesCount = 30;

	// Scoring: a cleared route is worth 10 and each tile of its sequence 2, with no exponents. Routes that share
	// tiles each earn 5 more for every route cleared at the same time.
	static constexpr int32 RoutePoints = 10;
	static constexpr int32 RouteTilePoints = 2;
	static constexpr int32 SharedRoutePoints = 5;

	// Bonus tiles (Play options > Bonus tiles): a pumpkin every 200 points, an outgoing/incoming bottle pair every 600.
	// A cleared pumpkin scores 25, a bottle on its own 50, each plus 2 for every tile of its chain; an outgoing bottle
	// linked to an incoming one scores a flat 100. A bonus tile has no arrow: an outgoing bottle sends a chain out
	// through any neighbour, an incoming one takes a chain arriving from any side, and a pumpkin does both.
	static constexpr int32 BasicBonusEvery = 200;
	static constexpr int32 PairBonusEvery = 600;
	// At most this many of each kind are ever on the board at once.
	static constexpr int32 MaxBasicBonusTiles = 2;
	static constexpr int32 MaxDirectionalBonusTiles = 1;
	static constexpr int32 BasicBonusPoints = 25;
	static constexpr int32 DirectionalBonusPoints = 50;
	static constexpr int32 LinkedBonusPoints = 100;
	static constexpr int32 BonusChainTilePoints = 2;

	// Luck (0-100): gathered by growing a combo, spent by using relics (the combo's rewards).
	static constexpr int32 MaxLuck = 100;
	static constexpr int32 StartingLuck = 20;
	static constexpr int32 LuckPerComboStep = 6;
	static constexpr int32 HolyLightLuckCost = 20;
	static constexpr int32 RerollLuckCost = 12;

	void BindToGrid(AGridManager* InGridManager);

	// Resets all rule state.
	void StartGame();

	// Places a tray piece (slot 0-2, or the reserve) and resolves clears, scoring,
	// combo, bonus moves and relics.
	bool TryPlacePiece(int32 TraySlot, int32 OriginX, int32 OriginY);

	// Moves a tray piece into the hold slot. Free: costs no move.
	bool ParkPiece(int32 TraySlot);

	bool UseHolyLight(int32 CenterX, int32 CenterY);
	bool UseReroll();

	bool IsGameOver() const;
	bool IsOutOfMoves() const { return bMoveBudgetEnabled && MovesLeft <= 0; }
	bool IsFinished() const { return bFinished; }
	bool IsStuck() const;

	int32 GetRelicCharges(ERelic Relic) const { return RelicCharges[static_cast<int32>(Relic)]; }

	int32 GetLuck() const { return Luck; }
	int32 Score = 0;
	int32 ComboStreak = 0;
	// Placements left before the combo breaks (shown as pips).
	int32 ComboWindow = 0;
	int32 LastRoundScore = 0;
	int32 StartingMoves = StartingMovesCount;
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

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBonusSpawned, FIntPoint);
	FOnBonusSpawned OnBonusSpawned;

	DECLARE_MULTICAST_DELEGATE(FOnFinished);
	FOnFinished OnFinished;


private:
	UPROPERTY()
	TObjectPtr<AGridManager> GridManager;

	int32 RelicCharges[2] = { 0, 0 };
	int32 NextRelicCombo = RelicComboStep;
	ERelic NextRelic = ERelic::HolyLight;
	bool bFinished = false;
	int32 Luck = StartingLuck;
	int32 NextBasicBonusScore = BasicBonusEvery;
	int32 NextPairBonusScore = PairBonusEvery;

	void AddLuck(int32 Delta);
	// The points of the routes cleared together: 10 per route and 2 per tile of its sequence, plus 5 per route
	// cleared at once for each route that shares tiles with another.
	static int32 RoutesPoints(const TArray<FRouteInfo>& Routes);
	static int32 BonusPoints(const FBonusEvent& Event);
	void SpawnDueBonusTiles();
	void GrantRelics();
	void CheckForEnd();
};
