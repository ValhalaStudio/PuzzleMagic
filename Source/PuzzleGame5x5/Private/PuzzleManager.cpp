#include "PuzzleManager.h"
#include "GridManager.h"

void UPuzzleManager::BindToGrid(AGridManager* InGridManager)
{
	GridManager = InGridManager;
}

void UPuzzleManager::StartGame(EPlayMode InMode, const FQuestLevel& Level)
{
	Mode = InMode;
	Quest = Level;
	Score = 0;
	ComboStreak = 0;
	ComboWindow = 0;
	LastRoundScore = 0;
	StartingMoves = Mode == EPlayMode::Quest ? Level.Moves : EndlessStartingMoves;
	MovesLeft = StartingMoves;
	MovesMade = 0;
	LastBonusMoves = 0;
	// One of each to start, so the relic buttons are learnable from the first move.
	RelicCharges[0] = 1;
	RelicCharges[1] = 1;
	NextRelicCombo = RelicComboStep;
	NextRelic = ERelic::HolyLight;
	QuestProgress = 0;
	bFinished = false;
	bWon = false;
	Luck = StartingLuck;
	MovesSinceOmen = 0;
}

int32 UPuzzleManager::StoneInterval() const
{
	return Mode == EPlayMode::Quest ? Quest.StoneInterval : EndlessStoneInterval;
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
	const FClearResult Result = GridManager->CheckAndClearLines(AGridManager::ArriveDuration);

	--MovesLeft;
	++MovesMade;

	int32 RoundScore = Shape.Cells.Num();
	LastBonusMoves = 0;
	if (Result.Lines > 0)
	{
		// Multi-line clears push the combo up faster; any clear refills the window.
		ComboStreak += Result.Lines;
		ComboWindow = ComboWindowMoves;

		int32 ClearPoints = (Result.Cells * 2 + Result.Lines * 15 + Result.Blessings * 50 + Result.StonesBroken * 25) * ComboStreak;
		if (Result.bBlessedBox)
		{
			ClearPoints *= 3;
		}
		RoundScore += ClearPoints;

		int32 Bonus = Result.Lines + 2 * Result.Blessings + (ComboStreak >= 3 ? 1 : 0) + (ComboStreak >= 6 ? 1 : 0) + (Result.bBlessedBox ? 1 : 0);
		// Quests are tighter: a plain single clear doesn't refund its own move.
		if (Mode == EPlayMode::Quest)
		{
			Bonus -= 1;
		}
		LastBonusMoves = FMath::Max(Bonus, 0);
		MovesLeft += LastBonusMoves;
		AddLuck(LuckPerComboStep * Result.Lines);
		GrantRelics();
	}
	else if (ComboStreak > 0 && --ComboWindow <= 0)
	{
		const int32 Lost = ComboStreak;
		ComboStreak = 0;
		ComboWindow = 0;
		NextRelicCombo = RelicComboStep;
		OnComboBroken.Broadcast(Lost);
	}

	Score += RoundScore;
	LastRoundScore = RoundScore;
	AddQuestProgress(Result);

	if (Result.Lines > 0)
	{
		FPuzzleClearEvent Event;
		Event.Result = Result;
		Event.Points = RoundScore;
		Event.BonusMoves = LastBonusMoves;
		Event.Combo = ComboStreak;
		Event.Centroid = GridManager->GetLastClearCentroid();
		OnCleared.Broadcast(Event);
	}

	// Gargoyles land after the move resolves, so they never block the piece just played.
	const int32 Interval = StoneInterval();
	bool bStoneFell = false;
	if (Interval > 0 && MovesMade % Interval == 0)
	{
		FIntPoint Cell;
		if (GridManager->SpawnCurseStone(Cell, AGridManager::ArriveDuration + 0.7f))
		{
			bStoneFell = true;
			OnStoneSpawned.Broadcast(Cell);
		}
	}

	// One misfortune per move: an omen never lands on the same move as a gargoyle.
	++MovesSinceOmen;
	if (!bStoneFell)
	{
		RollOmen();
	}

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
	if (!GridManager || bFinished || Charges <= 0)
	{
		return false;
	}
	--Charges;
	AddLuck(-HolyLightLuckCost);

	const FClearResult Result = GridManager->ClearArea(CenterX, CenterY, 0.05f);
	const int32 Points = Result.Cells * 3 + Result.StonesBroken * 25;
	Score += Points;
	LastRoundScore = Points;
	AddQuestProgress(Result);

	FPuzzleClearEvent Event;
	Event.Result = Result;
	Event.Points = Points;
	Event.Combo = ComboStreak;
	Event.bHolyLight = true;
	Event.Centroid = GridManager->GetLastClearCentroid();
	OnCleared.Broadcast(Event);

	CheckForEnd();
	return true;
}

