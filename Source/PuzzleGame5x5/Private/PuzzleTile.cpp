#include "PuzzleTile.h"
#include "ToonMeshBuilder.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "MeshBuffersUtil.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/CollisionProfile.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	ToonMesh::FBlockParams TileBlockParams()
	{
		ToonMesh::FBlockParams Params;
		Params.HalfExtent = FVector2D(40.f, 40.f);
		Params.CornerRadius = 14.f;
		Params.Height = 2.f * APuzzleTile::HalfHeight;
		Params.Bevel = 7.f;
		return Params;
	}

	// Part 0: the enamel body; part 1: the gold bezel outline hull.
	URealtimeMeshSimple* SharedTileMesh()
	{
		return MeshBuffers::GetSharedMesh(TEXT("PuzzleTile"), []()
		{
			return TArray<ToonMesh::FBuffers>{ ToonMesh::BuildBlock(TileBlockParams()), ToonMesh::BuildOutlineHull(TileBlockParams(), 3.f) };
		});
	}

	float EaseInOut(float T) { return T * T * (3.f - 2.f * T); }
}

APuzzleTile::APuzzleTile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(41.f, 41.f, HalfHeight));
	Box->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	RootComponent = Box;

	TileMesh = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetupAttachment(Box);
	TileMesh->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	TileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TileMesh->SetCastShadow(true);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BezelMaterialFinder(TEXT("/Game/Materials/M_Bezel.M_Bezel"));
	BezelMaterial = BezelMaterialFinder.Object;
}

void APuzzleTile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	TileMesh->SetRealtimeMesh(SharedTileMesh());

	if (!TileMaterial)
	{
		TileMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_TileRoute.M_TileRoute"));
	}
	if (TileMaterial)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(TileMaterial, this);
		TileMesh->SetMaterial(0, DynamicMaterial);
	}
	if (BezelMaterial)
	{
		TileMesh->SetMaterial(1, BezelMaterial);
	}

	ApplyVisualState();
}

void APuzzleTile::SetTileColor(EPuzzleTileColor NewColor)
{
	TileColor = NewColor;
	ApplyVisualState();
}

void APuzzleTile::SetDirection(EPuzzleDir NewDir)
{
	TileDirection = NewDir;
	bHasDirection = true;
	ApplyVisualState();
}

void APuzzleTile::SetBonus(EPuzzleBonus NewBonus)
{
	Bonus = NewBonus;
	BonusFill = Bonus == EPuzzleBonus::Outgoing ? 1.f : 0.f;
	if (Bonus != EPuzzleBonus::None)
	{
		// Loaded when needed, not from the constructor, so the asset can be rebuilt by the editor script.
		if (!BonusMaterial)
		{
			BonusMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_TileBonus.M_TileBonus"));
		}
		if (BonusMaterial)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(BonusMaterial, this);
			TileMesh->SetMaterial(0, DynamicMaterial);
		}
	}
	ApplyVisualState();
}

void APuzzleTile::MoveToPosition(int32 NewGridX, int32 NewGridY)
{
	GridX = NewGridX;
	GridY = NewGridY;
}

void APuzzleTile::SetCollidable(bool bCollidable)
{
	Box->SetCollisionEnabled(bCollidable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void APuzzleTile::ApplyVisualState()
{
	if (!DynamicMaterial)
	{
		return;
	}
	DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Bonus != EPuzzleBonus::None ? PuzzleTypes::BonusBaseColor(Bonus) : PuzzleTypes::ToLinearColor(TileColor));
	DynamicMaterial->SetScalarParameterValue(TEXT("Shimmer"), Bonus != EPuzzleBonus::None ? 0.35f : 0.f);
	const bool bIsBonus = Bonus != EPuzzleBonus::None;
	// Bonus tiles point nowhere and carry no sigil; their emblem (if any) tells the kind.
	DynamicMaterial->SetScalarParameterValue(TEXT("Symbol"), bIsBonus ? -1.f : static_cast<float>(TileColor));
	DynamicMaterial->SetScalarParameterValue(TEXT("Emblem"), Bonus == EPuzzleBonus::Basic ? 2.f : (bIsBonus ? 1.f : 0.f));
	DynamicMaterial->SetScalarParameterValue(TEXT("Fill"), BonusFill);
	DynamicMaterial->SetScalarParameterValue(TEXT("Direction"), (bHasDirection && !bIsBonus) ? static_cast<float>(TileDirection) : -1.f);
	DynamicMaterial->SetScalarParameterValue(TEXT("Glow"), CurrentGlow);
}

void APuzzleTile::SetGlow(float Glow)
{
	CurrentGlow = Glow;
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("Glow"), Glow);
	}
}

