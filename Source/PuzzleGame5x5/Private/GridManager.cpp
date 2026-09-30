#include "GridManager.h"
#include "PuzzleTile.h"
#include "PuzzleFX.h"
#include "PieceLibrary.h"
#include "ToonMeshBuilder.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "MeshBuffersUtil.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"
#include "UObject/ConstructorHelpers.h"

namespace BoardLayout
{
	// Stacked slabs, bottom to top: frame, panel, cell slots, tiles.
	constexpr float CellPitch = 90.f; // AGridManager::TileSpacing's default
	constexpr float FrameHeight = 18.f;
	constexpr float PanelHeight = 8.f;
	constexpr float SlotHalf = 41.f;
	constexpr float SlotHeight = 2.f;
	constexpr float TileBaseZ = FrameHeight + PanelHeight + SlotHeight;

	// Tray row below the board (screen-down is world +Y): three pieces + the reserve.
	constexpr float TrayGap = 32.f;
	// The tray sits nearer the camera than the board, so perspective widens it: keep it well inside the frame's width.
	constexpr float TrayPanelHalf = 92.f;
	constexpr float TrayPanelHeight = 14.f;
	constexpr float TraySlotSpacing = 2.f * TrayPanelHalf + 18.f;
	constexpr float TrayMiniScale = 0.38f;

	// Marble surfaces: base colour, vein colour, vein strength, roughness.
	struct FMarble { FLinearColor Base; FLinearColor Vein; float VeinStrength; float Roughness; };
	const FMarble FrameStone { FLinearColor(0.03f, 0.028f, 0.035f), FLinearColor(0.55f, 0.38f, 0.12f), 0.5f, 0.35f };
	const FMarble PanelMarble { FLinearColor(0.012f, 0.012f, 0.015f), FLinearColor(0.35f, 0.33f, 0.3f), 0.35f, 0.06f };
	const FMarble SlotIndigo { FLinearColor(0.04f, 0.035f, 0.11f), FLinearColor(0.4f, 0.35f, 0.65f), 0.4f, 0.15f };
	const FMarble SlotBurgundy { FLinearColor(0.09f, 0.015f, 0.03f), FLinearColor(0.55f, 0.25f, 0.3f), 0.4f, 0.15f };
	// Rougher than the board so the key light doesn't flare off the tray.
	const FMarble TrayStone { FLinearColor(0.025f, 0.022f, 0.03f), FLinearColor(0.4f, 0.3f, 0.15f), 0.3f, 0.6f };
	const FLinearColor ReserveBezel(0.75f, 0.55f, 1.f);

	// The board panel and frame grow with the board (Width cells across, Height cells up).
	inline float PanelHalfX(int32 Width) { return Width * CellPitch * 0.5f + 30.f; }
	inline float PanelHalfY(int32 Height) { return Height * CellPitch * 0.5f + 30.f; }
	inline float FrameHalfX(int32 Width) { return PanelHalfX(Width) + 32.f; }
	inline float FrameHalfY(int32 Height) { return PanelHalfY(Height) + 32.f; }
	inline float TrayCenterY(int32 Height) { return FrameHalfY(Height) + TrayGap + TrayPanelHalf; }
}

AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BoardMesh = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("BoardMesh"));
	BoardMesh->SetupAttachment(RootComponent);
	BoardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoardMesh->SetCastShadow(true);

	BoardCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardCollision"));
	BoardCollision->SetupAttachment(RootComponent);
	BoardCollision->SetBoxExtent(FVector(BoardLayout::FrameHalfX(GridWidth), BoardLayout::FrameHalfY(GridHeight), BoardLayout::TileBaseZ * 0.5f));
	BoardCollision->SetRelativeLocation(FVector(0.f, 0.f, BoardLayout::TileBaseZ * 0.5f));
	BoardCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	TargetAura = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetAura"));
	TargetAura->SetupAttachment(RootComponent);
	TargetAura->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetAura->SetCastShadow(false);
	TargetAura->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleFinder(TEXT("/Game/Materials/M_Marble.M_Marble"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BezelFinder(TEXT("/Game/Materials/M_Bezel.M_Bezel"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AuraFinder(TEXT("/Game/Materials/M_HolyAura.M_HolyAura"));
	MarbleMaterial = MarbleFinder.Object;
	BezelMaterial = BezelFinder.Object;
	AuraMaterial = AuraFinder.Object;
	if (PlaneFinder.Succeeded())
	{
		TargetAura->SetStaticMesh(PlaneFinder.Object);
	}
}

void AGridManager::BeginPlay()
{
	Super::BeginPlay();
	BuildBoardVisuals();
	if (AuraMaterial)
	{
		UMaterialInstanceDynamic* TargetMID = UMaterialInstanceDynamic::Create(AuraMaterial, this);
		TargetMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.92f, 0.65f));
		TargetMID->SetScalarParameterValue(TEXT("Intensity"), 5.f);
		TargetAura->SetMaterial(0, TargetMID);
	}
}

void AGridManager::ShowAreaTarget(int32 CenterX, int32 CenterY)
{
	CenterX = FMath::Clamp(CenterX, 1, FMath::Max(GridWidth - 2, 1));
	CenterY = FMath::Clamp(CenterY, 1, FMath::Max(GridHeight - 2, 1));
	const float Size = 3.f * TileSpacing * 1.25f;
	// Above the tiles so it reads over a full area.
	TargetAura->SetWorldLocation(GetWorldLocationForCell(CenterX, CenterY) + FVector(0.f, 0.f, 2.f * APuzzleTile::HalfHeight + 4.f));
	TargetAura->SetWorldScale3D(FVector(Size / 100.f, Size / 100.f, 1.f));
	TargetAura->SetVisibility(true);
}

void AGridManager::HideAreaTarget()
{
	TargetAura->SetVisibility(false);
}

void AGridManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGridManager::BuildBoardVisuals()
{
	using namespace BoardLayout;

	// Sections are collected, optimized with meshoptimizer and built into one realtime mesh at the end.
	TArray<ToonMesh::FBuffers> Parts;
	TArray<UMaterialInterface*> PartMaterials;
	auto AddSection = [&Parts, &PartMaterials](int32 Section, const ToonMesh::FBuffers& Buffers, UMaterialInterface* Material)
	{
		check(Section == Parts.Num());
		Parts.Add(Buffers);
		PartMaterials.Add(Material);
	};
	auto Marble = [this](const FMarble& Surface) -> UMaterialInterface*
	{
		if (!MarbleMaterial)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MarbleMaterial, this);
		MID->SetVectorParameterValue(TEXT("BaseColor"), Surface.Base);
		MID->SetVectorParameterValue(TEXT("VeinColor"), Surface.Vein);
		MID->SetScalarParameterValue(TEXT("VeinStrength"), Surface.VeinStrength);
		MID->SetScalarParameterValue(TEXT("Roughness"), Surface.Roughness);
		return MID;
	};

	ToonMesh::FBlockParams Frame;
	Frame.HalfExtent = FVector2D(FrameHalfX(GridWidth), FrameHalfY(GridHeight));
	Frame.CornerRadius = 44.f;
	Frame.Height = FrameHeight;
	Frame.Bevel = 8.f;
	AddSection(0, ToonMesh::BuildBlock(Frame), Marble(FrameStone));
	AddSection(1, ToonMesh::BuildOutlineHull(Frame, 5.f), BezelMaterial);

	ToonMesh::FBlockParams PanelBlock;
	PanelBlock.HalfExtent = FVector2D(PanelHalfX(GridWidth), PanelHalfY(GridHeight));
	PanelBlock.CornerRadius = 30.f;
	PanelBlock.Height = PanelHeight;
	PanelBlock.Bevel = 4.f;
	ToonMesh::FBuffers PanelBuffers;
	PanelBuffers.Append(ToonMesh::BuildBlock(PanelBlock), FVector(0.f, 0.f, FrameHeight));
	AddSection(2, PanelBuffers, Marble(PanelMarble));

	// All 64 slots merged into two sections (a checkerboard).
	ToonMesh::FBlockParams Slot;
	Slot.HalfExtent = FVector2D(SlotHalf, SlotHalf);
	Slot.CornerRadius = 12.f;
	Slot.Height = SlotHeight;
	Slot.Bevel = 1.5f;
	Slot.CornerSegments = 3;
	Slot.BevelSegments = 1;
	const ToonMesh::FBuffers SlotBlock = ToonMesh::BuildBlock(Slot);
	ToonMesh::FBuffers LightSlots, DarkSlots;
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			FVector Offset = GetWorldLocationForCell(X, Y) - GetActorLocation();
			Offset.Z = FrameHeight + PanelHeight;
			const bool bLightSlot = (X + Y) % 2 == 0;
			(bLightSlot ? LightSlots : DarkSlots).Append(SlotBlock, Offset);
		}
	}
	AddSection(3, LightSlots, Marble(SlotIndigo));
	AddSection(4, DarkSlots, Marble(SlotBurgundy));

	ToonMesh::FBlockParams TrayBlock;
	TrayBlock.HalfExtent = FVector2D(TrayPanelHalf, TrayPanelHalf);
	TrayBlock.CornerRadius = 28.f;
	TrayBlock.Height = TrayPanelHeight;
	TrayBlock.Bevel = 6.f;
	const ToonMesh::FBuffers TrayBody = ToonMesh::BuildBlock(TrayBlock);
	const ToonMesh::FBuffers TrayHull = ToonMesh::BuildOutlineHull(TrayBlock, 4.f);
	ToonMesh::FBuffers TrayBodies, TrayHulls, ReserveHull;
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		FVector Offset = GetTrayAnchorWorldLocation(SlotIndex) - GetActorLocation();
		Offset.Z = 0.f;
		TrayBodies.Append(TrayBody, Offset);
		(SlotIndex == ReserveSlot ? ReserveHull : TrayHulls).Append(TrayHull, Offset);
	}
	AddSection(5, TrayBodies, Marble(TrayStone));
	AddSection(6, TrayHulls, BezelMaterial);

	// The reserve slot gets a violet-silver bezel so it reads as different from the three pieces.
	UMaterialInterface* ReserveMaterial = BezelMaterial;
	if (BezelMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BezelMaterial, this);
		MID->SetVectorParameterValue(TEXT("BaseColor"), ReserveBezel);
		ReserveMaterial = MID;
	}
	AddSection(7, ReserveHull, ReserveMaterial);

	TArray<const ToonMesh::FBuffers*> Views;
	for (ToonMesh::FBuffers& Part : Parts)
	{
		MeshBuffers::Optimize(Part);
		Views.Add(&Part);
	}
	BoardMesh->SetRealtimeMesh(MeshBuffers::BuildRealtimeMesh(BoardMesh, Views));
	for (int32 Section = 0; Section < PartMaterials.Num(); ++Section)
	{
		BoardMesh->SetMaterial(Section, PartMaterials[Section]);
	}
}

