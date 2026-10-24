#include "DeskEnvironment.h"
#include "PuzzleLite.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// World layout, shared with the cathedral: the board sits at the origin on the floor plane, the camera stands on the +Y side
	// and looks toward -Y, so things "behind the board" have negative Y.
	constexpr float DeskTopZ = 0.6f;
	constexpr float DeskWallY = -2600.f;
}

ADeskEnvironment::ADeskEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Daylight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Daylight"));
	Daylight->SetupAttachment(RootComponent);
	BoardLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("BoardLight"));
	BoardLight->SetupAttachment(RootComponent);
}

UStaticMeshComponent* ADeskEnvironment::AddMesh(UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bCastShadow)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(bCastShadow);
	Component->RegisterComponent();
	Component->SetWorldTransform(Transform);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	Owned.Add(Component);
	return Component;
}

UMaterialInstanceDynamic* ADeskEnvironment::PropMaterial(const FLinearColor& Color, float Roughness, float Metal)
{
	if (!PropBase)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PropBase, this);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Rough"), Roughness);
	MID->SetScalarParameterValue(TEXT("Metal"), Metal);
	return MID;
}

void ADeskEnvironment::BeginPlay()
{
	Super::BeginPlay();
	const bool bLite = PuzzleLite::IsLite();

	// Loaded here, not in the constructor, so the editor script can rebuild these materials in place.
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	PropBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_DeskProp.M_DeskProp"));
	UMaterialInterface* WoodMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_DeskWood.M_DeskWood"));
	if (!PlaneMesh || !WoodMaterial || !PropBase)
	{
		UE_LOG(LogTemp, Warning, TEXT("DeskEnvironment: the desk materials are missing (run Tools/build_desk_materials.py)"));
	}

	// The desk top: one big plane, the planks are drawn by the material from the world position.
	if (PlaneMesh)
	{
		AddMesh(PlaneMesh, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, DeskTopZ), FVector(80.f, 80.f, 1.f)), WoodMaterial, false);

		// A plain wall far behind the desk, so the top of the picture is never the void.
		const FRotator FacingCamera = FRotationMatrix::MakeFromZX(FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f)).Rotator();
		AddMesh(PlaneMesh, FTransform(FacingCamera, FVector(0.f, DeskWallY, 1400.f), FVector(80.f, 30.f, 1.f)), PropMaterial(FLinearColor(0.20f, 0.19f, 0.18f), 1.f), false);
	}
	BuildProps();

	// Soft daylight from the front left: a gentle shadow under the objects, none in the lightweight mode.
	const FVector Toward = FVector(0.3f, -0.55f, -0.78f).GetSafeNormal();
	Daylight->SetWorldRotation(Toward.Rotation());
	Daylight->SetIntensity(DaylightLux);
	Daylight->SetLightColor(FLinearColor(1.f, 0.95f, 0.88f));
	Daylight->SetCastShadows(!bLite);

	// The same key light as the cathedral over the play area, so the tiles keep the look they were tuned in.
	const FVector LightLocation(0.f, 1300.f, 1700.f);
	BoardLight->SetWorldLocation(LightLocation);
	BoardLight->SetWorldRotation((FVector(0.f, 60.f, 0.f) - LightLocation).Rotation());
	BoardLight->SetIntensityUnits(ELightUnits::Candelas);
	BoardLight->SetIntensity(BoardLightIntensity);
	BoardLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.88f));
	BoardLight->SetInnerConeAngle(16.f);
	BoardLight->SetOuterConeAngle(26.f);
	BoardLight->SetAttenuationRadius(4000.f);
	BoardLight->SetSourceRadius(60.f);
	BoardLight->SetCastShadows(false);

	// The map's own moonlight belongs to the cathedral; a second directional light only makes the engine warn about forward shading.
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		It->GetLightComponent()->SetVisibility(false);
	}

	// The map's height fog belongs to the cathedral: a clear desk has none of it.
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		It->GetComponent()->SetVolumetricFog(false);
		It->GetComponent()->SetFogDensity(0.f);
	}
}

void ADeskEnvironment::BuildProps()
{
	if (!CubeMesh || !CylinderMesh || !ConeMesh)
	{
		return;
	}
	// A ceramic mug with coffee in it, behind the board on the left. The cylinder mesh is 100 wide and 100 tall.
	{
		const FVector Base(-560.f, -380.f, DeskTopZ);
		AddMesh(CylinderMesh, FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, 38.f), FVector(0.62f, 0.62f, 0.76f)), PropMaterial(FLinearColor(0.62f, 0.64f, 0.66f), 0.35f), true);
		AddMesh(CylinderMesh, FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, 77.5f), FVector(0.54f, 0.54f, 0.02f)), PropMaterial(FLinearColor(0.03f, 0.015f, 0.008f), 0.2f), false);
		AddMesh(CubeMesh, FTransform(FRotator::ZeroRotator, Base + FVector(34.f, 0.f, 40.f), FVector(0.1f, 0.34f, 0.46f)), PropMaterial(FLinearColor(0.62f, 0.64f, 0.66f), 0.35f), true);
	}
	// A pencil lying along the front-left edge of the picture.
	{
		const FVector Base(-380.f, -520.f, DeskTopZ);
		const FRotator Along(90.f, 12.f, 0.f); // the cylinder's axis is Z; this lays it along X
		AddMesh(CylinderMesh, FTransform(Along, Base + FVector(0.f, 0.f, 7.f), FVector(0.14f, 0.14f, 3.6f)), PropMaterial(FLinearColor(0.60f, 0.38f, 0.04f), 0.5f), true);
		const FVector Tip = Base + FVector(FMath::Cos(FMath::DegreesToRadians(12.f)) * 195.f, FMath::Sin(FMath::DegreesToRadians(12.f)) * 195.f, 7.f);
		AddMesh(ConeMesh, FTransform(FRotator(-90.f, 12.f, 0.f), Tip, FVector(0.14f, 0.14f, 0.34f)), PropMaterial(FLinearColor(0.55f, 0.36f, 0.2f), 0.7f), true);
	}
	// A closed notebook on the right, a little crooked.
	{
		const FRotator Crooked(0.f, -14.f, 0.f);
		const FVector Base(580.f, -400.f, DeskTopZ);
		AddMesh(CubeMesh, FTransform(Crooked, Base + FVector(0.f, 0.f, 3.f), FVector(2.2f, 3.0f, 0.06f)), PropMaterial(FLinearColor(0.025f, 0.07f, 0.09f), 0.6f), true);
		AddMesh(CubeMesh, FTransform(Crooked, Base + FVector(0.f, 0.f, 11.f), FVector(2.05f, 2.88f, 0.1f)), PropMaterial(FLinearColor(0.75f, 0.73f, 0.66f), 0.9f), true);
		AddMesh(CubeMesh, FTransform(Crooked, Base + FVector(0.f, 0.f, 19.f), FVector(2.2f, 3.0f, 0.06f)), PropMaterial(FLinearColor(0.025f, 0.07f, 0.09f), 0.6f), true);
	}
}