void APuzzleTile::SetGhost(bool bValid)
{
	SetCollidable(false);
	TileMesh->SetCastShadow(false);
	SetActorScale3D(FVector(0.9f));
	if (DynamicMaterial)
	{
		if (bValid)
		{
			SetGlow(0.45f);
		}
		else
		{
			DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.06f, 0.08f));
			SetGlow(0.f);
		}
	}
}

void APuzzleTile::PlayArrive(const FVector& FromLocation, float FromScale, float Duration, float InArcHeight, float Delay)
{
	ArriveTo = GetActorLocation();
	RestScale = GetActorScale3D().X;
	ArriveFrom = FromLocation;
	ArriveFromScale = FromScale;
	ArcHeight = InArcHeight;
	AnimDuration = FMath::Max(Duration, 0.01f);
	AnimDelay = Delay;
	AnimTime = 0.f;
	Anim = EAnim::Arriving;

	SetActorLocation(FromLocation);
	SetActorScale3D(FVector(FromScale));
	// Long waits (e.g. a gargoyle queued behind a clear) stay hidden instead of hovering in mid-air.
	if (Delay > 0.1f)
	{
		SetActorHiddenInGame(true);
	}
	SetActorTickEnabled(true);
}

void APuzzleTile::PlayClearEffectAndDestroy(float Delay, float InLaunchStrength)
{
	// If it is still flying in, snap to rest first so the pop starts from the board.
	if (Anim == EAnim::Arriving || Anim == EAnim::Squash)
	{
		SetActorLocation(ArriveTo);
		SetActorScale3D(FVector(RestScale));
	}
	RestScale = GetActorScale3D().X;
	LaunchStrength = InLaunchStrength;
	AnimDelay = Delay;
	AnimTime = 0.f;
	Anim = EAnim::ClearPending;
	SetActorTickEnabled(true);
}

void APuzzleTile::PlayBonusClear(float Delay, float InLaunchStrength)
{
	if (Anim == EAnim::Arriving || Anim == EAnim::Squash)
	{
		SetActorLocation(ArriveTo);
		SetActorScale3D(FVector(RestScale));
	}
	RestScale = GetActorScale3D().X;
	BonusRestLocation = GetActorLocation();
	LaunchStrength = InLaunchStrength;
	AnimDelay = Delay;
	AnimTime = 0.f;
	Anim = EAnim::BonusPre;
	SetActorTickEnabled(true);
}

void APuzzleTile::Launch()
{
	SetActorScale3D(FVector(RestScale));

	Box->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Box->SetSimulatePhysics(true);

	// Biased toward -Y (away from the camera and the tray below the board) so debris
	// tumbles back toward the candles instead of landing on the pieces you pick from.
	const FVector Direction = FVector(FMath::FRandRange(-0.6f, 0.6f), FMath::FRandRange(-0.9f, 0.1f), 1.f).GetSafeNormal();
	Box->AddImpulse(Direction * FMath::FRandRange(420.f, 640.f) * LaunchStrength, NAME_None, true);
	Box->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-540.f, 540.f), FMath::FRandRange(-540.f, 540.f), FMath::FRandRange(-360.f, 360.f)), NAME_None, true);
}