APuzzleTile* AGridManager::SpawnTile(const FVector& BaseLocation, EPuzzleTileColor Color, EPuzzleDir Dir, float Scale)
{
	if (!GetWorld())
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector Location = BaseLocation + FVector(0.f, 0.f, APuzzleTile::HalfHeight * Scale);
	APuzzleTile* Tile = GetWorld()->SpawnActor<APuzzleTile>(APuzzleTile::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
	if (Tile)
	{
		Tile->SetActorScale3D(FVector(Scale));
		Tile->SetTileColor(Color);
		Tile->SetDirection(Dir);
	}
	return Tile;
}

void AGridManager::InitBoard()
{
	for (APuzzleTile* Tile : CellVisuals)
	{
		if (Tile) { Tile->Destroy(); }
	}
	HideGhostPreview();

	const int32 NumCells = GridWidth * GridHeight;
	Filled.Init(false, NumCells);
	CellColors.Init(EPuzzleTileColor::Red, NumCells);
	CellDirs.Init(EPuzzleDir::Up, NumCells);
	CellBonus.Init(0, NumCells);
	CellVisuals.Init(nullptr, NumCells);

	Tray.Reset();
	Tray.SetNum(SlotCount);
	TraySlotUsed.Init(true, SlotCount);
	RefillTrayIfEmpty();
	RefreshTrayVisuals();
}

void AGridManager::SetGridSize(int32 Width, int32 Height)
{
	GridWidth = FMath::Clamp(Width, MinGridSide, MaxGridSide);
	GridHeight = FMath::Clamp(Height, MinGridSide, MaxGridSide);
	BoardCollision->SetBoxExtent(FVector(BoardLayout::FrameHalfX(GridWidth), BoardLayout::FrameHalfY(GridHeight), BoardLayout::TileBaseZ * 0.5f));
	BuildBoardVisuals();
}

bool AGridManager::IsValidCoord(int32 X, int32 Y) const
{
	return X >= 0 && X < GridWidth && Y >= 0 && Y < GridHeight;
}

bool AGridManager::CanPlacePieceAt(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY) const
{
	for (const FIntPoint& Cell : Shape.Cells)
	{
		const int32 X = OriginX + Cell.X;
		const int32 Y = OriginY + Cell.Y;
		if (!IsValidCoord(X, Y) || Filled[Y * GridWidth + X])
		{
			return false;
		}
	}
	return true;
}

bool AGridManager::PlacePieceAt(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY, int32 FromTraySlot)
{
	if (!CanPlacePieceAt(Shape, OriginX, OriginY) || !GetWorld())
	{
		return false;
	}

	// Tray previews are spawned in Shape.Cells order, so preview i matches cell i.
	TArray<APuzzleTile*> FromTiles;
	for (int32 I = 0; I < TrayVisuals.Num(); ++I)
	{
		if (TrayVisualSlots[I] == FromTraySlot && TrayVisuals[I])
		{
			FromTiles.Add(TrayVisuals[I]);
		}
	}

	for (int32 CellIndex = 0; CellIndex < Shape.Cells.Num(); ++CellIndex)
	{
		const int32 X = OriginX + Shape.Cells[CellIndex].X;
		const int32 Y = OriginY + Shape.Cells[CellIndex].Y;
		const int32 Index = Y * GridWidth + X;

		Filled[Index] = true;
		CellColors[Index] = Shape.Color;
		CellDirs[Index] = Shape.Dirs.IsValidIndex(CellIndex) ? Shape.Dirs[CellIndex] : EPuzzleDir::Up;

		APuzzleTile* Tile = SpawnTile(GetWorldLocationForCell(X, Y), Shape.Color, CellDirs[Index], 1.f);
		if (!Tile)
		{
			continue;
		}
		Tile->MoveToPosition(X, Y);
		CellVisuals[Index] = Tile;

		if (FromTiles.IsValidIndex(CellIndex))
		{
			const APuzzleTile* From = FromTiles[CellIndex];
			Tile->PlayArrive(From->GetActorLocation(), From->GetActorScale3D().X, ArriveDuration, 170.f, CellIndex * 0.02f);
		}
		else
		{
			Tile->PlayArrive(Tile->GetActorLocation() + FVector(0.f, 0.f, 240.f), 1.f, 0.3f, 0.f);
		}
	}

	return true;
}

bool AGridManager::CanPieceFitAnywhere(const FPuzzlePieceShape& Shape) const
{
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			if (CanPlacePieceAt(Shape, X, Y))
			{
				return true;
			}
		}
	}
	return false;
}

bool AGridManager::CanAnyTrayPieceFit() const
{
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (!TraySlotUsed[SlotIndex] && CanPieceFitAnywhere(Tray[SlotIndex]))
		{
			return true;
		}
	}
	return false;
}

void AGridManager::ShowGhostPreview(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY, bool bValid)
{
	// Same piece at the same spot as last frame: nothing to respawn.
	const FIntVector Key(OriginX, OriginY, Shape.Cells.Num() * 1000 + static_cast<int32>(Shape.Color) * 100 + (Shape.Dirs.Num() > 0 ? static_cast<int32>(Shape.Dirs[0]) * 10 : 0) + (bValid ? 1 : 0));
	if (Key == GhostKey && GhostTiles.Num() > 0)
	{
		return;
	}
	HideGhostPreview();
	GhostKey = Key;

	for (int32 CellIndex = 0; CellIndex < Shape.Cells.Num(); ++CellIndex)
	{
		const FIntPoint& Cell = Shape.Cells[CellIndex];
		const int32 X = OriginX + Cell.X;
		const int32 Y = OriginY + Cell.Y;
		if (!IsValidCoord(X, Y))
		{
			continue;
		}

		const EPuzzleDir Dir = Shape.Dirs.IsValidIndex(CellIndex) ? Shape.Dirs[CellIndex] : EPuzzleDir::Up;
		if (APuzzleTile* Ghost = SpawnTile(GetWorldLocationForCell(X, Y) + FVector(0.f, 0.f, 10.f), Shape.Color, Dir, 1.f))
		{
			Ghost->SetGhost(bValid);
			GhostTiles.Add(Ghost);
		}
	}
}

void AGridManager::HideGhostPreview()
{
	for (APuzzleTile* Ghost : GhostTiles)
	{
		if (Ghost) { Ghost->Destroy(); }
	}
	GhostTiles.Reset();
	GhostKey = FIntVector(MAX_int32);
}

