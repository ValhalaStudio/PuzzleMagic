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
	Black,
	Purple,
	Red,
	Ash
};

static constexpr int32 PuzzleColorCount = 4;

// The way a tile's triangle points, and so the way a route flows through it. Grid space has +Y up.
UENUM(BlueprintType)
enum class EPuzzleDir : uint8
{
	Up,     // (0, +1)
	Right,  // (+1, 0)
	Down,   // (0, -1)
	Left    // (-1, 0)
};

// Special tiles that appear as the score grows (Play options > Bonus tiles).
UENUM(BlueprintType)
enum class EPuzzleBonus : uint8
{
	None,
	Basic,     // clears when a chain of tiles links it to any side of the board
	Outgoing,  // clears when a route starts at it and leaves the board
	Incoming   // clears when a route from a side of the board ends at it
};

UENUM(BlueprintType)
enum class ERelic : uint8
{
	HolyLight,  // clears a chosen 3x3 area
	Reroll      // replaces the tray pieces
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

	// The triangle direction of each tile, parallel to Cells. Chosen when the piece is made and locked.
	UPROPERTY(BlueprintReadOnly)
	TArray<EPuzzleDir> Dirs;

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
	// The colour of a bonus kind's effects and popups: pumpkin orange, potion magenta.
	inline FLinearColor BonusToColor(EPuzzleBonus Kind)
	{
		switch (Kind)
		{
		case EPuzzleBonus::Basic:    return FLinearColor(1.00f, 0.45f, 0.05f);
		case EPuzzleBonus::Outgoing:
		case EPuzzleBonus::Incoming: return FLinearColor(1.00f, 0.08f, 0.78f);
		default:                     return FLinearColor::White;
		}
	}

	// The dark enamel a bonus tile's emblem sits on.
	inline FLinearColor BonusBaseColor(EPuzzleBonus Kind)
	{
		return Kind == EPuzzleBonus::Basic ? FLinearColor(0.05f, 0.03f, 0.02f) : FLinearColor(0.10f, 0.03f, 0.16f);
	}

	inline FIntPoint DirToOffset(EPuzzleDir Dir)
	{
		switch (Dir)
		{
		case EPuzzleDir::Up:    return FIntPoint(0, 1);
		case EPuzzleDir::Right: return FIntPoint(1, 0);
		case EPuzzleDir::Down:  return FIntPoint(0, -1);
		default:                return FIntPoint(-1, 0);
		}
	}

	inline EPuzzleDir OffsetToDir(const FIntPoint& Offset)
	{
		if (Offset.X > 0) { return EPuzzleDir::Right; }
		if (Offset.X < 0) { return EPuzzleDir::Left; }
		return Offset.Y > 0 ? EPuzzleDir::Up : EPuzzleDir::Down;
	}

	// Bright cartoon palette (linear). Each colour also owns a symbol: see M_ToonTile
	// (Red blood drop, Green skull, Blue moon, Yellow cross, Purple bat).
	inline FLinearColor ToLinearColor(EPuzzleTileColor Color)
	{
		switch (Color)
		{
		case EPuzzleTileColor::Black:  return FLinearColor(0.025f, 0.025f, 0.03f);
		case EPuzzleTileColor::Purple: return FLinearColor(0.42f, 0.10f, 0.78f);
		case EPuzzleTileColor::Red:    return FLinearColor(0.90f, 0.08f, 0.10f);
		case EPuzzleTileColor::Ash:    return FLinearColor(0.20f, 0.21f, 0.24f);
		default:                       return FLinearColor::White;
		}
	}
}
