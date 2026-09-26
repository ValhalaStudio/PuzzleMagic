#include "GridManager.h"
#include "PuzzleTile.h"
#include "PuzzleFX.h"
#include "PieceLibrary.h"
#include "ToonMeshBuilder.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
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
	constexpr float FrameHalf = 452.f;
	constexpr float FrameHeight = 18.f;
	constexpr float PanelHalf = 420.f;
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
	constexpr float TrayCenterY = FrameHalf + TrayGap + TrayPanelHalf;
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
	BoardCollision->SetBoxExtent(FVector(BoardLayout::FrameHalf, BoardLayout::FrameHalf, BoardLayout::TileBaseZ * 0.5f));
	BoardCollision->SetRelativeLocation(FVector(0.f, 0.f, BoardLayout::TileBaseZ * 0.5f));
	BoardCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	BlessedAura = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlessedAura"));
	BlessedAura->SetupAttachment(RootComponent);
	BlessedAura->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlessedAura->SetCastShadow(false);

	BlessedLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BlessedLight"));
	BlessedLight->SetupAttachment(RootComponent);
	BlessedLight->SetIntensityUnits(ELightUnits::Candelas);
	BlessedLight->SetLightColor(FLinearColor(1.f, 0.78f, 0.35f));
	BlessedLight->SetAttenuationRadius(420.f);
	BlessedLight->SetCastShadows(false);

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
		BlessedAura->SetStaticMesh(PlaneFinder.Object);
		TargetAura->SetStaticMesh(PlaneFinder.Object);
	}
}

void AGridManager::BeginPlay()
{
	Super::BeginPlay();
	BuildBoardVisuals();
	if (AuraMaterial)
	{
		BlessedAura->SetMaterial(0, UMaterialInstanceDynamic::Create(AuraMaterial, this));

		UMaterialInstanceDynamic* TargetMID = UMaterialInstanceDynamic::Create(AuraMaterial, this);
		TargetMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.92f, 0.65f));
		TargetMID->SetScalarParameterValue(TEXT("Intensity"), 5.f);
		TargetAura->SetMaterial(0, TargetMID);
	}
}

void AGridManager::ShowAreaTarget(int32 CenterX, int32 CenterY)
{
	CenterX = FMath::Clamp(CenterX, 1, GridSize - 2);
	CenterY = FMath::Clamp(CenterY, 1, GridSize - 2);
	const float Size = BoxSize * TileSpacing * 1.25f;
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
	if (BlessedBox >= 0)
	{
		BlessedLight->SetIntensity(22.f + 8.f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.5f));
	}
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
	Frame.HalfExtent = FVector2D(FrameHalf, FrameHalf);
	Frame.CornerRadius = 44.f;
	Frame.Height = FrameHeight;
	Frame.Bevel = 8.f;
	AddSection(0, ToonMesh::BuildBlock(Frame), Marble(FrameStone));
	AddSection(1, ToonMesh::BuildOutlineHull(Frame, 5.f), BezelMaterial);

	ToonMesh::FBlockParams PanelBlock;
	PanelBlock.HalfExtent = FVector2D(PanelHalf, PanelHalf);
	PanelBlock.CornerRadius = 30.f;
	PanelBlock.Height = PanelHeight;
	PanelBlock.Bevel = 4.f;
	ToonMesh::FBuffers PanelBuffers;
	PanelBuffers.Append(ToonMesh::BuildBlock(PanelBlock), FVector(0.f, 0.f, FrameHeight));
	AddSection(2, PanelBuffers, Marble(PanelMarble));

	// All 81 slots merged into two sections (alternating 3x3 boxes).
	ToonMesh::FBlockParams Slot;
	Slot.HalfExtent = FVector2D(SlotHalf, SlotHalf);
	Slot.CornerRadius = 12.f;
	Slot.Height = SlotHeight;
	Slot.Bevel = 1.5f;
	Slot.CornerSegments = 3;
	Slot.BevelSegments = 1;
	const ToonMesh::FBuffers SlotBlock = ToonMesh::BuildBlock(Slot);
	ToonMesh::FBuffers LightSlots, DarkSlots;
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			FVector Offset = GetWorldLocationForCell(X, Y) - GetActorLocation();
			Offset.Z = FrameHeight + PanelHeight;
			const bool bLightBox = ((X / BoxSize) + (Y / BoxSize)) % 2 == 0;
			(bLightBox ? LightSlots : DarkSlots).Append(SlotBlock, Offset);
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