void AGridManager::FindRoutes(const TArray<bool>& FilledState, const TArray<EPuzzleDir>& DirState, TArray<FRouteInfo>& OutRoutes, TArray<int32>& OutCircuitCells) const
{
	OutRoutes.Reset();
	OutCircuitCells.Reset();
	const int32 NumCells = GridWidth * GridHeight;

	// The cell a tile points at: -1 if that is off the board, -2 if it is empty.
	auto Next = [&](int32 Index) -> int32
	{
		// Bonus tiles point nowhere: a chain that reaches one ends there.
		if (CellBonus.IsValidIndex(Index) && CellBonus[Index] != 0)
		{
			return -2;
		}
		const FIntPoint Offset = PuzzleTypes::DirToOffset(DirState[Index]);
		const int32 NX = Index % GridWidth + Offset.X;
		const int32 NY = Index / GridWidth + Offset.Y;
		if (NX < 0 || NX >= GridWidth || NY < 0 || NY >= GridHeight)
		{
			return -1;
		}
		const int32 Target = NY * GridWidth + NX;
		return FilledState[Target] ? Target : -2;
	};

	// Closed circuits: every tile has one outgoing arrow, so a loop shows up as a walk that meets itself.
	TArray<uint8> Seen;
	Seen.Init(0, NumCells);
	TSet<int32> CircuitSet;
	for (int32 Start = 0; Start < NumCells; ++Start)
	{
		if (!FilledState[Start] || Seen[Start] != 0)
		{
			continue;
		}
		TArray<int32> Walk;
		int32 Cur = Start;
		while (Cur >= 0 && Seen[Cur] == 0)
		{
			Seen[Cur] = 1;
			Walk.Add(Cur);
			Cur = Next(Cur);
		}
		if (Cur >= 0 && Seen[Cur] == 1)
		{
			for (int32 I = Walk.Find(Cur); I < Walk.Num(); ++I)
			{
				CircuitSet.Add(Walk[I]);
			}
		}
		for (int32 Index : Walk)
		{
			Seen[Index] = 2;
		}
	}

	// Walk from every tile on the board's edge until a tile points off the board. The sides are
	// Left, Right, Bottom, Top (0 to 3); the side a chain leaves through is set by its last tile.
	enum { SideLeft, SideRight, SideBottom, SideTop };
	auto Opposite = [](int32 Side) { return Side ^ 1; };

	TArray<FRouteInfo> Candidates;
	TArray<TArray<int32>> SameSideLoops;
	for (int32 Start = 0; Start < NumCells; ++Start)
	{
		const int32 SX = Start % GridWidth;
		const int32 SY = Start / GridWidth;
		if (!FilledState[Start] || (SX != 0 && SX != GridWidth - 1 && SY != 0 && SY != GridHeight - 1))
		{
			continue;
		}
		TArray<int32> Path;
		int32 Cur = Start;
		bool bLeft = false;
		while (Path.Num() <= NumCells)
		{
			Path.Add(Cur);
			const int32 Target = Next(Cur);
			if (Target == -1)
			{
				bLeft = true;
				break;
			}
			if (Target < 0)
			{
				break;
			}
			Cur = Target;
		}
		// A lone tile pointing straight out of the edge it sits on is nothing.
		if (!bLeft || Path.Num() < 2)
		{
			continue;
		}

		int32 ExitSide = SideLeft;
		switch (DirState[Path.Last()])
		{
		case EPuzzleDir::Up:    ExitSide = SideTop; break;
		case EPuzzleDir::Right: ExitSide = SideRight; break;
		case EPuzzleDir::Down:  ExitSide = SideBottom; break;
		case EPuzzleDir::Left:  ExitSide = SideLeft; break;
		}
		TArray<int32, TInlineAllocator<2>> StartSides;
		if (SX == 0) { StartSides.Add(SideLeft); }
		if (SX == GridWidth - 1) { StartSides.Add(SideRight); }
		if (SY == 0) { StartSides.Add(SideBottom); }
		if (SY == GridHeight - 1) { StartSides.Add(SideTop); }

		bool bOpposite = false;
		bool bNeighbouring = false;
		for (int32 Side : StartSides)
		{
			bOpposite |= ExitSide == Opposite(Side);
			bNeighbouring |= ExitSide != Side && ExitSide != Opposite(Side);
		}

		if (bOpposite || bNeighbouring)
		{
			FRouteInfo Info;
			Info.bNeighbouring = !bOpposite;
			// Opposite sides: tiles beyond the straight line across (the board's width or height). Neighbouring sides: beyond 2 tiles.
			const int32 Across = (ExitSide == SideLeft || ExitSide == SideRight) ? GridWidth : GridHeight;
			Info.ExtraTiles = FMath::Max(0, Path.Num() - (bOpposite ? Across : 2));
			Info.Cells = MoveTemp(Path);
			Candidates.Add(MoveTemp(Info));
		}
		else
		{
			// Looping back out of the side it started on.
			SameSideLoops.Add(MoveTemp(Path));
		}
	}

	// A route that starts partway along another route is just its tail: keep the longer one.
	TSet<int32> RouteCells;
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		bool bTail = false;
		for (int32 J = 0; J < Candidates.Num() && !bTail; ++J)
		{
			bTail = I != J && Candidates[J].Cells.Find(Candidates[I].Cells[0]) > 0;
		}
		if (!bTail)
		{
			RouteCells.Append(Candidates[I].Cells);
			OutRoutes.Add(Candidates[I]);
		}
	}

	// Loops back to the starting side clear as closed circuits, unless a scoring route already owns the tile.
	for (const TArray<int32>& Loop : SameSideLoops)
	{
		CircuitSet.Append(Loop);
	}
	for (int32 Index : CircuitSet)
	{
		if (!RouteCells.Contains(Index))
		{
			OutCircuitCells.Add(Index);
		}
	}
}

int32 AGridManager::SimulateLinesCleared(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY) const
{
	if (!CanPlacePieceAt(Shape, OriginX, OriginY))
	{
		return -1;
	}

	TArray<bool> HypotheticalFilled = Filled;
	TArray<EPuzzleDir> HypotheticalDirs = CellDirs;
	for (int32 CellIndex = 0; CellIndex < Shape.Cells.Num(); ++CellIndex)
	{
		const int32 Index = (OriginY + Shape.Cells[CellIndex].Y) * GridWidth + (OriginX + Shape.Cells[CellIndex].X);
		HypotheticalFilled[Index] = true;
		HypotheticalDirs[Index] = Shape.Dirs.IsValidIndex(CellIndex) ? Shape.Dirs[CellIndex] : EPuzzleDir::Up;
	}

	TArray<FRouteInfo> Routes;
	TArray<int32> Circuits;
	FindRoutes(HypotheticalFilled, HypotheticalDirs, Routes, Circuits);
	return Routes.Num();
}