void APuzzleTile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (AnimDelay > 0.f)
	{
		AnimDelay -= DeltaTime;
		return;
	}
	if (IsHidden())
	{
		SetActorHiddenInGame(false);
	}
	AnimTime += DeltaTime;

	switch (Anim)
	{
	case EAnim::Arriving:
	{
		const float T = FMath::Clamp(AnimTime / AnimDuration, 0.f, 1.f);
		const float E = EaseInOut(T);
		const FVector Location = FMath::Lerp(ArriveFrom, ArriveTo, E) + FVector(0.f, 0.f, ArcHeight * 4.f * T * (1.f - T));
		SetActorLocation(Location);
		SetActorScale3D(FVector(FMath::Lerp(ArriveFromScale, RestScale, E)));
		if (T >= 1.f)
		{
			Anim = EAnim::Squash;
			AnimTime = 0.f;
		}
		break;
	}
	case EAnim::Squash:
	{
		constexpr float SquashDuration = 0.35f;
		const float U = FMath::Min(AnimTime / SquashDuration, 1.f);
		const float Q = FMath::Exp(-6.f * U) * FMath::Cos(U * 18.f) * (1.f - U);
		const float ScaleXY = RestScale * (1.f + 0.16f * Q);
		const float ScaleZ = RestScale * (1.f - 0.3f * Q);
		SetActorScale3D(FVector(ScaleXY, ScaleXY, ScaleZ));
		// Keep the base planted while the height squashes.
		const float BaseZ = ArriveTo.Z - HalfHeight * RestScale;
		SetActorLocation(FVector(ArriveTo.X, ArriveTo.Y, BaseZ + HalfHeight * ScaleZ));
		if (U >= 1.f)
		{
			SetActorScale3D(FVector(RestScale));
			SetActorLocation(ArriveTo);
			Anim = EAnim::None;
			SetActorTickEnabled(false);
		}
		break;
	}
	case EAnim::BonusPre:
	{
		constexpr float PreDuration = 0.9f;
		const float U = FMath::Min(AnimTime / PreDuration, 1.f);
		if (Bonus == EPuzzleBonus::Basic)
		{
			// The pumpkin swells and shakes, glowing from inside, before it bursts.
			SetActorScale3D(FVector(RestScale * (1.f + 0.4f * U * U)));
			SetActorLocation(BonusRestLocation + FVector(FMath::Sin(AnimTime * 70.f) * 5.f * U, 0.f, 0.f));
			SetGlow(1.8f * U * U);
		}
		else
		{
			BonusFill = Bonus == EPuzzleBonus::Outgoing ? 1.f - U : U;
			if (DynamicMaterial)
			{
				DynamicMaterial->SetScalarParameterValue(TEXT("Fill"), BonusFill);
			}
			SetGlow(0.7f * U);
		}
		if (U >= 1.f)
		{
			SetActorLocation(BonusRestLocation);
			SetActorScale3D(FVector(RestScale));
			Anim = EAnim::ClearPending;
			AnimTime = 0.f;
		}
		break;
	}
	case EAnim::ClearPending:
		Anim = EAnim::Popping;
		AnimTime = 0.f;
		break;
	case EAnim::Popping:
	{
		constexpr float PopDuration = 0.16f;
		const float U = FMath::Min(AnimTime / PopDuration, 1.f);
		SetGlow(1.4f * U);
		SetActorScale3D(FVector(RestScale * (1.f + 0.22f * FMath::Sin(U * PI))));
		if (U >= 1.f)
		{
			Launch();
			Anim = EAnim::Flying;
			AnimTime = 0.f;
		}
		break;
	}
	case EAnim::Flying:
	{
		constexpr float Lifetime = 1.7f;
		constexpr float ShrinkTime = 0.4f;
		SetGlow(FMath::Lerp(1.4f, 0.25f, FMath::Min(AnimTime / 0.6f, 1.f)));
		if (AnimTime > Lifetime - ShrinkTime)
		{
			const float Shrink = FMath::Clamp((Lifetime - AnimTime) / ShrinkTime, 0.f, 1.f);
			SetActorScale3D(FVector(FMath::Max(RestScale * Shrink, 0.01f)));
		}
		if (AnimTime >= Lifetime)
		{
			Destroy();
		}
		break;
	}
	default:
		SetActorTickEnabled(false);
		break;
	}
}
