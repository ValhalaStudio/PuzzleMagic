#pragma once

#include "CoreMinimal.h"
#include "PuzzleTypes.h"

// Static table of the piece shapes: single, 2x1, 3x1 and 4x1 lines, 2x2 square and
// the 3-block angle, in every rotation. Independent of color; color is assigned when
// a piece is drawn for the tray.
namespace PieceLibrary
{
	inline const TArray<TArray<FIntPoint>>& GetAllShapes()
	{
		static const TArray<TArray<FIntPoint>> Shapes = {
			// Single
			{ {0,0} },
			// 2x1 line
			{ {0,0},{1,0} },
			{ {0,0},{0,1} },
			// 3x1 line
			{ {0,0},{1,0},{2,0} },
			{ {0,0},{0,1},{0,2} },
			// 4x1 line
			{ {0,0},{1,0},{2,0},{3,0} },
			{ {0,0},{0,1},{0,2},{0,3} },
			// 2x2 square
			{ {0,0},{1,0},{0,1},{1,1} },
			// 3-block angle, in its four rotations
			{ {0,0},{1,0},{0,1} },
			{ {0,0},{1,0},{1,1} },
			{ {0,1},{1,1},{0,0} },
			{ {0,0},{0,1},{1,1} },
		};
		return Shapes;
	}

	// Picks the locked, random triangle directions of a piece. Straight shapes, squares and singles
	// point one way as a whole. The 3-block angle is a chain end -> corner -> end, so a route can run
	// through its bend: the first end points at the corner, the corner and the last end point onward.
	inline void AssignRandomDirections(FPuzzlePieceShape& Piece)
	{
		const int32 Count = Piece.Cells.Num();
		if (Count == 3 && Piece.GetWidth() == 2 && Piece.GetHeight() == 2)
		{
			int32 Corner = 0;
			for (int32 I = 0; I < 3; ++I)
			{
				const FIntPoint ToA = Piece.Cells[(I + 1) % 3] - Piece.Cells[I];
				const FIntPoint ToB = Piece.Cells[(I + 2) % 3] - Piece.Cells[I];
				if (FMath::Abs(ToA.X) + FMath::Abs(ToA.Y) == 1 && FMath::Abs(ToB.X) + FMath::Abs(ToB.Y) == 1)
				{
					Corner = I;
				}
			}
			const bool bFlip = FMath::RandBool();
			const int32 From = (Corner + (bFlip ? 2 : 1)) % 3;
			const int32 To = (Corner + (bFlip ? 1 : 2)) % 3;
			const EPuzzleDir Onward = PuzzleTypes::OffsetToDir(Piece.Cells[To] - Piece.Cells[Corner]);
			Piece.Dirs.Init(Onward, 3);
			Piece.Dirs[From] = PuzzleTypes::OffsetToDir(Piece.Cells[Corner] - Piece.Cells[From]);
			return;
		}
		Piece.Dirs.Init(static_cast<EPuzzleDir>(FMath::RandRange(0, 3)), Count);
	}

	inline FPuzzlePieceShape MakeRandomPiece(EPuzzleTileColor Color)
	{
		const TArray<TArray<FIntPoint>>& Shapes = GetAllShapes();
		FPuzzlePieceShape Piece;
		Piece.Cells = Shapes[FMath::RandRange(0, Shapes.Num() - 1)];
		Piece.Color = Color;
		AssignRandomDirections(Piece);
		return Piece;
	}

	inline FPuzzlePieceShape MakeRandomPieceRandomColor()
	{
		static const TArray<EPuzzleTileColor> Palette = {
			EPuzzleTileColor::Red, EPuzzleTileColor::Green, EPuzzleTileColor::Blue,
			EPuzzleTileColor::Yellow, EPuzzleTileColor::Purple
		};
		return MakeRandomPiece(Palette[FMath::RandRange(0, Palette.Num() - 1)]);
	}
}