int32 AGridManager::CountFilledNeighbors(int32 X, int32 Y) const
{
	int32 Count = 0;
	static const FIntPoint Offsets[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
	for (const FIntPoint& Offset : Offsets)
	{
		const int32 NX = X + Offset.X;
		const int32 NY = Y + Offset.Y;
		if (IsValidCoord(NX, NY) && Filled[NY * GridWidth + NX])
		{
			++Count;
		}
	}
	return Count;
}

int32 AGridManager::CountFilled() const
{
	int32 Count = 0;
	for (bool bFilled : Filled)
	{
		Count += bFilled ? 1 : 0;
	}
	return Count;
}

void AGridManager::PopCells(const TArray<int32>& Indices, const FVector2D& Centre, float Delay, FClearResult& Result, APuzzleFX* FX)
{
	for (int32 Index : Indices)
	{
		APuzzleTile* Tile = CellVisuals[Index];
		const float Ripple = FVector2D::Distance(FVector2D(Index % GridWidth, Index / GridWidth), Centre) * 0.05f;
		const float PopDelay = Delay + 0.1f + Ripple;

		Filled[Index] = false;
		CellBonus[Index] = 0;
		CellVisuals[Index] = nullptr;

		if (!Tile)
		{
			continue;
		}
		Tile->PlayClearEffectAndDestroy(PopDelay, 1.f);

		if (FX)
		{
			const FLinearColor Color = PuzzleTypes::ToLinearColor(CellColors[Index]) * 1.4f + FLinearColor(0.25f, 0.25f, 0.25f);
			const FVector Top = Tile->GetActorLocation() + FVector(0.f, 0.f, APuzzleTile::HalfHeight + 4.f);
			for (int32 S = 0; S < 3; ++S)
			{
				FX->AddSparkle(Top + FVector(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), 0.f), Color, PopDelay + 0.1f);
			}
			FX->AddSparkle(Top, FLinearColor(1.f, 0.95f, 0.8f), PopDelay + 0.12f);
		}
	}
}

void AGridManager::FindBonusEvents(TArray<FBonusEvent>& OutEvents) const
{
	OutEvents.Reset();
	const int32 NumCells = GridWidth * GridHeight;
	auto IsBonus = [&](int32 Index) { return CellBonus.IsValidIndex(Index) && CellBonus[Index] != 0; };
	auto KindOf = [&](int32 Index) { return static_cast<EPuzzleBonus>(CellBonus[Index]); };

	// The cell a tile points at: -1 if that is off the board, -2 if it is empty. Bonus tiles point nowhere,
	// so a chain that reaches one ends on it.
	auto Next = [&](int32 Index) -> int32
	{
		if (IsBonus(Index))
		{
			return -2;
		}
		const FIntPoint Offset = PuzzleTypes::DirToOffset(CellDirs[Index]);
		const int32 NX = Index % GridWidth + Offset.X;
		const int32 NY = Index / GridWidth + Offset.Y;
		if (NX < 0 || NX >= GridWidth || NY < 0 || NY >= GridHeight)
		{
			return -1;
		}
		const int32 Target = NY * GridWidth + NX;
		return Filled[Target] ? Target : -2;
	};
	// Follows the arrows from a tile; true if the chain leaves the board.
	auto WalkForward = [&](int32 Start, TArray<int32>& Path) -> bool
	{
		Path.Reset();
		int32 Cur = Start;
		while (Path.Num() <= NumCells)
		{
			Path.Add(Cur);
			const int32 Target = Next(Cur);
			if (Target == -1)
			{
				return true;
			}
			if (Target < 0)
			{
				return false;
			}
			Cur = Target;
		}
		return false;
	};

	TArray<int32> Bonuses;
	TArray<TArray<int32>> EdgeChains;
	for (int32 Index = 0; Index < NumCells; ++Index)
	{
		if (!Filled[Index])
		{
			continue;
		}
		if (IsBonus(Index))
		{
			Bonuses.Add(Index);
			continue;
		}
		const int32 X = Index % GridWidth;
		const int32 Y = Index / GridWidth;
		if (X == 0 || X == GridWidth - 1 || Y == 0 || Y == GridHeight - 1)
		{
			TArray<int32> Chain;
			WalkForward(Index, Chain);
			EdgeChains.Add(MoveTemp(Chain));
		}
	}
	if (Bonuses.Num() == 0)
	{
		return;
	}

	// A bonus tile with no arrow sends a chain out through any of its four neighbours.
	static const FIntPoint Around[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
	auto Neighbour = [&](int32 Index, int32 Side) -> int32
	{
		const int32 NX = Index % GridWidth + Around[Side].X;
		const int32 NY = Index / GridWidth + Around[Side].Y;
		return (NX >= 0 && NX < GridWidth && NY >= 0 && NY < GridHeight && Filled[NY * GridWidth + NX]) ? NY * GridWidth + NX : -1;
	};

	TSet<int32> Used;

	// Linked: an outgoing tile whose chain runs into an incoming tile (or sits right next to it).
	for (int32 Index : Bonuses)
	{
		if (KindOf(Index) != EPuzzleBonus::Outgoing)
		{
			continue;
		}
		for (int32 Side = 0; Side < 4 && !Used.Contains(Index); ++Side)
		{
			const int32 N = Neighbour(Index, Side);
			if (N < 0)
			{
				continue;
			}
			TArray<int32> Path;
			if (IsBonus(N))
			{
				Path.Add(N);
			}
			else
			{
				WalkForward(N, Path);
			}
			const int32 Last = Path.Last();
			if (IsBonus(Last) && KindOf(Last) == EPuzzleBonus::Incoming && !Used.Contains(Last))
			{
				FBonusEvent Event;
				Event.Kind = EPuzzleBonus::Outgoing;
				Event.bLinked = true;
				Event.Cells.Add(Index);
				Event.Cells.Append(Path);
				Used.Add(Index);
				Used.Add(Last);
				OutEvents.Add(MoveTemp(Event));
			}
		}
	}

	// The rest: each clears on its own chain out of it, or into it, reaching a side of the board.
	for (int32 Index : Bonuses)
	{
		if (Used.Contains(Index))
		{
			continue;
		}
		const EPuzzleBonus Kind = KindOf(Index);
		const int32 X = Index % GridWidth;
		const int32 Y = Index / GridWidth;
		const int32 Straight = FMath::Min(FMath::Min(X, GridWidth - 1 - X), FMath::Min(Y, GridHeight - 1 - Y)) + 1;

		TArray<int32> Best;
		if (Kind == EPuzzleBonus::Basic || Kind == EPuzzleBonus::Outgoing)
		{
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 N = Neighbour(Index, Side);
				TArray<int32> Path;
				if (N >= 0 && !IsBonus(N) && WalkForward(N, Path) && Path.Num() + 1 > Best.Num())
				{
					Best.Reset();
					Best.Add(Index);
					Best.Append(Path);
				}
			}
		}
		if (Kind == EPuzzleBonus::Basic || Kind == EPuzzleBonus::Incoming)
		{
			for (const TArray<int32>& Chain : EdgeChains)
			{
				const int32 At = Chain.Find(Index);
				if (At >= 1 && At + 1 > Best.Num())
				{
					Best.Reset();
					for (int32 I = 0; I <= At; ++I)
					{
						Best.Add(Chain[I]);
					}
				}
			}
		}
		if (Best.Num() > 0)
		{
			FBonusEvent Event;
			Event.Kind = Kind;
			Event.ExtraTiles = FMath::Max(0, Best.Num() - Straight);
			Event.Cells = MoveTemp(Best);
			OutEvents.Add(MoveTemp(Event));
		}
	}
}