APuzzleTile* AGridManager::SpawnTile(const FVector& BaseLocation, EPuzzleTileColor Color, float Scale)
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

	const int32 NumCells = GridSize * GridSize;
	Filled.Init(false, NumCells);
	CellColors.Init(EPuzzleTileColor::Red, NumCells);
	CellStone.Init(0, NumCells);
	CellVisuals.Init(nullptr, NumCells);

	Tray.Reset();
	Tray.SetNum(SlotCount);
	TraySlotUsed.Init(true, SlotCount);
	BlessedBox = -1;
	RefillTrayIfEmpty();
	RefreshTrayVisuals();
}

bool AGridManager::IsValidCoord(int32 X, int32 Y) const
{
	return X >= 0 && X < GridSize && Y >= 0 && Y < GridSize;
}

bool AGridManager::CanPlacePieceAt(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY) const
{
	for (const FIntPoint& Cell : Shape.Cells)
	{
		const int32 X = OriginX + Cell.X;
		const int32 Y = OriginY + Cell.Y;
		if (!IsValidCoord(X, Y) || Filled[Y * GridSize + X])
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
		const int32 Index = Y * GridSize + X;

		Filled[Index] = true;
		CellColors[Index] = Shape.Color;
		CellStone[Index] = 0;

		APuzzleTile* Tile = SpawnTile(GetWorldLocationForCell(X, Y), Shape.Color, 1.f);
		if (!Tile)
		{
			continue;
		}
		Tile->MoveToPosition(X, Y);
		Tile->SetShimmer(IsInBlessedBox(X, Y));
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
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
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
	const FIntVector Key(OriginX, OriginY, Shape.Cells.Num() * 100 + static_cast<int32>(Shape.Color) * 10 + (bValid ? 1 : 0));
	if (Key == GhostKey && GhostTiles.Num() > 0)
	{
		return;
	}
	HideGhostPreview();
	GhostKey = Key;

	for (const FIntPoint& Cell : Shape.Cells)
	{
		const int32 X = OriginX + Cell.X;
		const int32 Y = OriginY + Cell.Y;
		if (!IsValidCoord(X, Y))
		{
			continue;
		}

		if (APuzzleTile* Ghost = SpawnTile(GetWorldLocationForCell(X, Y) + FVector(0.f, 0.f, 10.f), Shape.Color, 1.f))
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

TArray<int32> AGridManager::GetFullRows() const { return ComputeFullRows(Filled); }
TArray<int32> AGridManager::GetFullColumns() const { return ComputeFullColumns(Filled); }
TArray<int32> AGridManager::GetFullBoxes() const { return ComputeFullBoxes(Filled); }

TArray<int32> AGridManager::ComputeFullRows(const TArray<bool>& FilledState)
{
	TArray<int32> Rows;
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		bool bFull = true;
		for (int32 X = 0; X < GridSize; ++X)
		{
			if (!FilledState[Y * GridSize + X]) { bFull = false; break; }
		}
		if (bFull) { Rows.Add(Y); }
	}
	return Rows;
}

TArray<int32> AGridManager::ComputeFullColumns(const TArray<bool>& FilledState)
{
	TArray<int32> Columns;
	for (int32 X = 0; X < GridSize; ++X)
	{
		bool bFull = true;
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			if (!FilledState[Y * GridSize + X]) { bFull = false; break; }
		}
		if (bFull) { Columns.Add(X); }
	}
	return Columns;
}

TArray<int32> AGridManager::ComputeFullBoxes(const TArray<bool>& FilledState)
{
	TArray<int32> Boxes;
	const int32 BoxesPerSide = GridSize / BoxSize;
	for (int32 BoxY = 0; BoxY < BoxesPerSide; ++BoxY)
	{
		for (int32 BoxX = 0; BoxX < BoxesPerSide; ++BoxX)
		{
			bool bFull = true;
			for (int32 InnerY = 0; InnerY < BoxSize && bFull; ++InnerY)
			{
				for (int32 InnerX = 0; InnerX < BoxSize; ++InnerX)
				{
					const int32 X = BoxX * BoxSize + InnerX;
					const int32 Y = BoxY * BoxSize + InnerY;
					if (!FilledState[Y * GridSize + X]) { bFull = false; break; }
				}
			}
			if (bFull) { Boxes.Add(BoxY * BoxesPerSide + BoxX); }
		}
	}
	return Boxes;
}

int32 AGridManager::SimulateLinesCleared(const FPuzzlePieceShape& Shape, int32 OriginX, int32 OriginY) const
{
	if (!CanPlacePieceAt(Shape, OriginX, OriginY))
	{
		return -1;
	}

	TArray<bool> Hypothetical = Filled;
	for (const FIntPoint& Cell : Shape.Cells)
	{
		Hypothetical[(OriginY + Cell.Y) * GridSize + (OriginX + Cell.X)] = true;
	}

	return ComputeFullRows(Hypothetical).Num() + ComputeFullColumns(Hypothetical).Num() + ComputeFullBoxes(Hypothetical).Num();
}

int32 AGridManager::CountFilledNeighbors(int32 X, int32 Y) const
{
	int32 Count = 0;
	static const FIntPoint Offsets[4] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
	for (const FIntPoint& Offset : Offsets)
	{
		const int32 NX = X + Offset.X;
		const int32 NY = Y + Offset.Y;
		if (IsValidCoord(NX, NY) && Filled[NY * GridSize + NX])
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

bool AGridManager::IsLineBlessed(const TArray<int32>& Cells) const
{
	for (int32 Index : Cells)
	{
		if (CellStone[Index] != 0 || CellColors[Index] != CellColors[Cells[0]])
		{
			return false;
		}
	}
	return true;
}

void AGridManager::PopCells(const TArray<int32>& Indices, const FVector2D& Centre, float Delay, FClearResult& Result, APuzzleFX* FX)
{
	for (int32 Index : Indices)
	{
		APuzzleTile* Tile = CellVisuals[Index];
		const float Ripple = FVector2D::Distance(FVector2D(Index % GridSize, Index / GridSize), Centre) * 0.05f;
		const float PopDelay = Delay + 0.1f + Ripple;

		if (CellStone[Index] == 2)
		{
			// First hit only cracks a gargoyle stone; it stays on the board.
			CellStone[Index] = 1;
			++Result.StonesCracked;
			if (Tile)
			{
				Tile->PlayStoneHit(PopDelay);
			}
			continue;
		}

		if (CellStone[Index] == 1)
		{
			++Result.StonesBroken;
		}
		else
		{
			++Result.ColorCounts[static_cast<int32>(CellColors[Index])];
		}
		CellStone[Index] = 0;
		Filled[Index] = false;
		CellVisuals[Index] = nullptr;
		++Result.Cells;

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

FClearResult AGridManager::CheckAndClearLines(float ClearDelay)
{
	FClearResult Result;
	const TArray<int32> Rows = GetFullRows();
	const TArray<int32> Columns = GetFullColumns();
	const TArray<int32> Boxes = GetFullBoxes();

	Result.Lines = Rows.Num() + Columns.Num() + Boxes.Num();
	Result.Boxes = Boxes.Num();
	if (Result.Lines == 0)
	{
		return Result;
	}

	const int32 BoxesPerSide = GridSize / BoxSize;
	TArray<TArray<int32>> Lines;
	TArray<FLinearColor> LineColors;
	for (int32 Y : Rows)
	{
		TArray<int32>& Line = Lines.AddDefaulted_GetRef();
		for (int32 X = 0; X < GridSize; ++X) { Line.Add(Y * GridSize + X); }
	}
	for (int32 X : Columns)
	{
		TArray<int32>& Line = Lines.AddDefaulted_GetRef();
		for (int32 Y = 0; Y < GridSize; ++Y) { Line.Add(Y * GridSize + X); }
	}
	for (int32 Box : Boxes)
	{
		TArray<int32>& Line = Lines.AddDefaulted_GetRef();
		for (int32 Inner = 0; Inner < BoxSize * BoxSize; ++Inner)
		{
			Line.Add(((Box / BoxesPerSide) * BoxSize + Inner / BoxSize) * GridSize + (Box % BoxesPerSide) * BoxSize + Inner % BoxSize);
		}
		Result.bBlessedBox |= (Box == BlessedBox);
	}

	TSet<int32> CellsToClear;
	TArray<bool> LineBlessed;
	for (const TArray<int32>& Line : Lines)
	{
		const bool bBlessed = IsLineBlessed(Line);
		LineBlessed.Add(bBlessed);
		Result.Blessings += bBlessed ? 1 : 0;
		CellsToClear.Append(Line);
	}

	FVector2D CentroidCell = FVector2D::ZeroVector;
	for (int32 Index : CellsToClear)
	{
		CentroidCell += FVector2D(Index % GridSize, Index / GridSize);
	}
	CentroidCell /= CellsToClear.Num();
	LastClearCentroid = GetWorldLocationForCell(0, 0) + FVector(CentroidCell.X * TileSpacing, -CentroidCell.Y * TileSpacing, APuzzleTile::HalfHeight * 2.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	APuzzleFX* FX = GetWorld() ? GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), LastClearCentroid, FRotator::ZeroRotator, SpawnParams) : nullptr;

	// Magic sweep over each cleared line (gold for blessed lines), rune rings for boxes.
	const float StripZ = GetPickPlaneZ() + APuzzleTile::HalfHeight + 6.f;
	const float BoardLength = GridSize * TileSpacing + 30.f;
	if (FX)
	{
		int32 LineIndex = 0;
		auto SweepColor = [&LineBlessed](int32 I) { return LineBlessed[I] ? FLinearColor(1.f, 0.85f, 0.3f) * 1.8f : FLinearColor(1.0f, 0.75f, 0.3f); };
		for (int32 Y : Rows)
		{
			const FVector Center = GetWorldLocationForCell(GridSize / 2, Y);
			FX->AddStrip(FVector(Center.X, Center.Y, StripZ), FVector2D(BoardLength, 120.f), 0.f, SweepColor(LineIndex++), ClearDelay);
		}
		for (int32 X : Columns)
		{
			const FVector Center = GetWorldLocationForCell(X, GridSize / 2);
			FX->AddStrip(FVector(Center.X, Center.Y, StripZ), FVector2D(BoardLength, 120.f), 90.f, SweepColor(LineIndex++), ClearDelay);
		}
		for (int32 Box : Boxes)
		{
			const FVector Center = GetWorldLocationForCell((Box % BoxesPerSide) * BoxSize + 1, (Box / BoxesPerSide) * BoxSize + 1);
			const bool bHoly = Box == BlessedBox || LineBlessed[LineIndex++];
			FX->AddRing(FVector(Center.X, Center.Y, StripZ), BoxSize * TileSpacing * (bHoly ? 0.8f : 0.62f),
				bHoly ? FLinearColor(1.f, 0.85f, 0.35f) * 2.f : FLinearColor(0.9f, 0.45f, 1.f), ClearDelay);
		}
		if (Result.Lines >= 2 || Result.bBlessedBox)
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
	CenterX = FMath::Clamp(CenterX, 1, GridSize - 2);
	CenterY = FMath::Clamp(CenterY, 1, GridSize - 2);

	TArray<int32> Cells;
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			const int32 Index = (CenterY + DY) * GridSize + CenterX + DX;
			if (Filled[Index])
			{
				// Holy Light shatters stones outright.
				CellStone[Index] = FMath::Min<uint8>(CellStone[Index], 1);
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

	PopCells(Cells, FVector2D(CenterX, CenterY), Delay, Result, FX);
	return Result;
}

bool AGridManager::SpawnCurseStone(FIntPoint& OutCell, float Delay)
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
	const int32 X = Index % GridSize;
	const int32 Y = Index / GridSize;
	OutCell = FIntPoint(X, Y);
	SpawnStoneAt(Index, Delay, 0.45f, 420.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	const FVector Where = GetWorldLocationForCell(X, Y) + FVector(0.f, 0.f, 6.f);
	if (APuzzleFX* FX = GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), Where, FRotator::ZeroRotator, SpawnParams))
	{
		const float Impact = Delay + 0.45f;
		FX->AddRing(Where, 150.f, FLinearColor(0.6f, 0.1f, 0.9f) * 2.f, Impact);
		for (int32 S = 0; S < 8; ++S)
		{
			FX->AddSparkle(Where + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 10.f), FLinearColor(0.7f, 0.2f, 1.f), Impact);
		}
	}
	return true;
}

void AGridManager::SpawnStoneAt(int32 Index, float Delay, float FallTime, float FallHeight)
{
	const int32 X = Index % GridSize;
	const int32 Y = Index / GridSize;
	Filled[Index] = true;
	CellStone[Index] = 2;

	if (APuzzleTile* Stone = SpawnTile(GetWorldLocationForCell(X, Y), EPuzzleTileColor::Red, 1.f))
	{
		Stone->MoveToPosition(X, Y);
		Stone->SetStone(2);
		Stone->PlayArrive(Stone->GetActorLocation() + FVector(0.f, 0.f, FallHeight), 1.25f, FallTime, 0.f, Delay);
		CellVisuals[Index] = Stone;
	}
}

FIntPoint AGridManager::PickLightningTarget() const
{
	TArray<int32> Tiles;
	TArray<int32> Empty;
	for (int32 Index = 0; Index < Filled.Num(); ++Index)
	{
		if (!Filled[Index])
		{
			Empty.Add(Index);
		}
		else if (CellStone[Index] == 0)
		{
			Tiles.Add(Index);
		}
	}
	const TArray<int32>& Pool = Tiles.Num() > 0 ? Tiles : Empty;
	if (Pool.Num() == 0)
	{
		return FIntPoint(-1, -1);
	}
	const int32 Index = Pool[FMath::RandRange(0, Pool.Num() - 1)];
	return FIntPoint(Index % GridSize, Index / GridSize);
}

void AGridManager::StrikeLightning(const FIntPoint& Cell, float Delay, bool bWarded)
{
	if (!IsValidCoord(Cell.X, Cell.Y) || !GetWorld())
	{
		return;
	}
	const int32 Index = Cell.Y * GridSize + Cell.X;
	const FVector Ground = GetWorldLocationForCell(Cell.X, Cell.Y) + FVector(0.f, 0.f, APuzzleTile::HalfHeight * 2.f);
	// Warded bolts break on a shield of light hovering over the board.
	const FVector Impact = bWarded ? Ground + FVector(0.f, 0.f, 320.f) : Ground;

	if (!bWarded)
	{
		if (Filled[Index])
		{
			CellStone[Index] = 2;
			if (APuzzleTile* Tile = CellVisuals[Index])
			{
				Tile->PlayPetrify(Delay);
			}
		}
		else
		{
			// Slammed down by the bolt itself.
			SpawnStoneAt(Index, FMath::Max(Delay - 0.08f, 0.f), 0.08f, 80.f);
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	APuzzleFX* FX = GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), Impact, FRotator::ZeroRotator, SpawnParams);
	if (!FX)
	{
		return;
	}
	const FLinearColor Electric(0.55f, 0.72f, 1.f);
	FX->AddBolt(Impact, 1500.f, 420.f, Electric, Delay, FMath::FRandRange(0.f, 100.f));
	// A real light at the strike: every tile's ray-traced shadow flicks across the board.
	FX->AddFlash(Impact + FVector(0.f, 0.f, 70.f), FLinearColor(0.75f, 0.85f, 1.f), 450.f, 1500.f, Delay, 0.9f, true);
	if (bWarded)
	{
		const FLinearColor Holy(1.f, 0.8f, 0.4f);
		const FVector Centre(GetActorLocation().X, GetActorLocation().Y, Impact.Z);
		FX->AddAura(Centre, GridSize * TileSpacing * 1.05f, Holy * 1.2f, Delay, 1.2f, 50.f);
		FX->AddRing(Impact, 160.f, Holy * 1.6f, Delay);
		FX->AddRing(Impact, 300.f, Holy * 1.f, Delay + 0.08f);
		FX->AddFlash(Impact, Holy, 120.f, 1200.f, Delay + 0.05f, 1.f, false);
		for (int32 S = 0; S < 16; ++S)
		{
			const FVector Offset(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-20.f, 20.f));
			FX->AddSparkle(Impact + Offset, Holy, Delay + FMath::FRandRange(0.f, 0.25f));
		}
	}
	else
	{
		FX->AddRing(Impact + FVector(0.f, 0.f, 4.f), 130.f, Electric * 3.f, Delay);
		FX->AddRing(Impact + FVector(0.f, 0.f, 4.f), 260.f, Electric * 1.5f, Delay + 0.1f);
		for (int32 S = 0; S < 14; ++S)
		{
			const FVector Offset(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 8.f);
			FX->AddSparkle(Impact + Offset, FLinearColor(0.8f, 0.9f, 1.f), Delay + FMath::FRandRange(0.f, 0.15f));
		}
	}
}

void AGridManager::PlayHex(float Delay, bool bWarded)
{
	if (!GetWorld())
	{
		return;
	}
	const FVector Centre = GetActorLocation() + FVector(0.f, 0.f, GetPickPlaneZ() + APuzzleTile::HalfHeight + 12.f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	APuzzleFX* FX = GetWorld()->SpawnActor<APuzzleFX>(APuzzleFX::StaticClass(), Centre, FRotator::ZeroRotator, SpawnParams);
	if (!FX)
	{
		return;
	}
	const FLinearColor Curse(0.5f, 0.08f, 1.f);
	const float Size = GridSize * TileSpacing * 1.1f;
	const float CurseLife = bWarded ? 0.9f : 2.f;
	FX->AddAura(Centre, Size, Curse * 3.f, Delay, CurseLife, -35.f);
	FX->AddAura(Centre, Size * 0.55f, FLinearColor(0.9f, 0.1f, 0.4f) * 2.f, Delay + 0.15f, CurseLife - 0.15f, 70.f);
	FX->AddFlash(Centre + FVector(0.f, 0.f, 180.f), Curse, 350.f, 1400.f, Delay, CurseLife, false);
	for (int32 S = 0; S < 24; ++S)
	{
		const float Angle = S * (2.f * PI / 24.f);
		const FVector Offset(FMath::Cos(Angle) * Size * 0.45f, FMath::Sin(Angle) * Size * 0.45f, 0.f);
		FX->AddSparkle(Centre + Offset, FLinearColor(0.7f, 0.2f, 1.f), Delay + 0.2f + S * 0.02f);
	}
	if (bWarded)
	{
		const FLinearColor Holy(1.f, 0.8f, 0.4f);
		const float WardDelay = Delay + 0.55f;
		FX->AddAura(Centre + FVector(0.f, 0.f, 4.f), Size * 1.1f, Holy * 1.4f, WardDelay, 1.1f, 60.f);
		FX->AddRing(Centre, Size * 0.3f, Holy * 1.4f, WardDelay);
		FX->AddRing(Centre, Size * 0.55f, Holy * 1.f, WardDelay + 0.1f);
		FX->AddFlash(Centre + FVector(0.f, 0.f, 200.f), Holy, 220.f, 1400.f, WardDelay, 1.f, false);
		for (int32 S = 0; S < 20; ++S)
		{
			const FVector Offset(FMath::FRandRange(-Size * 0.4f, Size * 0.4f), FMath::FRandRange(-Size * 0.4f, Size * 0.4f), 0.f);
			FX->AddSparkle(Centre + Offset, Holy, WardDelay + FMath::FRandRange(0.f, 0.3f));
		}
	}
}

FIntPoint AGridManager::FindDensestArea() const
{
	FIntPoint Best(GridSize / 2, GridSize / 2);
	int32 BestWeight = -1;
	for (int32 CY = 1; CY < GridSize - 1; ++CY)
	{
		for (int32 CX = 1; CX < GridSize - 1; ++CX)
		{
			int32 Weight = 0;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const int32 Index = (CY + DY) * GridSize + CX + DX;
					Weight += Filled[Index] ? (CellStone[Index] ? 2 : 1) : 0;
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
			const int32 Row = Index / GridSize;
			Tile->PlayClearEffectAndDestroy(0.25f + (GridSize - 1 - Row) * 0.06f + FMath::FRandRange(0.f, 0.05f), 0.8f);
			CellVisuals[Index] = nullptr;
		}
	}
}

bool AGridManager::IsInBlessedBox(int32 X, int32 Y) const
{
	const int32 BoxesPerSide = GridSize / BoxSize;
	return BlessedBox >= 0 && (Y / BoxSize) * BoxesPerSide + X / BoxSize == BlessedBox;
}

void AGridManager::ChooseBlessedBox()
{
	const int32 BoxCount = (GridSize / BoxSize) * (GridSize / BoxSize);
	int32 Next = FMath::RandRange(0, BoxCount - 1);
	if (Next == BlessedBox)
	{
		Next = (Next + 1 + FMath::RandRange(0, BoxCount - 2)) % BoxCount;
	}
	BlessedBox = Next;
	UpdateBlessedVisuals();
}

void AGridManager::UpdateBlessedVisuals()
{
	const bool bActive = BlessedBox >= 0;
	BlessedAura->SetVisibility(bActive);
	BlessedLight->SetVisibility(bActive);
	if (!bActive)
	{
		return;
	}

	const int32 BoxesPerSide = GridSize / BoxSize;
	const FVector Centre = GetWorldLocationForCell((BlessedBox % BoxesPerSide) * BoxSize + 1, (BlessedBox / BoxesPerSide) * BoxSize + 1);
	const float Size = BoxSize * TileSpacing * 1.15f;
	BlessedAura->SetWorldLocation(Centre + FVector(0.f, 0.f, 1.5f));
	BlessedAura->SetWorldScale3D(FVector(Size / 100.f, Size / 100.f, 1.f));
	BlessedLight->SetWorldLocation(Centre + FVector(0.f, 0.f, 140.f));

	for (int32 Index = 0; Index < CellVisuals.Num(); ++Index)
	{
		if (CellVisuals[Index] && CellStone[Index] == 0)
		{
			CellVisuals[Index]->SetShimmer(IsInBlessedBox(Index % GridSize, Index / GridSize));
		}
	}
}

// Cell +Y maps to world -Y: the camera looks toward -Y, so this makes cell +X
// screen-right and cell +Y screen-up (Unreal is left-handed, so one axis must flip).
FVector AGridManager::GetWorldLocationForCell(int32 X, int32 Y) const
{
	const float Center = (GridSize - 1) * 0.5f;
	return GetActorLocation() + FVector((X - Center) * TileSpacing, -(Y - Center) * TileSpacing, BoardLayout::TileBaseZ);
}

bool AGridManager::WorldLocationToCell(const FVector& WorldLocation, int32& OutX, int32& OutY) const
{
	const float Center = (GridSize - 1) * 0.5f;
	const FVector Local = WorldLocation - GetActorLocation();

	OutX = FMath::RoundToInt(Local.X / TileSpacing + Center);
	OutY = FMath::RoundToInt(-Local.Y / TileSpacing + Center);
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
	return FBox2D(Origin + FVector2D(-FrameHalf, -FrameHalf), Origin + FVector2D(FrameHalf, TrayCenterY + TrayPanelHalf));
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
	// A fresh tray also moves the blessing to a new box.
	ChooseBlessedBox();
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
	return GetActorLocation() + FVector((SlotIndex - (SlotCount - 1) * 0.5f) * TraySlotSpacing, TrayCenterY, TrayPanelHeight);
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

		for (const FIntPoint& Cell : Shape.Cells)
		{
			// Same axis convention as GetWorldLocationForCell so the tray preview isn't mirrored vs. the placed piece.
			const FVector Base = Anchor + FVector((Cell.X - CenterX) * MiniSpacing, -(Cell.Y - CenterY) * MiniSpacing, 0.f);
			if (APuzzleTile* Tile = SpawnTile(Base, Shape.Color, BoardLayout::TrayMiniScale))
			{
				Tile->SetCollidable(false);
				TrayVisuals.Add(Tile);
				TrayVisualSlots.Add(SlotIndex);
			}
		}
	}
}
