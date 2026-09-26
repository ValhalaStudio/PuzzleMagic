#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleTypes.h"
#include "GridManager.generated.h"

class APuzzleTile;
class UBoxComponent;
class URealtimeMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Outcome of one clear (a placement's line clears, or a Holy Light blast).
struct FClearResult
{
	int32 Lines = 0;          // rows + columns + boxes completed
	int32 Cells = 0;          // cells actually emptied
	int32 Boxes = 0;
	int32 Blessings = 0;      // completed lines whose tiles all share one symbol
	bool bBlessedBox = false; // the glowing blessed box was among the clears
	int32 StonesCracked = 0;
	int32 StonesBroken = 0;
	int32 ColorCounts[PuzzleColorCount] = {};
};

// Owns the 9x9 board state, the tray (three pieces + one reserve "hold" slot),
// gargoyle stones, the blessed box, and all board/tray visuals.
// Portrait layout (iOS): board on top, tray row underneath.
UCLASS()
class PUZZLEGAME5X5_API AGridManager : public AActor
{
	GENERATED_BODY()

public:
	AGridManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	static constexpr int32 GridSize = 9;
	static constexpr int32 BoxSize = 3;
	static constexpr int32 TraySize = 3;      // regular slots 0..2
	static constexpr int32 ReserveSlot = 3;   // the hold slot
	static constexpr int32 SlotCount = 4;

	// How long a placed piece takes to fly from the tray into the board.
	static constexpr float ArriveDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Puzzle")
	float TileSpacing = 90.f;

	void InitBoard();

	bool CanPlacePieceAt(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY) const;

	// FromTraySlot >= 0 makes the new tiles fly in from that tray slot's preview.
	bool PlacePieceAt(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY, int32 FromTraySlot = -1);

	bool CanPieceFitAnywhere(const FPuzzlePieceShape& Shape) const;
	bool CanAnyTrayPieceFit() const;

	void ShowGhostPreview(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY, bool bValid);
	void HideGhostPreview();

	// Holy Light aiming: a rune circle over the 3x3 area around a cell.
	void ShowAreaTarget(int32 CenterX, int32 CenterY);
	void HideAreaTarget();

	// Clears full rows/columns/boxes from the board state immediately; the visual
	// pop starts after ClearDelay (so it can wait for a piece still flying in).
	FClearResult CheckAndClearLines(float ClearDelay = 0.f);

	// Holy Light: clears the 3x3 area around a cell, shattering stones outright.
	FClearResult ClearArea(int32 CenterX, int32 CenterY, float Delay = 0.f);

	// Drops a gargoyle stone on a random empty cell (the fall starts after Delay). False if the board is full.
	bool SpawnCurseStone(FIntPoint& OutCell, float Delay = 0.f);

	// Omens. Lightning prefers a normal tile (it petrifies it); an empty cell gets a stone. (-1,-1) if nothing can be struck.
	FIntPoint PickLightningTarget() const;
	// The bolt lands after Delay. Warded: it breaks on a golden shield above the board and the cell is spared.
	void StrikeLightning(const FIntPoint& Cell, float Delay, bool bWarded);
	// A hex's rune circle over the board (the rules take the moves); warded, gold light burns it away.
	void PlayHex(float Delay, bool bWarded);

	// Game over: every tile on the board bursts off with physics.
	void CollapseBoard();

	// Centre cell of the 3x3 area holding the most tiles (stones count double).
	FIntPoint FindDensestArea() const;

	int32 CountFilled() const;
	int32 GetBlessedBox() const { return BlessedBox; }
	FVector GetLastClearCentroid() const { return LastClearCentroid; }

	// Location of a tile's base (the board surface) at a cell.
	FVector GetWorldLocationForCell(int32 X, int32 Y) const;
	bool WorldLocationToCell(const FVector& WorldLocation, int32& OutX, int32& OutY) const;

	// Z of the plane used to pick cells: mid-height of a resting tile.
	float GetPickPlaneZ() const;
	// Z of the plane used to pick tray pieces.
	float GetTrayPickPlaneZ() const;

	// Occupied tray slot (including the reserve) whose panel contains WorldPoint, or -1.
	int32 FindTraySlotAt(const FVector& WorldPoint) const;
	bool IsOverReserve(const FVector& WorldPoint) const;

	// XY bounds of everything the camera should frame (board + tray).
	FBox2D GetContentBounds() const;

	// SlotCount entries; TraySlotUsed[i] == true means the slot is empty.
	UPROPERTY()
	TArray<FPuzzlePieceShape> Tray;

	UPROPERTY()
	TArray<bool> TraySlotUsed;

	void RefillTrayIfEmpty();
	void ConsumeTraySlot(int32 SlotIndex);
	void RerollTray();

	// Moves a regular tray piece into the reserve (swapping if the reserve is occupied).
	bool ParkPiece(int32 SlotIndex);

	// Centre of a tray slot, at the tray panel's top surface.
	FVector GetTrayAnchorWorldLocation(int32 SlotIndex) const;

	void RefreshTrayVisuals();

	// --- AI/bot support: evaluate a hypothetical placement without mutating the board ---
	int32 SimulateLinesCleared(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY) const;
	int32 CountFilledNeighbors(int32 X, int32 Y) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<URealtimeMeshComponent> BoardMesh;

	// Physics floor for tiles bursting off the board.
	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<UBoxComponent> BoardCollision;

	// Rune circle + light marking the blessed box.
	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<UStaticMeshComponent> BlessedAura;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<UPointLightComponent> BlessedLight;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<UStaticMeshComponent> TargetAura;

	// Ghost preview cache so a drag only respawns ghosts when the target actually changes.
	FIntVector GhostKey = FIntVector(MAX_int32);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MarbleMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BezelMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> AuraMaterial;

	void BuildBoardVisuals();
	void SpawnStoneAt(int32 Index, float Delay, float FallTime, float FallHeight);
	APuzzleTile* SpawnTile(const FVector& BaseLocation, EPuzzleTileColor Color, float Scale);
	void ChooseBlessedBox();
	void UpdateBlessedVisuals();
	bool IsInBlessedBox(int32 X, int32 Y) const;
	bool IsLineBlessed(const TArray<int32>& Cells) const;
	void PopCells(const TArray<int32>& Indices, const FVector2D& Centre, float Delay, FClearResult& Result, class APuzzleFX* FX);

	UPROPERTY()
	TArray<TObjectPtr<APuzzleTile>> TrayVisuals;
	TArray<int32> TrayVisualSlots;

	UPROPERTY()
	TArray<bool> Filled; // row-major, Y * GridSize + X

	UPROPERTY()
	TArray<EPuzzleTileColor> CellColors;

	// 0 = normal tile/empty, 2 = intact gargoyle stone, 1 = cracked stone.
	UPROPERTY()
	TArray<uint8> CellStone;

	UPROPERTY()
	TArray<TObjectPtr<APuzzleTile>> CellVisuals;

	UPROPERTY()
	TArray<TObjectPtr<APuzzleTile>> GhostTiles;

	int32 BlessedBox = -1;
	FVector LastClearCentroid = FVector::ZeroVector;

	bool IsValidCoord(int32 X, int32 Y) const;
	TArray<int32> GetFullRows() const;
	TArray<int32> GetFullColumns() const;
	TArray<int32> GetFullBoxes() const;

	static TArray<int32> ComputeFullRows(const TArray<bool>& FilledState);
	static TArray<int32> ComputeFullColumns(const TArray<bool>& FilledState);
	static TArray<int32> ComputeFullBoxes(const TArray<bool>& FilledState);
};
