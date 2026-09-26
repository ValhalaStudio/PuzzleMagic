#pragma once

#include "CoreMinimal.h"
#include "PuzzleTypes.h"

// Static table of the standard block-puzzle piece shapes (monominoes through
// pentominoes/lines), independent of color; color is assigned when a piece
// is drawn for the tray.
namespace PieceLibrary
{
	inline const TArray<TArray<FIntPoint>>& GetAllShapes()
	{
		static const TArray<TArray<FIntPoint>> Shapes = {
			// Single
			{ {0,0} },
			// Domino
			{ {0,0},{1,0} },
			{ {0,0},{0,1} },
			// Tromino line
			{ {0,0},{1,0},{2,0} },
			{ {0,0},{0,1},{0,2} },
			// Tromino corner (L)
			{ {0,0},{1,0},{0,1} },
			{ {0,0},{1,0},{1,1} },
			{ {0,1},{1,1},{0,0} },
			{ {0,0},{0,1},{1,1} },
			// Square 2x2
			{ {0,0},{1,0},{0,1},{1,1} },
			// Tetromino line (I)
			{ {0,0},{1,0},{2,0},{3,0} },
			{ {0,0},{0,1},{0,2},{0,3} },
			// Tetromino L / J
			{ {0,0},{0,1},{0,2},{1,2} },
			{ {1,0},{1,1},{1,2},{0,2} },
			{ {0,0},{1,0},{2,0},{2,1} },
			{ {0,0},{1,0},{2,0},{0,1} },
			// Tetromino T
			{ {0,0},{1,0},{2,0},{1,1} },
			{ {1,0},{0,1},{1,1},{2,1} },
			// Tetromino S / Z
			{ {1,0},{2,0},{0,1},{1,1} },
			{ {0,0},{1,0},{1,1},{2,1} },
			// Pentomino line (I5)
			{ {0,0},{1,0},{2,0},{3,0},{4,0} },
			{ {0,0},{0,1},{0,2},{0,3},{0,4} },
			// Big L (5-cell)
			{ {0,0},{0,1},{0,2},{1,2},{2,2} },
			// Plus / cross
			{ {1,0},{0,1},{1,1},{2,1},{1,2} },
			// Corner box (3x3 L)
			{ {0,0},{1,0},{2,0},{0,1},{0,2} },
		};
		return Shapes;
	}

	inline FPuzzlePieceShape MakeRandomPiece(EPuzzleTileColor Color)
	{
		const TArray<TArray<FIntPoint>>& Shapes = GetAllShapes();
		FPuzzlePieceShape Piece;
		Piece.Cells = Shapes[FMath::RandRange(0, Shapes.Num() - 1)];
		Piece.Color = Color;
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