int32 AGridManager::CountBonusTiles(EPuzzleBonus Kind) const
{
	int32 Count = 0;
	for (uint8 Value : CellBonus)
	{
		Count += Value == static_cast<uint8>(Kind) ? 1 : 0;
	}
	return Count;
}

bool AGridManager::SpawnBonusTile(EPuzzleBonus Kind, FIntPoint& OutCell)
{
	TArray<int32> Empty;
	for (int32 Index = 0; Index < Filled.Num(); ++Index)
	{
		if (!Filled[Index])
		{
			Empty.Add(Index);
		}
	}
	if (Empty.Num() == 0)
	{
		return false;
	}

	const int32 Index = Empty[FMath::RandRange(0, Empty.Num() - 1)];
	const int32 X = Index % GridWidth;
	const int32 Y = Index / GridWidth;
	OutCell = FIntPoint(X, Y);
	Filled[Index] = true;
	CellColors[Index] = EPuzzleTileColor::Red;
	CellDirs[Index] = static_cast<EPuzzleDir>(FMath::RandRange(0, 3));
	CellBonus[Index] = static_cast<uint8>(Kind);

	if (APuzzleTile* Tile = SpawnTile(GetWorldLocationForCell(X, Y), EPuzzleTileColor::Red, CellDirs[Index], 1.f))
	{
		Tile->SetBonus(Kind);
		Tile->MoveToPosition(X, Y);
		// It drops in once the clears of the move that earned it have popped.
		Tile->PlayArrive(Tile->GetActorLocation() + FVector(0.f, 0.f, 420.f), 1.25f, 0.45f, 0.f, ArriveDuration + 0.6f);
		CellVisuals[Index] = Tile;
	}
	return true;
}

FClearResult AGridManager::CheckAndClearLines(float ClearDelay)
{
	FClearResult Result;
	TArray<FRouteInfo> Routes;
	TArray<int32> Circuits;
	FindRoutes(Filled, CellDirs, Routes, Circuits);
	TArray<FBonusEvent> Bonuses;
	FindBonusEvents(Bonuses);

	Result.Lines = Routes.Num();
	Result.Routes = Routes;
	Result.Bonuses = Bonuses;
	if (Routes.Num() == 0 && Circuits.Num() == 0 && Bonuses.Num() == 0)
	{
		return Result;
	}

	TSet<int32> RouteCells;
	for (const FRouteInfo& Info : Routes)
	{
		RouteCells.Append(Info.Cells);
	}
	TSet<int32> CellsToClear = RouteCells;
	CellsToClear.Append(Circuits);
	for (const FBonusEvent& Event : Bonuses)
	{
		CellsToClear.Append(Event.Cells);
	}
	Result.Cells = RouteCells.Num();
	Result.CircuitCells = Circuits.Num();

	FVector2D CentroidCell = FVector2D::ZeroVector;
	for (int32 Index : CellsToClear)
	{
		CentroidCell += FVector2D(Index % GridWidth, Index / GridWidth);
	}
	CentroidCell /= CellsToClear.Num();
	LastClearCentroid = GetWorldLocationForCell(0, 0) + FVector(CentroidCell.X * TileSpacing, -CentroidCell.Y * TileSpacing, APuzzleTile::HalfHeight * 2.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	APuzzleFX* FX = GetWorld() ? GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), LastClearCentroid, FRotator::ZeroRotator, SpawnParams) : nullptr;

	// A glow runs along each route tile to tile (gold for routes, the bonus tile's colour for bonus chains);
	// closed circuits get a violet ring on every tile.
	const float StripZ = GetPickPlaneZ() + APuzzleTile::HalfHeight + 6.f;
	if (FX)
	{
		auto GlowAlong = [&](const TArray<int32>& Chain, const FLinearColor& Color)
		{
			for (int32 Step = 0; Step + 1 < Chain.Num(); ++Step)
			{
				const FVector From = GetWorldLocationForCell(Chain[Step] % GridWidth, Chain[Step] / GridWidth);
				const FVector To = GetWorldLocationForCell(Chain[Step + 1] % GridWidth, Chain[Step + 1] / GridWidth);
				const FVector Middle = (From + To) * 0.5f;
				const bool bHorizontal = Chain[Step] / GridWidth == Chain[Step + 1] / GridWidth;
				FX->AddStrip(FVector(Middle.X, Middle.Y, StripZ), FVector2D(TileSpacing * 1.15f, 70.f), bHorizontal ? 0.f : 90.f, Color, ClearDelay + Step * 0.03f);
			}
		};
		for (const FRouteInfo& Info : Routes)
		{
			GlowAlong(Info.Cells, FLinearColor(1.0f, 0.75f, 0.3f));
		}
		for (const FBonusEvent& Event : Bonuses)
		{
			GlowAlong(Event.Cells, PuzzleTypes::BonusToColor(Event.Kind) * 1.6f);
		}
		for (int32 Index : Circuits)
		{
			const FVector At = GetWorldLocationForCell(Index % GridWidth, Index / GridWidth);
			FX->AddRing(FVector(At.X, At.Y, StripZ), 70.f, FLinearColor(0.9f, 0.45f, 1.f), ClearDelay);
		}
		if (Result.Lines + Bonuses.Num() >= 2)
		{
			FX->AddRing(FVector(LastClearCentroid.X, LastClearCentroid.Y, StripZ), 420.f, FLinearColor(1.f, 0.85f, 0.4f), ClearDelay + 0.1f);
		}
	}

	PopCells(CellsToClear.Array(), CentroidCell, ClearDelay, Result, FX);
	return Result;
}

