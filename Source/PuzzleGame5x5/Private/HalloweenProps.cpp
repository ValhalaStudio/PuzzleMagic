#include "HalloweenProps.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

namespace
{
	// Direction from the scene toward the camera (it looks toward -Y, pitched down): billboards face this way.
	const FVector TowardCamera = FVector(0.f, 0.57f, 0.82f).GetSafeNormal();
}

AHalloweenProps::AHalloweenProps()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleFinder(TEXT("/Game/Materials/M_Marble.M_Marble"));
	SphereMesh = SphereFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	PlaneMesh = PlaneFinder.Object;
	IronMaterial = MarbleFinder.Object;
}

UStaticMeshComponent* AHalloweenProps::AddPart(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, UMaterialInterface* Material, bool bCastShadow)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(bCastShadow);
	Component->RegisterComponent();
	Owned.Add(Component);
	Component->SetWorldLocation(GetActorLocation() + Location);
	Component->SetWorldScale3D(Scale);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	return Component;
}

void AHalloweenProps::BeginPlay()
{
	Super::BeginPlay();

	// Loaded here, not in the constructor, so the editor scripts can rebuild these materials in place.
	LiquidMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_CauldronLiquid.M_CauldronLiquid"));
	SteamMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_SteamPuff.M_SteamPuff"));
	BatMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_BatSilhouette.M_BatSilhouette"));
	FacingCamera = FRotationMatrix::MakeFromZX(TowardCamera, FVector(1.f, 0.f, 0.f)).Rotator();

	BuildAll();
}

void AHalloweenProps::BuildAll()
{
	BuildCauldron(-1.f);
	BuildCauldron(1.f);
	BuildBats();
}

void AHalloweenProps::ClearAll()
{
	for (UActorComponent* Component : Owned)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	Owned.Reset();
	Puffs.Reset();
	Bubbles.Reset();
	Bats.Reset();
	Cauldrons.Reset();
}

