#pragma once

#include "CoreMinimal.h"
#include "PuzzleTypes.generated.h"

UENUM(BlueprintType)
enum class EPuzzleGamepadButton : uint8
{
	DPadUp,
	DPadDown,
	DPadLeft,
	DPadRight,
	Confirm,
	Cancel,
	CyclePiece
};

UENUM(BlueprintType)
enum class EPuzzleTileColor : uint8
{
	Red,
	Green,
	Blue,
	Yellow,
	Purple
};

static constexpr int32 PuzzleColorCount = 5;

UENUM(BlueprintType)
enum class EPlayMode : uint8
{
	Endless,
	Quest
};

UENUM(BlueprintType)
enum class EQuestGoal : uint8
{
	CollectSymbol,  // clear N tiles of one colour/symbol
	ClearLines,     // clear N rows/columns/boxes
	ClearBoxes,     // clear N 3x3 boxes
	BreakStones,    // shatter N gargoyle stones
	ReachScore
};

UENUM(BlueprintType)
enum class ERelic : uint8
{
	HolyLight,  // clears a chosen 3x3 area
	Reroll      // replaces the tray pieces
};

// Natural and unnatural misfortunes that luck can turn aside.
UENUM(BlueprintType)
enum class EOmen : uint8
{
	Lightning,  // a bolt petrifies a tile into a gargoyle stone
	Hex         // a witch's hex steals moves
};

USTRUCT(BlueprintType)
struct FQuestLevel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Number = 1;
	UPROPERTY(BlueprintReadOnly) EQuestGoal Goal = EQuestGoal::ClearLines;
	UPROPERTY(BlueprintReadOnly) int32 Target = 3;
	UPROPERTY(BlueprintReadOnly) EPuzzleTileColor Color = EPuzzleTileColor::Red;
	UPROPERTY(BlueprintReadOnly) int32 Moves = 20;
	// A gargoyle stone appears every N moves (0 = never).
	UPROPERTY(BlueprintReadOnly) int32 StoneInterval = 0;
};

// A piece is a set of occupied cell offsets relative to its own top-left origin.
USTRUCT(BlueprintType)
struct FPuzzlePieceShape
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FIntPoint> Cells;

	UPROPERTY(BlueprintReadOnly)
	EPuzzleTileColor Color = EPuzzleTileColor::Red;

	int32 GetWidth() const
	{
		int32 MaxX = 0;
		for (const FIntPoint& Cell : Cells) { MaxX = FMath::Max(MaxX, Cell.X); }
		return MaxX + 1;
	}

	int32 GetHeight() const
	{
		int32 MaxY = 0;
		for (const FIntPoint& Cell : Cells) { MaxY = FMath::Max(MaxY, Cell.Y); }
		return MaxY + 1;
	}
};

namespace PuzzleTypes
{
	// Bright cartoon palette (linear). Each colour also owns a symbol: see M_ToonTile
	// (Red blood drop, Green skull, Blue moon, Yellow cross, Purple bat).
	inline FLinearColor ToLinearColor(EPuzzleTileColor Color)
	{
		switch (Color)
		{
		case EPuzzleTileColor::Red:    return FLinearColor(0.90f, 0.10f, 0.14f);
		case EPuzzleTileColor::Green:  return FLinearColor(0.16f, 0.66f, 0.18f);
		case EPuzzleTileColor::Blue:   return FLinearColor(0.12f, 0.38f, 0.95f);
		case EPuzzleTileColor::Yellow: return FLinearColor(1.00f, 0.66f, 0.06f);
		case EPuzzleTileColor::Purple: return FLinearColor(0.52f, 0.16f, 0.88f);
		default:                       return FLinearColor::White;
		}
	}
}
