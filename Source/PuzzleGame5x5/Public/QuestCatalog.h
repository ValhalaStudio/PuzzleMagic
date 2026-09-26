#pragma once

#include "CoreMinimal.h"
#include "PuzzleTypes.h"

// Hand-tuned quest levels: goals rotate, move budgets stay tight, and gargoyle
// stones arrive more often as the levels go on.
namespace QuestCatalog
{
	inline const TArray<FQuestLevel>& Levels()
	{
		static const TArray<FQuestLevel> All = []()
		{
			struct FRow { EQuestGoal Goal; int32 Target; EPuzzleTileColor Color; int32 Moves; int32 Stones; };
			const FRow Rows[] = {
				{ EQuestGoal::ClearLines,    3,    EPuzzleTileColor::Red,    20, 0 },
				{ EQuestGoal::CollectSymbol, 10,   EPuzzleTileColor::Red,    20, 0 },
				{ EQuestGoal::ClearBoxes,    2,    EPuzzleTileColor::Red,    22, 0 },
				{ EQuestGoal::ReachScore,    600,  EPuzzleTileColor::Red,    25, 0 },
				{ EQuestGoal::CollectSymbol, 15,   EPuzzleTileColor::Blue,   22, 8 },
				{ EQuestGoal::ClearLines,    8,    EPuzzleTileColor::Red,    24, 8 },
				{ EQuestGoal::BreakStones,   3,    EPuzzleTileColor::Red,    26, 5 },
				{ EQuestGoal::CollectSymbol, 20,   EPuzzleTileColor::Yellow, 25, 7 },
				{ EQuestGoal::ClearBoxes,    4,    EPuzzleTileColor::Red,    26, 7 },
				{ EQuestGoal::ReachScore,    1500, EPuzzleTileColor::Red,    28, 6 },
				{ EQuestGoal::CollectSymbol, 24,   EPuzzleTileColor::Purple, 26, 6 },
				{ EQuestGoal::BreakStones,   6,    EPuzzleTileColor::Red,    30, 4 },
				{ EQuestGoal::ClearLines,    14,   EPuzzleTileColor::Red,    28, 5 },
				{ EQuestGoal::ClearBoxes,    6,    EPuzzleTileColor::Red,    30, 5 },
				{ EQuestGoal::ReachScore,    3000, EPuzzleTileColor::Red,    32, 4 },
			};

			TArray<FQuestLevel> Result;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rows); ++Index)
			{
				FQuestLevel& Level = Result.AddDefaulted_GetRef();
				Level.Number = Index + 1;
				Level.Goal = Rows[Index].Goal;
				Level.Target = Rows[Index].Target;
				Level.Color = Rows[Index].Color;
				Level.Moves = Rows[Index].Moves;
				Level.StoneInterval = Rows[Index].Stones;
			}
			return Result;
		}();
		return All;
	}

	inline const FQuestLevel& Get(int32 Number)
	{
		const TArray<FQuestLevel>& All = Levels();
		return All[FMath::Clamp(Number, 1, All.Num()) - 1];
	}

	inline FString Describe(const FQuestLevel& Level)
	{
		static const TCHAR* Symbols[] = { TEXT("blood drops"), TEXT("skulls"), TEXT("moons"), TEXT("crosses"), TEXT("bats") };
		switch (Level.Goal)
		{
		case EQuestGoal::CollectSymbol: return FString::Printf(TEXT("Clear %d %s"), Level.Target, Symbols[static_cast<int32>(Level.Color)]);
		case EQuestGoal::ClearLines:    return FString::Printf(TEXT("Clear %d lines"), Level.Target);
		case EQuestGoal::ClearBoxes:    return FString::Printf(TEXT("Clear %d boxes"), Level.Target);
		case EQuestGoal::BreakStones:   return FString::Printf(TEXT("Shatter %d gargoyles"), Level.Target);
		case EQuestGoal::ReachScore:    return FString::Printf(TEXT("Score %d points"), Level.Target);
		}
		return FString();
	}
}