void AHalloweenProps::FitToCamera()
{
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	FVector2D Viewport(0.f, 0.f);
	if (!Controller || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	GEngine->GameViewport->GetViewportSize(Viewport);
	if (Viewport.X <= 0.f || Viewport.Y <= 0.f)
	{
		return;
	}

	// On the 8x8 board the cauldron stands at about 12% across and 41% down the screen: keep it there.
	FVector Origin, Direction;
	if (!Controller->DeprojectScreenPositionToWorld(Viewport.X * 0.123f, Viewport.Y * 0.41f, Origin, Direction) || FMath::Abs(Direction.Z) < 0.01f)
	{
		return;
	}
	const FVector Hit = Origin + Direction * (-Origin.Z / Direction.Z);
	const float NewX = FMath::Abs(Hit.X);
	if (NewX < 50.f || FMath::IsNearlyEqual(NewX, CauldronX, CauldronX * 0.03f))
	{
		return;
	}
	CauldronScale = 0.85f * NewX / 780.f;
	CauldronX = NewX;
	CauldronY = Hit.Y;
	ClearAll();
	BuildAll();
}

void AHalloweenProps::BuildCauldron(float Side)
{
	const float S = CauldronScale;
	const FVector Base(Side * CauldronX, CauldronY, 0.f);

	UMaterialInstanceDynamic* Iron = UMaterialInstanceDynamic::Create(IronMaterial, this);
	Iron->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.012f, 0.012f, 0.016f));
	Iron->SetVectorParameterValue(TEXT("VeinColor"), FLinearColor(0.08f, 0.08f, 0.1f));
	Iron->SetScalarParameterValue(TEXT("VeinStrength"), 0.25f);
	Iron->SetScalarParameterValue(TEXT("Roughness"), 0.4f);

	// Three short legs, the round body, a collar round the mouth, and two handles.
	for (int32 Leg = 0; Leg < 3; ++Leg)
	{
		const float Angle = FMath::DegreesToRadians(90.f + Leg * 120.f);
		AddPart(CylinderMesh, Base + FVector(FMath::Cos(Angle) * 62.f * S, FMath::Sin(Angle) * 62.f * S, 21.f * S), FVector(0.2f * S, 0.2f * S, 0.42f * S), Iron, true);
	}
	AddPart(SphereMesh, Base + FVector(0.f, 0.f, 100.f * S), FVector(2.3f * S, 2.3f * S, 1.8f * S), Iron, true);
	AddPart(CylinderMesh, Base + FVector(0.f, 0.f, 172.f * S), FVector(2.2f * S, 2.2f * S, 0.22f * S), Iron, true);
	AddPart(SphereMesh, Base + FVector(-115.f * S, 0.f, 150.f * S), FVector(0.32f * S), Iron, true);
	AddPart(SphereMesh, Base + FVector(115.f * S, 0.f, 150.f * S), FVector(0.32f * S), Iron, true);

	// The lime liquid.
	const FVector Surface = Base + FVector(0.f, 0.f, 184.f * S);
	AddPart(CylinderMesh, Surface, FVector(1.95f * S, 1.95f * S, 0.03f * S), LiquidMaterial, false);

	for (int32 I = 0; I < 6; ++I)
	{
		FBubble& Bubble = Bubbles.AddDefaulted_GetRef();
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::FRandRange(0.f, 75.f * S);
		Bubble.Base = Surface + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 2.f);
		Bubble.Size = FMath::FRandRange(14.f, 26.f) * S;
		Bubble.Period = FMath::FRandRange(0.9f, 1.8f);
		Bubble.Age = FMath::FRandRange(0.f, Bubble.Period);
		Bubble.Mesh = AddPart(SphereMesh, Bubble.Base, FVector(0.001f), LiquidMaterial, false);
	}

	// Steam curling up and away.
	for (int32 I = 0; I < 9; ++I)
	{
		FPuff& Puff = Puffs.AddDefaulted_GetRef();
		Puff.Top = Surface + FVector(FMath::FRandRange(-45.f, 45.f) * S, FMath::FRandRange(-30.f, 30.f) * S, 6.f);
		Puff.Life = FMath::FRandRange(3.f, 4.4f);
		Puff.Age = FMath::FRandRange(0.f, Puff.Life);
		Puff.Drift = FMath::FRandRange(-70.f, 70.f);
		Puff.Size = FMath::FRandRange(120.f, 190.f) * S;
		Puff.Mesh = AddPart(PlaneMesh, Puff.Top, FVector(0.01f), nullptr, false);
		Puff.Mesh->SetWorldRotation(FacingCamera);
		if (SteamMaterial)
		{
			Puff.MID = UMaterialInstanceDynamic::Create(SteamMaterial, this);
			Puff.MID->SetScalarParameterValue(TEXT("Seed"), FMath::FRand() * 10.f);
			Puff.Mesh->SetMaterial(0, Puff.MID);
		}
	}

	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->RegisterComponent();
	Owned.Add(Light);
	Light->SetWorldLocation(GetActorLocation() + Base + FVector(0.f, -40.f, 250.f * S));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetLightColor(FLinearColor(0.45f, 1.f, 0.12f));
	Light->SetIntensity(60.f);
	Light->SetAttenuationRadius(520.f * S / 0.85f);
	Light->SetCastShadows(false);
	FCauldron& Cauldron = Cauldrons.AddDefaulted_GetRef();
	Cauldron.Light = Light;
	Cauldron.Seed = FMath::FRandRange(0.f, 20.f);
}

void AHalloweenProps::BuildBats()
{
	if (!BatMaterial)
	{
		return;
	}
	// Six bats circle above the two cauldrons, two more cross the top of the scene. Everything scales with the cauldrons.
	const float Fit = CauldronScale / 0.85f;
	for (int32 I = 0; I < 8; ++I)
	{
		FBat& Bat = Bats.AddDefaulted_GetRef();
		Bat.bCrossing = I >= 6;
		if (Bat.bCrossing)
		{
			Bat.Center = FVector(0.f, (-560.f + 40.f * (I - 6)) * Fit, (250.f + 70.f * (I - 6)) * Fit);
			Bat.Radius = FVector(1250.f, 0.f, 50.f) * Fit;
			Bat.Speed = FMath::FRandRange(0.11f, 0.16f);
		}
		else
		{
			const float Side = (I % 2 == 0) ? -1.f : 1.f;
			Bat.Center = FVector(Side * CauldronX, CauldronY - 30.f * Fit, (380.f + 50.f * (I / 2)) * Fit);
			Bat.Radius = FVector(FMath::FRandRange(200.f, 290.f), FMath::FRandRange(90.f, 160.f), FMath::FRandRange(40.f, 90.f)) * Fit;
			Bat.Speed = FMath::FRandRange(0.5f, 0.9f) * (FMath::RandBool() ? 1.f : -1.f);
		}
		Bat.Phase = FMath::FRandRange(0.f, 2.f * PI);
		Bat.FlapRate = FMath::FRandRange(8.f, 12.f);
		Bat.Size = FMath::FRandRange(105.f, 150.f) * Fit;
		Bat.Mesh = AddPart(PlaneMesh, Bat.Center, FVector(Bat.Size / 100.f), nullptr, false);
		Bat.Mesh->SetWorldRotation(FacingCamera);
		Bat.Mesh->SetMaterial(0, BatMaterial);
	}
}