FClearResult AGridManager::ClearArea(int32 CenterX, int32 CenterY, float Delay)
{
	FClearResult Result;
	CenterX = FMath::Clamp(CenterX, 1, FMath::Max(GridWidth - 2, 1));
	CenterY = FMath::Clamp(CenterY, 1, FMath::Max(GridHeight - 2, 1));

	TArray<int32> Cells;
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			const int32 Index = (CenterY + DY) * GridWidth + CenterX + DX;
			if (Filled[Index])
			{
				Cells.Add(Index);
			}
		}
	}

	LastClearCentroid = GetWorldLocationForCell(CenterX, CenterY) + FVector(0.f, 0.f, APuzzleTile::HalfHeight * 2.f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	APuzzleFX* FX = GetWorld() ? GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), LastClearCentroid, FRotator::ZeroRotator, SpawnParams) : nullptr;
	if (FX)
	{
		const FVector RingCenter(LastClearCentroid.X, LastClearCentroid.Y, GetPickPlaneZ() + APuzzleTile::HalfHeight + 8.f);
		const FLinearColor Holy = FLinearColor(1.f, 0.85f, 0.45f) * 2.5f;
		FX->AddRing(RingCenter, 180.f, Holy, Delay);
		FX->AddRing(RingCenter, 320.f, Holy, Delay + 0.12f);
		FX->AddStrip(RingCenter, FVector2D(300.f, 300.f), 0.f, Holy, Delay);
		FX->AddStrip(RingCenter, FVector2D(300.f, 300.f), 90.f, Holy, Delay);
		for (int32 S = 0; S < 14; ++S)
		{
			FX->AddSparkle(RingCenter + FVector(FMath::FRandRange(-140.f, 140.f), FMath::FRandRange(-140.f, 140.f), 0.f), FLinearColor(1.f, 0.9f, 0.6f), Delay + FMath::FRandRange(0.f, 0.3f));
		}
	}

	Result.Cells = Cells.Num();
	PopCells(Cells, FVector2D(CenterX, CenterY), Delay, Result, FX);
	return Result;
}

FIntPoint AGridManager::FindDensestArea() const
{
	FIntPoint Best(GridWidth / 2, GridHeight / 2);
	int32 BestWeight = -1;
	for (int32 CY = 1; CY < GridHeight - 1; ++CY)
	{
		for (int32 CX = 1; CX < GridWidth - 1; ++CX)
		{
			int32 Weight = 0;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const int32 Index = (CY + DY) * GridWidth + CX + DX;
					Weight += Filled[Index] ? 1 : 0;
				}
			}
			if (Weight > BestWeight)
			{
				BestWeight = Weight;
				Best = FIntPoint(CX, CY);
			}
		}
	}
	return Best;
}

void AGridManager::CollapseBoard()
{
	HideGhostPreview();
	for (int32 Index = 0; Index < CellVisuals.Num(); ++Index)
	{
		if (APuzzleTile* Tile = CellVisuals[Index])
		{
			const int32 Row = Index / GridWidth;
			Tile->PlayClearEffectAndDestroy(0.25f + (GridHeight - 1 - Row) * 0.06f + FMath::FRandRange(0.f, 0.05f), 0.8f);
			CellVisuals[Index] = nullptr;
		}
	}
}

// Cell +Y maps to world -Y: the camera looks toward -Y, so this makes cell +X
// screen-right and cell +Y screen-up (Unreal is left-handed, so one axis must flip).
FVector AGridManager::GetWorldLocationForCell(int32 X, int32 Y) const
{
	const float CenterX = (GridWidth - 1) * 0.5f;
	const float CenterY = (GridHeight - 1) * 0.5f;
	return GetActorLocation() + FVector((X - CenterX) * TileSpacing, -(Y - CenterY) * TileSpacing, BoardLayout::TileBaseZ);
}

bool AGridManager::WorldLocationToCell(const FVector& WorldLocation, int32& OutX, int32& OutY) const
{
	const float CenterX = (GridWidth - 1) * 0.5f;
	const float CenterY = (GridHeight - 1) * 0.5f;
	const FVector Local = WorldLocation - GetActorLocation();

	OutX = FMath::RoundToInt(Local.X / TileSpacing + CenterX);
	OutY = FMath::RoundToInt(-Local.Y / TileSpacing + CenterY);
	return IsValidCoord(OutX, OutY);
}

float AGridManager::GetPickPlaneZ() const
{
	return GetActorLocation().Z + BoardLayout::TileBaseZ + APuzzleTile::HalfHeight;
}

