#include "PuzzleBotPlayer.h"
#include "GridManager.h"
#include "PuzzleManager.h"

void UPuzzleBotPlayer::Init(AGridManager* InGridManager, UPuzzleManager* InPuzzleManager)
{
	GridManager = InGridManager;
	PuzzleManager = InPuzzleManager;
}

bool UPuzzleBotPlayer::TakeBestAction()
{
	if (!GridManager || !PuzzleManager || PuzzleManager->IsFinished())
	{
		return false;
	}

	// Stuck: dig out with a relic (reroll first, it's the cheaper one).
	if (PuzzleManager->IsStuck())
	{
		if (PuzzleManager->UseReroll())
		{
			return true;
		}
		const FIntPoint Area = GridManager->FindDensestArea();
		return PuzzleManager->UseHolyLight(Area.X, Area.Y);
	}

	// A crowded board is worth a Holy Light before it gets desperate.
	const int32 CellCount = GridManager->GridWidth * GridManager->GridHeight;
	if (PuzzleManager->GetRelicCharges(ERelic::HolyLight) > 0 && GridManager->CountFilled() > CellCount * 0.6f)
	{
		const FIntPoint Area = GridManager->FindDensestArea();
		return PuzzleManager->UseHolyLight(Area.X, Area.Y);
	}

	int32 BestSlot = -1;
	int32 BestX = 0;
	int32 BestY = 0;
	int32 BestLines = 0;
	int64 BestScore = TNumericLimits<int64>::Min();

	for (int32 Slot = 0; Slot < AGridManager::SlotCount; ++Slot)
	{
		if (GridManager->TraySlotUsed[Slot])
		{
			continue;
		}

		const FPuzzlePieceShape& Shape = GridManager->Tray[Slot];

		for (int32 OriginY = 0; OriginY < GridManager->GridHeight; ++OriginY)
		{
			for (int32 OriginX = 0; OriginX < GridManager->GridWidth; ++OriginX)
			{
				const int32 LinesCleared = GridManager->SimulateLinesCleared(Shape, OriginX, OriginY);
				if (LinesCleared < 0)
				{
					continue; // illegal placement
				}

				int32 Adjacency = 0;
				for (const FIntPoint& Cell : Shape.Cells)
				{
					Adjacency += GridManager->CountFilledNeighbors(OriginX + Cell.X, OriginY + Cell.Y);
				}

				const int64 Score = static_cast<int64>(LinesCleared) * 100000 + Adjacency * 10;
				if (Score > BestScore)
				{
					BestScore = Score;
					BestSlot = Slot;
					BestX = OriginX;
					BestY = OriginY;
					BestLines = LinesCleared;
				}
			}
		}
	}

	if (BestSlot < 0)
	{
		return false;
	}

	// Nothing to clear this turn: stash an awkward big piece in the empty hold slot.
	if (BestLines == 0 && GridManager->TraySlotUsed[AGridManager::ReserveSlot])
	{
		for (int32 Slot = 0; Slot < AGridManager::TraySize; ++Slot)
		{
			if (Slot != BestSlot && !GridManager->TraySlotUsed[Slot] && GridManager->Tray[Slot].Cells.Num() >= 5)
			{
				return PuzzleManager->ParkPiece(Slot);
			}
		}
	}

	return PuzzleManager->TryPlacePiece(BestSlot, BestX, BestY);
}