bool UPuzzleManager::UseReroll()
{
	int32& Charges = RelicCharges[static_cast<int32>(ERelic::Reroll)];
	if (!GridManager || bFinished || Charges <= 0)
	{
		return false;
	}
	--Charges;
	AddLuck(-RerollLuckCost);
	GridManager->RerollTray();
	CheckForEnd();
	return true;
}

void UPuzzleManager::AddLuck(int32 Delta)
{
	Luck = FMath::Clamp(Luck + Delta, 0, MaxLuck);
}

float UPuzzleManager::GetOmenChance() const
{
	if (OmenChanceOverride >= 0.f)
	{
		return OmenChanceOverride;
	}
	if (Mode == EPlayMode::Quest)
	{
		// The first levels teach the basics in calm weather; the storm arrives with the gargoyles.
		return Quest.Number < 5 ? 0.f : FMath::Min(0.08f + 0.012f * (Quest.Number - 5), 0.2f);
	}
	return MovesMade < 4 ? 0.f : FMath::Min(0.1f + 0.002f * MovesMade, 0.25f);
}

void UPuzzleManager::RollOmen()
{
	if (MovesSinceOmen < OmenCooldownMoves || FMath::FRand() >= GetOmenChance())
	{
		return;
	}

	// A hex with no moves to steal would be an empty threat: the storm comes instead.
	const EOmen Omen = (MovesLeft > HexMoveCost && FMath::RandBool()) ? EOmen::Hex : EOmen::Lightning;
	FIntPoint Cell(AGridManager::GridSize / 2, AGridManager::GridSize / 2);
	if (Omen == EOmen::Lightning)
	{
		Cell = GridManager->PickLightningTarget();
		if (Cell.X < 0)
		{
			return; // nothing left to strike
		}
	}
	MovesSinceOmen = 0;

	const bool bWarded = FMath::FRand() < GetWardChance();
	if (bWarded)
	{
		AddLuck(-WardLuckCost);
	}
	else if (Omen == EOmen::Hex)
	{
		MovesLeft -= HexMoveCost;
	}
	if (Omen == EOmen::Lightning)
	{
		GridManager->StrikeLightning(Cell, OmenDelay, bWarded);
	}
	else
	{
		GridManager->PlayHex(OmenDelay, bWarded);
	}
	OnOmen.Broadcast(Omen, bWarded, Cell);
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

void UPuzzleManager::AddQuestProgress(const FClearResult& Result)
{
	if (Mode != EPlayMode::Quest)
	{
		return;
	}
	switch (Quest.Goal)
	{
	case EQuestGoal::CollectSymbol: QuestProgress += Result.ColorCounts[static_cast<int32>(Quest.Color)]; break;
	case EQuestGoal::ClearLines:    QuestProgress += Result.Lines; break;
	case EQuestGoal::ClearBoxes:    QuestProgress += Result.Boxes; break;
	case EQuestGoal::BreakStones:   QuestProgress += Result.StonesBroken; break;
	case EQuestGoal::ReachScore:    QuestProgress = Score; break;
	}
}

int32 UPuzzleManager::ComputeStars() const
{
	if (!bWon)
	{
		return 0;
	}
	const float Spare = static_cast<float>(MovesLeft) / FMath::Max(Quest.Moves, 1);
	return Spare >= 0.4f ? 3 : (Spare >= 0.2f ? 2 : 1);
}

void UPuzzleManager::CheckForEnd()
{
	if (bFinished)
	{
		return;
	}
	if (Mode == EPlayMode::Quest && QuestProgress >= Quest.Target)
	{
		bFinished = true;
		bWon = true;
		OnFinished.Broadcast(true);
		return;
	}
	if (IsGameOver())
	{
		bFinished = true;
		bWon = false;
		OnFinished.Broadcast(false);
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
