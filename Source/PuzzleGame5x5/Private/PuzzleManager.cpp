#include "PuzzleManager.h"
#include "GridManager.h"

void UPuzzleManager::BindToGrid(AGridManager* InGridManager)
{
	GridManager = InGridManager;
}

void UPuzzleManager::StartGame()
{
	Score = 0;
	ComboStreak = 0;
	ComboWindow = 0;
	LastRoundScore = 0;
	StartingMoves = StartingMovesCount;
	MovesLeft = StartingMoves;
	MovesMade = 0;
	LastBonusMoves = 0;
	// One of each to start, so the relic buttons are learnable from the first move.
	RelicCharges[0] = bRelicsEnabled ? 1 : 0;
	RelicCharges[1] = bRelicsEnabled ? 1 : 0;
	NextRelicCombo = RelicComboStep;
	NextRelic = ERelic::HolyLight;
	bFinished = false;
	Luck = StartingLuck;
	NextBasicBonusScore = BasicBonusEvery;
	NextPairBonusScore = PairBonusEvery;
}

bool UPuzzleManager::TryPlacePiece(int32 TraySlot, int32 OriginX, int32 OriginY)
{
	if (!GridManager || bFinished || IsOutOfMoves() || !GridManager->Tray.IsValidIndex(TraySlot) || GridManager->TraySlotUsed[TraySlot])
	{
		return false;
	}

	const FPuzzlePieceShape Shape = GridManager->Tray[TraySlot];
	if (!GridManager->PlacePieceAt(Shape, OriginX, OriginY, TraySlot))
	{
		return false;
	}
	GridManager->ConsumeTraySlot(TraySlot);
	OnPiecePlaced.Broadcast();

	// The board state clears now; the visual pop waits for the piece to land.
	FClearResult Result = GridManager->CheckAndClearLines(AGridManager::ArriveDuration);

	if (bMoveBudgetEnabled)
	{
		--MovesLeft;
	}
	++MovesMade;

	// Placing a piece scores nothing; only clears do.
	int32 RoundScore = 0;
	LastBonusMoves = 0;
	if (Result.Lines > 0)
	{
		// Multi-line clears push the combo up faster; any clear refills the window.
		if (bComboEnabled)
		{
			ComboStreak += Result.Lines;
			ComboWindow = ComboWindowMoves;
		}

		const int32 Multiplier = bComboEnabled ? ComboStreak : 1;
		int32 ClearPoints = Result.Cells * 2 + Result.Lines * 15;
		for (const FRouteInfo& Route : Result.Routes)
		{
			ClearPoints += RouteBonus(Route);
		}
		RoundScore += ClearPoints * Multiplier;

		if (bMoveBudgetEnabled)
		{
			LastBonusMoves = Result.Lines + (ComboStreak >= 3 ? 1 : 0) + (ComboStreak >= 6 ? 1 : 0);
			MovesLeft += LastBonusMoves;
		}
		AddLuck(LuckPerComboStep * Result.Lines);
		if (bRelicsEnabled)
		{
			GrantRelics();
		}
	}
	else if (bComboEnabled && ComboStreak > 0 && --ComboWindow <= 0)
	{
		const int32 Lost = ComboStreak;
		ComboStreak = 0;
		ComboWindow = 0;
		NextRelicCombo = RelicComboStep;
		OnComboBroken.Broadcast(Lost);
	}

	// Bonus tiles cleared by chains add their own points (never multiplied by the combo).
	for (FBonusEvent& Bonus : Result.Bonuses)
	{
		Bonus.Points = BonusPoints(Bonus);
		RoundScore += Bonus.Points;
	}

	Score += RoundScore;
	LastRoundScore = RoundScore;

	if (Result.Lines > 0 || Result.CircuitCells > 0 || Result.Bonuses.Num() > 0)
	{
		FPuzzleClearEvent Event;
		Event.Result = Result;
		Event.Points = RoundScore;
		Event.BonusMoves = LastBonusMoves;
		Event.Combo = ComboStreak;
		Event.Centroid = GridManager->GetLastClearCentroid();
		OnCleared.Broadcast(Event);
	}

	SpawnDueBonusTiles();
	CheckForEnd();
	return true;
}

bool UPuzzleManager::ParkPiece(int32 TraySlot)
{
	return GridManager && !bFinished && GridManager->ParkPiece(TraySlot);
}