float AGridManager::GetTrayPickPlaneZ() const
{
	return GetActorLocation().Z + BoardLayout::TrayPanelHeight;
}

int32 AGridManager::FindTraySlotAt(const FVector& WorldPoint) const
{
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		if (!TraySlotUsed.IsValidIndex(SlotIndex) || TraySlotUsed[SlotIndex])
		{
			continue;
		}
		const FVector Delta = WorldPoint - GetTrayAnchorWorldLocation(SlotIndex);
		if (FMath::Abs(Delta.X) <= BoardLayout::TrayPanelHalf && FMath::Abs(Delta.Y) <= BoardLayout::TrayPanelHalf)
		{
			return SlotIndex;
		}
	}
	return -1;
}

bool AGridManager::IsOverReserve(const FVector& WorldPoint) const
{
	const FVector Delta = WorldPoint - GetTrayAnchorWorldLocation(ReserveSlot);
	return FMath::Abs(Delta.X) <= BoardLayout::TrayPanelHalf && FMath::Abs(Delta.Y) <= BoardLayout::TrayPanelHalf;
}

FBox2D AGridManager::GetContentBounds() const
{
	using namespace BoardLayout;
	const FVector2D Origin(GetActorLocation().X, GetActorLocation().Y);
	// The tray row is wider than a small board: frame whichever is wider.
	const float HalfWidth = FMath::Max(FrameHalfX(GridWidth), 1.5f * TraySlotSpacing + TrayPanelHalf);
	return FBox2D(Origin + FVector2D(-HalfWidth, -FrameHalfY(GridHeight)), Origin + FVector2D(HalfWidth, TrayCenterY(GridHeight) + TrayPanelHalf + 70.f)); // + room for the HOLD label under the tray
}

void AGridManager::RefillTrayIfEmpty()
{
	for (int32 SlotIndex = 0; SlotIndex < TraySize; ++SlotIndex)
	{
		if (!TraySlotUsed[SlotIndex])
		{
			return;
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < TraySize; ++SlotIndex)
	{
		Tray[SlotIndex] = PieceLibrary::MakeRandomPieceRandomColor();
		TraySlotUsed[SlotIndex] = false;
	}
}

void AGridManager::ConsumeTraySlot(int32 SlotIndex)
{
	if (TraySlotUsed.IsValidIndex(SlotIndex))
	{
		TraySlotUsed[SlotIndex] = true;
	}
	RefillTrayIfEmpty();
	RefreshTrayVisuals();
}

void AGridManager::RerollTray()
{
	for (int32 SlotIndex = 0; SlotIndex < TraySize; ++SlotIndex)
	{
		if (!TraySlotUsed[SlotIndex])
		{
			Tray[SlotIndex] = PieceLibrary::MakeRandomPieceRandomColor();
		}
	}
	RefreshTrayVisuals();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	if (APuzzleFX* FX = GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, SpawnParams))
	{
		for (int32 SlotIndex = 0; SlotIndex < TraySize; ++SlotIndex)
		{
			const FVector Anchor = GetTrayAnchorWorldLocation(SlotIndex) + FVector(0.f, 0.f, 20.f);
			FX->AddRing(Anchor, 120.f, FLinearColor(0.5f, 0.9f, 1.f) * 2.f, SlotIndex * 0.06f);
		}
	}
}

bool AGridManager::ParkPiece(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= TraySize || TraySlotUsed[SlotIndex])
	{
		return false;
	}

	if (TraySlotUsed[ReserveSlot])
	{
		Tray[ReserveSlot] = Tray[SlotIndex];
		TraySlotUsed[ReserveSlot] = false;
		TraySlotUsed[SlotIndex] = true;
		RefillTrayIfEmpty();
	}
	else
	{
		Swap(Tray[ReserveSlot], Tray[SlotIndex]);
	}
	RefreshTrayVisuals();
	return true;
}

FVector AGridManager::GetTrayAnchorWorldLocation(int32 SlotIndex) const
{
	using namespace BoardLayout;
	return GetActorLocation() + FVector((SlotIndex - (SlotCount - 1) * 0.5f) * TraySlotSpacing, TrayCenterY(GridHeight), TrayPanelHeight);
}

void AGridManager::RefreshTrayVisuals()
{
	for (APuzzleTile* Tile : TrayVisuals)
	{
		if (Tile) { Tile->Destroy(); }
	}
	TrayVisuals.Reset();
	TrayVisualSlots.Reset();

	const float MiniSpacing = TileSpacing * BoardLayout::TrayMiniScale;
	for (int32 SlotIndex = 0; SlotIndex < Tray.Num(); ++SlotIndex)
	{
		if (TraySlotUsed[SlotIndex])
		{
			continue;
		}

		const FPuzzlePieceShape& Shape = Tray[SlotIndex];
		const FVector Anchor = GetTrayAnchorWorldLocation(SlotIndex);
		const float CenterX = (Shape.GetWidth() - 1) * 0.5f;
		const float CenterY = (Shape.GetHeight() - 1) * 0.5f;

		for (int32 CellIndex = 0; CellIndex < Shape.Cells.Num(); ++CellIndex)
		{
			const FIntPoint& Cell = Shape.Cells[CellIndex];
			const EPuzzleDir Dir = Shape.Dirs.IsValidIndex(CellIndex) ? Shape.Dirs[CellIndex] : EPuzzleDir::Up;
			// Same axis convention as GetWorldLocationForCell so the tray preview isn't mirrored vs. the placed piece.
			const FVector Base = Anchor + FVector((Cell.X - CenterX) * MiniSpacing, -(Cell.Y - CenterY) * MiniSpacing, 0.f);
			if (APuzzleTile* Tile = SpawnTile(Base, Shape.Color, Dir, BoardLayout::TrayMiniScale))
			{
				Tile->SetCollidable(false);
				TrayVisuals.Add(Tile);
				TrayVisualSlots.Add(SlotIndex);
			}
		}
	}
}
