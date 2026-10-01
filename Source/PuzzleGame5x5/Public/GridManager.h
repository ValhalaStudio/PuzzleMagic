#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleTypes.h"
#include "GridManager.generated.h"

class APuzzleTile;
class UBoxComponent;
class URealtimeMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// One completed route, in flow order.
struct FRouteInfo
{
	TArray<int32> Cells;
	bool bNeighbouring = false; // joins two neighbouring sides rather than opposite ones
	int32 ExtraTiles = 0;       // tiles beyond the route's basic length (the n in the route bonus)
};

// One bonus tile that was cleared, with the chain of tiles that cleared it.
struct FBonusEvent
{
	EPuzzleBonus Kind = EPuzzleBonus::Basic;
	bool bLinked = false;     // an outgoing tile joined to an incoming one
	int32 ExtraTiles = 0;     // chain tiles beyond the straight line from the bonus tile to its nearest side
	TArray<int32> Cells;      // the chain, in flow order, including the bonus tile(s)
	int32 Points = 0;         // filled in by the rules
};

// Outcome of one clear (a placement's route clears, or a Holy Light blast).
struct FClearResult
{
	int32 Lines = 0;          // routes completed
	TArray<FRouteInfo> Routes; // the completed routes, for scoring
	TArray<FBonusEvent> Bonuses; // the bonus tiles cleared by chains
	int32 Cells = 0;          // cells emptied by routes (these score)
	int32 CircuitCells = 0;   // cells emptied by closed circuits (these do not score)
};

// Owns the board state (4x4 up to 8x8), the tray (three pieces + one reserve "hold" slot)
// and all board/tray visuals.
// Landscape layout: board on top, tray row underneath.
UCLASS()
class PUZZLEGAME5X5_API AGridManager : public AActor
{
	GENERATED_BODY()

public:
	AGridManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// The board is GridWidth by GridHeight cells, each 4 to 8. Change it with SetGridSize (then InitBoard).
	static constexpr int32 MinGridSide = 4;
	static constexpr int32 MaxGridSide = 8;
	int32 GridWidth = MaxGridSide;
	int32 GridHeight = MaxGridSide;

	// Resizes the board: rebuilds the frame, slots, tray and collision. Call InitBoard afterwards for a fresh round.
	void SetGridSize(int32 Width, int32 Height);
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

	// Clears every completed route and closed circuit from the board state immediately; the visual
	// pop starts after ClearDelay (so it can wait for a piece still flying in).
	// A route is a chain of tiles, each pointing at the next, that starts on one side of the board
	// and ends with its last tile pointing off the opposite (or a neighbouring) side. A closed circuit is
	// a loop of tiles that point round in a circle, or a chain that leaves through the side it started on.
	FClearResult CheckAndClearLines(float ClearDelay = 0.f);

	// Holy Light: clears the 3x3 area around a cell.
	FClearResult ClearArea(int32 CenterX, int32 CenterY, float Delay = 0.f);

	// Drops a bonus tile of the given kind on a random empty cell (it lands after the clears have popped).
	// False if the board is full.
	bool SpawnBonusTile(EPuzzleBonus Kind, FIntPoint& OutCell);
	int32 CountBonusTiles(EPuzzleBonus Kind) const;

	// Game over: every tile on the board bursts off with physics.
	void CollapseBoard();

	// Centre cell of the 3x3 area holding the most tiles.
	FIntPoint FindDensestArea() const;

	int32 CountFilled() const;
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
	// Number of routes the placement would complete, or -1 if it is illegal.
	int32 SimulateLinesCleared(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY) const;
	int32 CountFilledNeighbors(int32 X, int32 Y) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<URealtimeMeshComponent> BoardMesh;

	// Physics floor for tiles bursting off the board.
	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<UBoxComponent> BoardCollision;

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
	APuzzleTile* SpawnTile(const FVector& BaseLocation, EPuzzleTileColor Color, EPuzzleDir Dir, float Scale);
	// FlowDelay (optional) gives cells of a route the delay at which the stream reaches them; the rest ripple out from Centre.
	void PopCells(const TArray<int32>& Indices, const FVector2D& Centre, float Delay, FClearResult& Result, class APuzzleFX* FX, const TMap<int32, float>* FlowDelay = nullptr);

	UPROPERTY()
	TArray<TObjectPtr<APuzzleTile>> TrayVisuals;
	TArray<int32> TrayVisualSlots;

	UPROPERTY()
	TArray<bool> Filled; // row-major, Y * GridWidth + X

	UPROPERTY()
	TArray<EPuzzleTileColor> CellColors;

	// The triangle direction of the tile on each cell (only meaningful where Filled is true).
	UPROPERTY()
	TArray<EPuzzleDir> CellDirs;

	// The bonus kind of the tile on each cell (an EPuzzleBonus value; 0 = an ordinary tile).
	UPROPERTY()
	TArray<uint8> CellBonus;

	UPROPERTY()
	TArray<TObjectPtr<APuzzleTile>> CellVisuals;

	UPROPERTY()
	TArray<TObjectPtr<APuzzleTile>> GhostTiles;

	FVector LastClearCentroid = FVector::ZeroVector;

	bool IsValidCoord(int32 X, int32 Y) const;

	// Route roles, from where a tile sits and which way it points. A starter sits next to a side of the board and
	// points straight away from it; an ender sits next to a side and points straight at it.
	// Only a starter can begin a route, and a route ends on an ender.
	bool IsRouteStarter(int32 X, int32 Y, EPuzzleDir Dir) const;
	bool IsRouteEnder(int32 X, int32 Y, EPuzzleDir Dir) const;

	// Finds the routes and the cells of closed circuits. A route begins on a starter, follows the tiles and ends on an ender; it leaves
	// the board through the side opposite its starter's side, or through a neighbouring side. A chain of two or more tiles that leaves through
	// the side it started on counts as a closed circuit.
	// Finds the bonus tiles that a chain clears right now: basic, outgoing, incoming and linked pairs.
	// A bonus tile points nowhere: chains end on it, and a chain can leave it through any neighbour.
	void FindBonusEvents(TArray<FBonusEvent>& OutEvents) const;
	void FindRoutes(const TArray<bool>& FilledState, const TArray<EPuzzleDir>& DirState, TArray<FRouteInfo>& OutRoutes, TArray<int32>& OutCircuitCells) const;
};