bool UPuzzleManager::UseHolyLight(int32 CenterX, int32 CenterY)
{
	int32& Charges = RelicCharges[static_cast<int32>(ERelic::HolyLight)];
	if (!bRelicsEnabled || !GridManager || bFinished || Charges <= 0)
	{
		return false;
	}
	--Charges;
	AddLuck(-HolyLightLuckCost);

	const FClearResult Result = GridManager->ClearArea(CenterX, CenterY, 0.05f);
	const int32 Points = Result.Cells * 3;
	Score += Points;
	LastRoundScore = Points;

	FPuzzleClearEvent Event;
	Event.Result = Result;
	Event.Points = Points;
	Event.Combo = ComboStreak;
	Event.bHolyLight = true;
	Event.Centroid = GridManager->GetLastClearCentroid();
	OnCleared.Broadcast(Event);

	SpawnDueBonusTiles();
	CheckForEnd();
	return true;
}

bool UPuzzleManager::UseReroll()
{
	int32& Charges = RelicCharges[static_cast<int32>(ERelic::Reroll)];
	if (!bRelicsEnabled || !GridManager || bFinished || Charges <= 0)
	{
		return false;
	}
	--Charges;
	AddLuck(-RerollLuckCost);
	GridManager->RerollTray();
	CheckForEnd();
	return true;
}

int32 UPuzzleManager::PowerBonus(int32 ExtraTiles, int32 Base)
{
	if (ExtraTiles <= 0)
	{
		return 0;
	}
	int64 Bonus = 1;
	for (int32 I = 0; I < ExtraTiles && Bonus < MaxRouteBonus; ++I)
	{
		Bonus *= Base;
	}
	return static_cast<int32>(FMath::Min<int64>(Bonus, MaxRouteBonus));
}

int32 UPuzzleManager::RouteBonus(const FRouteInfo& Route)
{
	return PowerBonus(Route.ExtraTiles, Route.bNeighbouring ? 5 : 10);
}

int32 UPuzzleManager::BonusPoints(const FBonusEvent& Event)
{
	if (Event.bLinked)
	{
		return LinkedBonusPoints;
	}
	return (Event.Kind == EPuzzleBonus::Basic ? BasicBonusPoints : DirectionalBonusPoints) + PowerBonus(Event.ExtraTiles, 10);
}

void UPuzzleManager::SpawnDueBonusTiles()
{
	if (!bBonusTilesEnabled || !GridManager)
	{
		return;
	}
	// One tile (or pair) per scoring step, however far a big clear carries the score past the marks.
	FIntPoint Cell;
	if (Score >= NextBasicBonusScore)
	{
		NextBasicBonusScore = (Score / BasicBonusEvery + 1) * BasicBonusEvery;
		if (GridManager->CountBonusTiles(EPuzzleBonus::Basic) < MaxBasicBonusTiles && GridManager->SpawnBonusTile(EPuzzleBonus::Basic, Cell))
		{
			OnBonusSpawned.Broadcast(Cell);
		}
	}
	if (Score >= NextPairBonusScore)
	{
		NextPairBonusScore = (Score / PairBonusEvery + 1) * PairBonusEvery;
		if (GridManager->CountBonusTiles(EPuzzleBonus::Outgoing) < MaxDirectionalBonusTiles && GridManager->SpawnBonusTile(EPuzzleBonus::Outgoing, Cell))
		{
			OnBonusSpawned.Broadcast(Cell);
		}
		if (GridManager->CountBonusTiles(EPuzzleBonus::Incoming) < MaxDirectionalBonusTiles && GridManager->SpawnBonusTile(EPuzzleBonus::Incoming, Cell))
		{
			OnBonusSpawned.Broadcast(Cell);
		}
	}
}

void UPuzzleManager::AddLuck(int32 Delta)
{
	if (bLuckEnabled)
	{
		Luck = FMath::Clamp(Luck + Delta, 0, MaxLuck);
	}
}

void UPuzzleManager::GrantRelics()
{
	while (ComboStreak >= NextRelicCombo)
	{
		NextRelicCombo += RelicComboStep;
		int32& Charges = RelicCharges[static_cast<int32>(NextRelic)];
		if (Charges < MaxRelicCharges)
		{
			++Charges;
			OnRelicGained.Broadcast(NextRelic);
		}
		NextRelic = NextRelic == ERelic::HolyLight ? ERelic::Reroll : ERelic::HolyLight;
	}
}

void UPuzzleManager::CheckForEnd()
{
	if (bFinished)
	{
		return;
	}
	if (IsGameOver())
	{
		bFinished = true;
		OnFinished.Broadcast();
	}
}

bool UPuzzleManager::IsStuck() const
{
	return GridManager && !GridManager->CanAnyTrayPieceFit();
}

bool UPuzzleManager::IsGameOver() const
{
	if (!GridManager)
	{
		return false;
	}
	if (IsOutOfMoves())
	{
		return true;
	}
	// Stuck only ends the run once no relic can dig you out.
	return IsStuck() && GetRelicCharges(ERelic::HolyLight) == 0 && GetRelicCharges(ERelic::Reroll) == 0;
}