void AHalloweenProps::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Time += DeltaTime;
	if (Time >= NextFitCheck)
	{
		NextFitCheck = Time + 0.5f;
		FitToCamera();
	}
	const FVector Origin = GetActorLocation();

	for (FBubble& Bubble : Bubbles)
	{
		Bubble.Age += DeltaTime;
		if (Bubble.Age > Bubble.Period)
		{
			Bubble.Age -= Bubble.Period;
		}
		const float U = Bubble.Age / Bubble.Period;
		// Swells, then pops: the last tenth of its life it is gone.
		const float Grow = U < 0.9f ? FMath::Sin(U / 0.9f * HALF_PI) : 0.f;
		Bubble.Mesh->SetWorldScale3D(FVector(FMath::Max(Bubble.Size * Grow / 100.f, 0.001f)));
		Bubble.Mesh->SetWorldLocation(Origin + Bubble.Base + FVector(0.f, 0.f, Bubble.Size * 0.4f * Grow));
	}

	for (FPuff& Puff : Puffs)
	{
		Puff.Age += DeltaTime;
		if (Puff.Age > Puff.Life)
		{
			Puff.Age -= Puff.Life;
			Puff.Drift = FMath::FRandRange(-70.f, 70.f);
		}
		const float U = Puff.Age / Puff.Life;
		const FVector Location = Origin + Puff.Top + FVector(Puff.Drift * U + FMath::Sin(Time * 0.9f + Puff.Top.X) * 18.f * U, 0.f, 340.f * CauldronScale / 0.85f * U);
		Puff.Mesh->SetWorldLocation(Location);
		const float Size = Puff.Size * (0.4f + 0.9f * U);
		Puff.Mesh->SetWorldScale3D(FVector(Size / 100.f, Size / 100.f, 1.f));
		if (Puff.MID)
		{
			Puff.MID->SetScalarParameterValue(TEXT("Opacity"), 1.3f * FMath::Sin(U * PI));
		}
	}

	for (FBat& Bat : Bats)
	{
		const float Angle = Time * Bat.Speed + Bat.Phase;
		FVector Location;
		if (Bat.bCrossing)
		{
			// Back and forth across the top, bobbing.
			Location = Bat.Center + FVector(Bat.Radius.X * FMath::Sin(Angle * 2.f), 0.f, Bat.Radius.Z * FMath::Sin(Time * 1.7f + Bat.Phase));
		}
		else
		{
			Location = Bat.Center + FVector(Bat.Radius.X * FMath::Cos(Angle), Bat.Radius.Y * FMath::Sin(Angle), Bat.Radius.Z * FMath::Sin(Time * 1.3f + Bat.Phase));
		}
		Bat.Mesh->SetWorldLocation(Origin + Location);
		// The wings beat: the silhouette squashes up and down.
		const float Flap = 0.5f + 0.5f * FMath::Abs(FMath::Sin(Time * Bat.FlapRate + Bat.Phase));
		Bat.Mesh->SetWorldScale3D(FVector(Bat.Size / 100.f, Bat.Size / 100.f * (0.45f + 0.55f * Flap), 1.f));
	}

	for (const FCauldron& Cauldron : Cauldrons)
	{
		if (Cauldron.Light)
		{
			Cauldron.Light->SetIntensity(60.f * (0.85f + 0.15f * FMath::Sin(Time * 3.1f + Cauldron.Seed) + 0.08f * FMath::Sin(Time * 9.7f + Cauldron.Seed)));
		}
	}
}
