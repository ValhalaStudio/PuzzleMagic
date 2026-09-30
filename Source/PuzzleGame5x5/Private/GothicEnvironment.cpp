#include "GothicEnvironment.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "MeshBuffersUtil.h"
#include "EldritchNoise.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/LocalFogVolume.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundSubmix.h"
#include "EffectConvolutionReverb.h"
#include "SubmixEffects/SubmixEffectConvolutionReverb.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "PuzzleGameMode.h"
#include "PuzzleManager.h"

namespace GothicLayout
{
	constexpr float WallY = -1050.f;
	constexpr float WallWidth = 2600.f;
	constexpr float WallHeight = 1600.f;
	// Direction from the scene toward the camera (camera looks toward -Y, pitched down).
	const FVector TowardCamera = FVector(0.f, 0.57f, 0.82f).GetSafeNormal();
}

AGothicEnvironment::AGothicEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BoardLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("BoardLight"));
	BoardLight->SetupAttachment(RootComponent);

	LightningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightningLight"));
	LightningLight->SetupAttachment(RootComponent);
	LightningLight->SetIntensityUnits(ELightUnits::Candelas);
	LightningLight->SetIntensity(0.f);
	LightningLight->SetLightColor(FLinearColor(0.72f, 0.8f, 1.f));
	LightningLight->SetAttenuationRadius(5000.f);
	LightningLight->SetSourceRadius(80.f);
	LightningLight->SetCastShadows(true);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MistFinder(TEXT("/Game/Materials/M_GroundMist2.M_GroundMist2"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ThunderFinder(TEXT("/Game/Audio/SFXG_Thunder.SFXG_Thunder"));
	MistMaterial = MistFinder.Object;
	ThunderSound = ThunderFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallFinder(TEXT("/Game/Materials/M_GothicWall.M_GothicWall"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassFinder(TEXT("/Game/Materials/M_StainedGlass.M_StainedGlass"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleFinder(TEXT("/Game/Materials/M_Marble.M_Marble"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlameFinder(TEXT("/Game/Materials/M_FXFlame.M_FXFlame"));
	PlaneMesh = PlaneFinder.Object;
	CylinderMesh = CylinderFinder.Object;
	WallMaterial = WallFinder.Object;
	GlassMaterial = GlassFinder.Object;
	MarbleMaterial = MarbleFinder.Object;
	FlameMaterial = FlameFinder.Object;
}

UStaticMeshComponent* AGothicEnvironment::AddMesh(UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bCastShadow)
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
	return Component;
}

void AGothicEnvironment::AddCandle(const FVector& Base, float Height, float Radius, bool bCastShadows)
{
	UMaterialInstanceDynamic* Wax = UMaterialInstanceDynamic::Create(MarbleMaterial, this);
	Wax->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.78f, 0.7f, 0.52f));
	Wax->SetScalarParameterValue(TEXT("VeinStrength"), 0.f);
	Wax->SetScalarParameterValue(TEXT("Roughness"), 0.45f);
	// Engine cylinder: 100 units wide and tall, pivot at its centre.
	AddMesh(CylinderMesh, FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(Radius / 50.f, Radius / 50.f, Height / 100.f)), Wax, true);

	const float Seed = FMath::FRandRange(0.f, 100.f);
	const FVector FlameCenter = Base + FVector(0.f, 0.f, Height + Radius * 1.3f);

	UMaterialInstanceDynamic* Flame = UMaterialInstanceDynamic::Create(FlameMaterial, this);
	Flame->SetScalarParameterValue(TEXT("Seed"), Seed);
	Flame->SetScalarParameterValue(TEXT("Intensity"), 4.f);
	const FRotator FlameRotation = FRotationMatrix::MakeFromZX(GothicLayout::TowardCamera, FVector(1.f, 0.f, 0.f)).Rotator();
	AddMesh(PlaneMesh, FTransform(FlameRotation, FlameCenter, FVector(Radius * 1.8f / 100.f, Radius * 3.6f / 100.f, 1.f)), Flame, false);

	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->RegisterComponent();
	Light->SetWorldLocation(FlameCenter);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(CandleIntensity);
	Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.24f));
	Light->SetAttenuationRadius(700.f);
	Light->SetSourceRadius(4.f);
	Light->SetCastShadows(bCastShadows);

	FCandle& Candle = Candles.AddDefaulted_GetRef();
	Candle.Light = Light;
	Candle.Flame = Flame;
	Candle.FlameCenter = FlameCenter;
	Candle.Seed = Seed;
	Candle.BaseIntensity = CandleIntensity;
}

void AGothicEnvironment::BeginPlay()
{
	Super::BeginPlay();
	using namespace GothicLayout;

	// Wall and the stained glass just behind it share one layout; the wall's
	// window areas are cut out so light and the glass both show through.
	const FRotator FacingCamera = FRotationMatrix::MakeFromZX(FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f)).Rotator();
	const FVector WallScale(WallWidth / 100.f, WallHeight / 100.f, 1.f);
	const FLinearColor WallSize(WallWidth, WallHeight, 0.f, 0.f);
	const FLinearColor Flip(WallUVFlip.X, WallUVFlip.Y, 0.f, 0.f);

	UMaterialInstanceDynamic* Wall = UMaterialInstanceDynamic::Create(WallMaterial, this);
	Wall->SetVectorParameterValue(TEXT("WallSize"), WallSize);
	Wall->SetVectorParameterValue(TEXT("FlipUV"), Flip);
	AddMesh(PlaneMesh, FTransform(FacingCamera, FVector(0.f, WallY, WallHeight * 0.5f), WallScale), Wall, true);

	UMaterialInstanceDynamic* Glass = UMaterialInstanceDynamic::Create(GlassMaterial, this);
	Glass->SetVectorParameterValue(TEXT("WallSize"), WallSize);
	Glass->SetVectorParameterValue(TEXT("FlipUV"), Flip);
	Glass->SetScalarParameterValue(TEXT("Intensity"), StainedGlassIntensity);
	AddMesh(PlaneMesh, FTransform(FacingCamera, FVector(0.f, WallY - 8.f, WallHeight * 0.5f), WallScale), Glass, false);
	GlassMID = Glass;

	// The storm sits outside, high behind the windows.
	LightningLight->SetWorldLocation(FVector(0.f, WallY - 450.f, WallHeight * 0.75f));

	// Two drifting mist layers: a dense one hugging the floor and a thin one at tile height.
	if (MistMaterial)
	{
		const FVector MistScale(60.f, 60.f, 1.f);
		// High enough above the floor that the shader's soft depth fade (90 units) leaves it visible.
		UMaterialInstanceDynamic* FloorMist = UMaterialInstanceDynamic::Create(MistMaterial, this);
		FloorMist->SetScalarParameterValue(TEXT("Density"), 1.f);
		FloorMist->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.45f, 0.6f));
		AddMesh(PlaneMesh, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 40.f), MistScale), FloorMist, false);

		UMaterialInstanceDynamic* HighMist = UMaterialInstanceDynamic::Create(MistMaterial, this);
		HighMist->SetScalarParameterValue(TEXT("Density"), 0.45f);
		HighMist->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.4f, 0.62f));
		AddMesh(PlaneMesh, FTransform(FRotator(0.f, 37.f, 0.f), FVector(0.f, 0.f, 95.f), MistScale), HighMist, false);
	}

	UMaterialInstanceDynamic* Stone = UMaterialInstanceDynamic::Create(MarbleMaterial, this);
	Stone->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.09f, 0.085f, 0.095f));
	Stone->SetVectorParameterValue(TEXT("VeinColor"), FLinearColor(0.2f, 0.19f, 0.2f));
	Stone->SetScalarParameterValue(TEXT("VeinStrength"), 0.3f);
	Stone->SetScalarParameterValue(TEXT("Roughness"), 0.8f);
	// Clustered gothic piers modelled and baked in Blender (Tools/blender_pier.py): a core with eight
	// engaged shafts, moulded base and annulet, triplanar carved stone (M_PierStone). The plain
	// cylinders remain as the fallback if the mesh is missing.
	UStaticMesh* PierMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Meshes/SM_GothicPier.SM_GothicPier"));
	for (float X : { -960.f, -320.f, 320.f, 960.f })
	{
		if (PierMesh)
		{
			// The mesh is 16 m tall with its base at the origin, the wall's full height.
			const float Scale = WallHeight / FMath::Max(PierMesh->GetBounds().BoxExtent.Z * 2.f, 1.f);
			AddMesh(PierMesh, FTransform(FRotator::ZeroRotator, FVector(X, WallY + 60.f, 0.f), FVector(Scale)), nullptr, true);
		}
		else
		{
			AddMesh(CylinderMesh, FTransform(FRotator::ZeroRotator, FVector(X, WallY + 50.f, WallHeight * 0.5f), FVector(0.8f, 0.8f, WallHeight / 100.f)), Stone, true);
		}
	}

	// Damp flagstones over the map's floor (ComfyUI seamless texture + DeepBump normals, M_FloorSlab):
	// the dark wet areas are glossy, so candles and lightning reflect in them.
	if (UMaterialInterface* Floor = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_FloorSlab.M_FloorSlab")))
	{
		AddMesh(PlaneMesh, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 0.6f), FVector(60.f, 60.f, 1.f)), Floor, false);
	}

	BuildAcoustics();

	// Tall candles behind the board read above it on screen; short ones sit in the front corners.
	AddCandle(FVector(-390.f, -580.f, 0.f), 150.f, 18.f, true);
	AddCandle(FVector(-300.f, -640.f, 0.f), 105.f, 15.f, false);
	AddCandle(FVector(-190.f, -600.f, 0.f), 70.f, 13.f, false);
	AddCandle(FVector(210.f, -610.f, 0.f), 85.f, 14.f, false);
	AddCandle(FVector(320.f, -650.f, 0.f), 125.f, 16.f, false);
	AddCandle(FVector(400.f, -575.f, 0.f), 165.f, 19.f, true);
	AddCandle(FVector(-410.f, 800.f, 0.f), 55.f, 14.f, false);
	AddCandle(FVector(410.f, 810.f, 0.f), 45.f, 13.f, false);

	// Soft key light over the play area so the pieces stay readable in the gloom. It sits
	// behind and above the camera: from there its mirror reflection off the glossy tile
	// tops can't reach the camera, so small pieces don't wash out to white.
	const FVector LightLocation(0.f, 1300.f, 1700.f);
	BoardLight->SetWorldLocation(LightLocation);
	BoardLight->SetWorldRotation((FVector(0.f, 60.f, 0.f) - LightLocation).Rotation());
	BoardLight->SetIntensityUnits(ELightUnits::Candelas);
	BoardLight->SetIntensity(BoardLightIntensity);
	BoardLight->SetLightColor(FLinearColor(1.f, 0.9f, 0.78f));
	BoardLight->SetInnerConeAngle(16.f);
	BoardLight->SetOuterConeAngle(26.f);
	BoardLight->SetAttenuationRadius(4000.f);
	BoardLight->SetSourceRadius(60.f);
	BoardLight->SetCastShadows(true);

	// Volumetric fog turns the moonlight through the window openings into visible shafts.
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		It->GetComponent()->SetVolumetricFog(true);
	}

	BuildEldritch();
}

namespace
{
	// Tapered tube along +Z that curls toward the camera (+Y). U runs around it, with U = 0.5
	// facing the camera (where M_Tentacle draws the suckers); V runs from base to tip. The skin is
	// knobbled with fractal noise (FastNoise2 on PC) so no two tentacles share a silhouette.
	ToonMesh::FBuffers BuildTentacleBuffers(float Height, float Radius, int32 Seed)
	{
		constexpr int32 Rings = 40;
		constexpr int32 Sides = 16;
		ToonMesh::FBuffers Out;
		TArray<FVector>& Vertices = Out.Vertices;
		TArray<FVector>& Normals = Out.Normals;
		TArray<FVector2D>& UVs = Out.UVs;
		TArray<int32>& Triangles = Out.Triangles;
		for (int32 R = 0; R <= Rings; ++R)
		{
			const float T = static_cast<float>(R) / Rings;
			// A lazy S sideways, and the upper half hooks toward the board.
			const FVector Spine(0.14f * Height * FMath::Sin(T * PI * 1.3f), 0.28f * Height * T * T * T, T * Height);
			const float Rad = Radius * FMath::Pow(1.f - T, 0.75f) + 1.f;
			for (int32 S = 0; S <= Sides; ++S)
			{
				const float U = static_cast<float>(S) / Sides;
				const float Angle = U * 2.f * PI - HALF_PI;
				const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
				Vertices.Add(Spine + Dir * Rad);
				Normals.Add(Dir);
				UVs.Add(FVector2D(U, T));
			}
		}
		// Rings counter-clockwise seen from +Z, same winding as ToonMesh's block walls (outward-facing).
		const int32 RingSize = Sides + 1;
		for (int32 R = 0; R < Rings; ++R)
		{
			for (int32 S = 0; S < Sides; ++S)
			{
				const int32 A = R * RingSize + S;
				const int32 B = A + 1;
				const int32 C = A + RingSize;
				const int32 D = C + 1;
				Triangles.Append({ A, B, C, B, D, C });
			}
		}

		// Knobbles and ridges: push each vertex along its normal by noise sampled on the surface.
		TArray<FVector3f> Points;
		Points.Reserve(Vertices.Num());
		for (const FVector& V : Vertices)
		{
			Points.Add(FVector3f(V));
		}
		TArray<float> Noise;
		EldritchNoise::Fractal3D(Points, Radius * 0.9f, Seed, Noise);
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			const float T = UVs[Index].Y;
			Vertices[Index] += Normals[Index] * Noise[Index] * Radius * 0.22f * (1.f - T);
		}
		Out.Colors.Init(FLinearColor::White, Vertices.Num());
		return Out;
	}
}

void AGothicEnvironment::BuildEldritch()
{
	using namespace GothicLayout;

	// Tentacles wait under the mist between the candles and the wall.
	if (UMaterialInterface* TentacleMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Tentacle.M_Tentacle")))
	{
		struct FSpec { float X; float Y; float Height; float Radius; float Threshold; };
		// In the gaps between the pillars (x = +-320), not in front of them.
		const FSpec Specs[] = {
			{ -170.f, -780.f, 600.f, 52.f, 0.15f }, { 480.f, -800.f, 680.f, 58.f, 0.3f }, { -500.f, -770.f, 560.f, 50.f, 0.45f },
			{ 140.f, -820.f, 460.f, 40.f, 0.6f }, { 20.f, -740.f, 380.f, 34.f, 0.75f },
			// Beyond the wall: giants between the storm and the windows. Unseen directly (the glass is
			// in the way), but every lightning flash throws their shadows into the nave.
			{ -640.f, WallY - 280.f, 1500.f, 150.f, 0.35f }, { 60.f, WallY - 360.f, 1800.f, 180.f, 0.5f },
			{ 660.f, WallY - 300.f, 1400.f, 140.f, 0.6f },
		};
		for (const FSpec& Spec : Specs)
		{
			URealtimeMeshComponent* Mesh = NewObject<URealtimeMeshComponent>(this);
			Mesh->SetupAttachment(RootComponent);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->RegisterComponent();
			ToonMesh::FBuffers Buffers = BuildTentacleBuffers(Spec.Height, Spec.Radius, Tentacles.Num() * 131 + 7);
			MeshBuffers::Optimize(Buffers);
			const ToonMesh::FBuffers* Parts[] = { &Buffers };
			Mesh->SetRealtimeMesh(MeshBuffers::BuildRealtimeMesh(Mesh, Parts));
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(TentacleMaterial, this);
			MID->SetScalarParameterValue(TEXT("Height"), Spec.Height);
			MID->SetScalarParameterValue(TEXT("Seed"), FMath::FRand());
			Mesh->SetMaterial(0, MID);
			Mesh->SetVisibility(false);

			FTentacle& Tentacle = Tentacles.AddDefaulted_GetRef();
			Tentacle.Mesh = Mesh;
			Tentacle.MID = MID;
			Tentacle.Base = FVector(Spec.X, Spec.Y, 0.f);
			Tentacle.Height = Spec.Height;
			Tentacle.Threshold = Spec.Threshold;
			Mesh->SetWorldLocation(Tentacle.Base - FVector(0.f, 0.f, Spec.Height * 1.05f));
		}
	}

	// Pairs of eyes in the dark between the pillars.
	if (UMaterialInterface* EyeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_EldritchEye.M_EldritchEye")))
	{
		const FRotator FacingCamera = FRotationMatrix::MakeFromZX(FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f)).Rotator();
		// Low on the wall: higher up, the HUD's badges cover it.
		const FVector2D Pairs[] = { { -600.f, 140.f }, { 590.f, 110.f }, { -240.f, 95.f }, { 250.f, 150.f }, { 10.f, 70.f } };
		for (int32 Pair = 0; Pair < UE_ARRAY_COUNT(Pairs); ++Pair)
		{
			const float Size = FMath::FRandRange(1.3f, 1.7f);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FVector Where(Pairs[Pair].X + (Side ? 55.f : -55.f) * Size, WallY + 30.f, Pairs[Pair].Y);
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(EyeMaterial, this);
				MID->SetScalarParameterValue(TEXT("Open"), 0.f);
				UStaticMeshComponent* Mesh = AddMesh(PlaneMesh, FTransform(FacingCamera, Where, FVector(0.9f * Size, 0.45f * Size, 1.f)), MID, false);
				Mesh->SetVisibility(false);

				FEye& Eye = Eyes.AddDefaulted_GetRef();
				Eye.Mesh = Mesh;
				Eye.MID = MID;
				Eye.Threshold = 0.3f + 0.12f * Pair;
				Eye.NextChange = FMath::FRandRange(2.f, 8.f);
			}
		}

		// The Watchers: one great eye in each lancet window, between the glass and the tracery.
		const FVector2D Windows[] = { { 0.f, 250.f }, { -640.f, 230.f }, { 640.f, 240.f } };
		for (int32 Window = 0; Window < UE_ARRAY_COUNT(Windows); ++Window)
		{
			const float Width = Window == 0 ? 2.6f : 2.2f;
			const FVector Where(Windows[Window].X, WallY - 4.f, Windows[Window].Y);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(EyeMaterial, this);
			MID->SetScalarParameterValue(TEXT("Open"), 0.f);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.55f, 0.15f));
			UStaticMeshComponent* Mesh = AddMesh(PlaneMesh, FTransform(FacingCamera, Where, FVector(Width, Width * 0.5f, 1.f)), MID, false);
			Mesh->SetVisibility(false);

			FEye& Eye = Eyes.AddDefaulted_GetRef();
			Eye.Mesh = Mesh;
			Eye.MID = MID;
			Eye.Threshold = 0.5f + 0.1f * Window;
			Eye.NextChange = FMath::FRandRange(4.f, 10.f);
		}
	}

	if (UMaterialInterface* MadnessMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_PPMadness.M_PPMadness")))
	{
		MadnessMID = UMaterialInstanceDynamic::Create(MadnessMaterial, this);
	}

	// Fog pooled on the floor behind the board and in front of the tray.
	struct FFogSpec { FVector Location; FVector Scale; float Density; };
	const FFogSpec Fogs[] = {
		// Thin: the camera looks through both, and thick fog would wash the pieces out.
		{ FVector(0.f, -780.f, -20.f), FVector(3.2f, 1.2f, 0.5f), 0.12f },
		{ FVector(0.f, 820.f, -80.f), FVector(3.f, 1.f, 0.3f), 0.05f },
	};
	for (const FFogSpec& Spec : Fogs)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		if (ALocalFogVolume* Fog = GetWorld()->SpawnActor<ALocalFogVolume>(ALocalFogVolume::StaticClass(), Spec.Location, FRotator::ZeroRotator, SpawnParams))
		{
			Fog->SetActorScale3D(Spec.Scale);
			if (ULocalFogVolumeComponent* Component = Fog->FindComponentByClass<ULocalFogVolumeComponent>())
			{
				Component->SetRadialFogExtinction(Spec.Density);
				Component->SetHeightFogExtinction(Spec.Density * 2.f);
				Component->SetHeightFogFalloff(900.f);
				Component->SetFogAlbedo(FLinearColor(0.55f, 0.6f, 0.62f));
			}
			FogVolumes.Add(Fog);
		}
	}

	BuildMotes();

	FlickerSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFXG_Flicker.SFXG_Flicker"));
	// The tension ambience is a MetaSound (MS_Abyss, built by Tools/build_allopts_assets.py): four
	// looping stems whose gains and a ladder-filter cutoff are set from dread every frame.
	// The flat AMB_Abyss mix is the fallback.
	if (USoundBase* Tension = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/MS_Abyss.MS_Abyss")))
	{
		AbyssAudio = UGameplayStatics::SpawnSound2D(this, Tension, 1.f, 1.f, 0.f, nullptr, true, false);
		bAbyssIsMetaSound = AbyssAudio != nullptr;
	}
	else if (USoundBase* Abyss = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/AMB_Abyss.AMB_Abyss")))
	{
		AbyssAudio = UGameplayStatics::SpawnSound2D(this, Abyss, 1.f, 1.f, 0.f, nullptr, true, false);
		if (AbyssAudio)
		{
			AbyssAudio->SetVolumeMultiplier(0.01f);
		}
	}
}

void AGothicEnvironment::TickEldritch(float Time, float DeltaTime, float& OutLightScale, float& OutEldritch)
{
	// Dread: nothing while luck holds above 30, full when it's gone.
	// -dread=N on the command line holds it at least at N (for testing and demo recordings).
	static const float MinDread = []() { float Value = 0.f; FParse::Value(FCommandLine::Get(), TEXT("dread="), Value); return FMath::Clamp(Value, 0.f, 1.f); }();
	const float Target = FMath::Clamp(FMath::Max((0.3f - Fortune) / 0.3f, MinDread), 0.f, 1.f);
	Dread = FMath::FInterpTo(Dread, Target, DeltaTime, 0.8f);

	// The room breathes: a slow 7-second swell and ebb of every light, deeper with dread.
	// Fractal noise on top keeps it from ever feeling mechanical.
	const float Breath = 1.f - 0.25f * Dread * (0.5f + 0.5f * FMath::Sin(Time * 2.f * PI / 7.f))
		- 0.1f * Dread * FMath::Max(EldritchNoise::Fractal1D(Time * 0.6f, 1928), 0.f);

	// Flicker episodes: every light stutters together, sometimes to near-black, then recovers.
	if (Time >= NextFlickerTime)
	{
		FlickerStart = Time;
		FlickerDuration = 0.7f + 1.1f * Dread;
		NextFlickerTime = Time + FMath::Lerp(45.f, 9.f, Dread) * FMath::FRandRange(0.7f, 1.3f);
		if (FlickerSound)
		{
			UGameplayStatics::PlaySound2D(this, FlickerSound, 0.25f + 0.35f * Dread);
		}
	}
	float Stutter = 1.f;
	const float U = (Time - FlickerStart) / FlickerDuration;
	if (U >= 0.f && U < 1.f)
	{
		// 16 Hz random on/off, dimmer the deeper the dread, easing back in over the last third.
		const float Step = FMath::Frac(FMath::Sin(FMath::FloorToFloat(Time * 16.f) * 91.7f) * 43758.5f);
		const float Low = FMath::Lerp(0.45f, 0.05f, Dread);
		const float Raw = Step > 0.45f ? 1.f : Low;
		Stutter = FMath::Lerp(Raw, 1.f, FMath::SmoothStep(0.65f, 1.f, U));
		OutEldritch = Dread * FMath::Sin(U * PI);
	}
	OutLightScale = Breath * Stutter;

	// Tentacles rise from the mist in turn as dread passes each one's threshold, and writhe harder.
	for (FTentacle& Tentacle : Tentacles)
	{
		const float Goal = FMath::Clamp((Dread - Tentacle.Threshold) / 0.3f, 0.f, 1.f);
		Tentacle.Emerge = FMath::FInterpTo(Tentacle.Emerge, Goal, DeltaTime, 0.6f);
		const bool bVisible = Tentacle.Emerge > 0.01f;
		Tentacle.Mesh->SetVisibility(bVisible);
		if (bVisible)
		{
			Tentacle.Mesh->SetWorldLocation(Tentacle.Base - FVector(0.f, 0.f, (1.f - Tentacle.Emerge) * Tentacle.Height * 1.05f));
			Tentacle.MID->SetScalarParameterValue(TEXT("Writhe"), 0.5f + 0.9f * Dread + (Stutter < 1.f ? 0.4f : 0.f));
			Tentacle.MID->SetScalarParameterValue(TEXT("Glow"), 1.5f * Dread * OutLightScale + 0.3f);
		}
	}

	// Eyes open one pair after another as dread deepens, look around, blink, and close again.
	for (FEye& Eye : Eyes)
	{
		if (Time >= Eye.NextChange)
		{
			const bool bAwake = Dread > Eye.Threshold && Eye.Target < 0.5f;
			Eye.Target = bAwake ? 1.f : 0.f;
			Eye.NextChange = Time + (bAwake ? FMath::FRandRange(3.f, 7.f) : FMath::FRandRange(2.f, 9.f));
			Eye.LookTarget = FMath::FRandRange(-1.f, 1.f);
		}
		if (Eye.Target > 0.5f && Eye.BlinkUntil < Time && FMath::FRand() < DeltaTime * 0.35f)
		{
			Eye.BlinkUntil = Time + 0.16f;
		}
		const float Want = (Time < Eye.BlinkUntil || Dread <= Eye.Threshold * 0.8f) ? 0.f : Eye.Target;
		Eye.Open = FMath::FInterpTo(Eye.Open, Want, DeltaTime, Time < Eye.BlinkUntil ? 25.f : 2.5f);
		Eye.Look = FMath::FInterpTo(Eye.Look, Eye.LookTarget, DeltaTime, 1.5f);
		const bool bVisible = Eye.Open > 0.01f;
		Eye.Mesh->SetVisibility(bVisible);
		if (bVisible)
		{
			Eye.MID->SetScalarParameterValue(TEXT("Open"), Eye.Open);
			Eye.MID->SetScalarParameterValue(TEXT("Look"), Eye.Look);
			Eye.MID->SetScalarParameterValue(TEXT("Intensity"), 3.f * FMath::Max(OutLightScale, 0.6f));
		}
	}

	// Madness post-process on the camera (attached once the player's pawn exists).
	if (MadnessMID && !bMadnessAttached)
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>())
			{
				Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, MadnessMID));
				bMadnessAttached = true;
			}
		}
	}
	if (MadnessMID)
	{
		MadnessMID->SetScalarParameterValue(TEXT("Madness"), FMath::Clamp(Dread * 0.85f + (Stutter < 1.f ? 0.25f : 0.f), 0.f, 1.f));
		MadnessMID->SetScalarParameterValue(TEXT("Exposure"), FMath::Lerp(1.f, Stutter, 0.45f));
	}

	if (AbyssAudio && bAbyssIsMetaSound)
	{
		// Calm: only the deep-water air. Rising dread brings in the drone, then the endless falling
		// Shepard tone, then the whispers, while the whole bed darkens (filter closes).
		AbyssAudio->SetVolumeMultiplier(0.35f);
		AbyssAudio->SetFloatParameter(TEXT("Air"), 1.f - 0.5f * Dread);
		AbyssAudio->SetFloatParameter(TEXT("Drone"), 0.15f + 0.85f * Dread);
		AbyssAudio->SetFloatParameter(TEXT("Shepard"), FMath::Pow(Dread, 1.5f));
		AbyssAudio->SetFloatParameter(TEXT("Whisper"), FMath::Clamp(1.6f * Dread - 0.5f, 0.f, 1.f) * (Stutter < 1.f ? 1.6f : 1.f));
		AbyssAudio->SetFloatParameter(TEXT("Cutoff"), FMath::Lerp(14000.f, 1800.f, Dread));
	}
	else if (AbyssAudio)
	{
		AbyssAudio->SetVolumeMultiplier(0.02f + 0.55f * Dread);
	}

	// The fog breathes with the room and thickens, faintly glowing green, as dread rises.
	for (int32 Index = 0; Index < FogVolumes.Num(); ++Index)
	{
		ULocalFogVolumeComponent* Fog = FogVolumes[Index] ? FogVolumes[Index]->FindComponentByClass<ULocalFogVolumeComponent>() : nullptr;
		if (Fog)
		{
			const float Base = Index == 0 ? 0.12f : 0.05f;
			const float Swell = 0.85f + 0.15f * FMath::Sin(Time * 2.f * PI / 7.f + Index);
			Fog->SetRadialFogExtinction(Base * (0.6f + 1.4f * Dread) * Swell);
			Fog->SetFogEmissive(FLinearColor(0.02f, 0.09f, 0.05f) * Dread * OutLightScale);
		}
	}

	TickMotes(Time, DeltaTime, OutLightScale);
}

void AGothicEnvironment::BuildAcoustics()
{
	USoundSubmix* Submix = LoadObject<USoundSubmix>(nullptr, TEXT("/Game/Audio/SM_Cathedral.SM_Cathedral"));
	if (!Submix)
	{
		return;
	}

	// Stereo impulse response, same recipe as Tools/cathedral_ir.py: sparse early reflections off
	// stone, then a noise tail whose highs die first (a one-pole low-pass fades in over time).
	constexpr int32 SampleRate = 44100;
	constexpr float Seconds = 5.5f;
	const int32 Frames = FMath::RoundToInt(SampleRate * Seconds);
	TArray<float> Samples;
	Samples.SetNumZeroed(Frames * 2);
	FRandomStream Random(1248);
	const float LowPassCoeff = FMath::Exp(-2.f * PI * 1800.f / SampleRate);
	for (int32 Channel = 0; Channel < 2; ++Channel)
	{
		float Dark = 0.f;
		for (int32 Frame = FMath::RoundToInt(0.01f * SampleRate); Frame < Frames; ++Frame)
		{
			const float T = static_cast<float>(Frame) / SampleRate;
			const float Noise = Random.FRandRange(-1.f, 1.f) * FMath::Exp(-T * 6.9f / Seconds);
			Dark = Dark * LowPassCoeff + Noise * (1.f - LowPassCoeff);
			const float Brightness = FMath::Exp(-T * 1.2f);
			Samples[Frame * 2 + Channel] = Noise * Brightness + Dark * 2.5f * (1.f - Brightness);
		}
		for (int32 Reflection = 0; Reflection < 14; ++Reflection)
		{
			const int32 Frame = FMath::RoundToInt(Random.FRandRange(0.012f, 0.11f) * SampleRate);
			Samples[Frame * 2 + Channel] += Random.FRandRange(0.3f, 0.9f) * (Random.FRand() < 0.5f ? -1.f : 1.f);
		}
	}

	CathedralIR = NewObject<UAudioImpulseResponse>(this);
	CathedralIR->ImpulseResponse = MoveTemp(Samples);
	CathedralIR->NumChannels = 2;
	CathedralIR->SampleRate = SampleRate;
	CathedralIR->NormalizationVolumeDb = -24.f;

	CathedralReverb = NewObject<USubmixEffectConvolutionReverbPreset>(this);
	FSubmixEffectConvolutionReverbSettings Settings = CathedralReverb->GetSettings();
	Settings.WetVolumeDb = -8.f;
	Settings.DryVolumeDb = -96.f; // the send carries only the wet signal; the dry sound plays as usual
	CathedralReverb->SetSettings(Settings);
	CathedralReverb->SetImpulseResponse(CathedralIR);
	UAudioMixerBlueprintLibrary::AddSubmixEffect(this, Submix, CathedralReverb);
}

void AGothicEnvironment::BuildMotes()
{
	UMaterialInterface* MoteMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Mote.M_Mote"));
	if (!MoteMaterial)
	{
		return;
	}
	MoteMesh = NewObject<UInstancedStaticMeshComponent>(this);
	MoteMesh->SetupAttachment(RootComponent);
	MoteMesh->SetStaticMesh(PlaneMesh);
	MoteMesh->SetMaterial(0, MoteMaterial);
	MoteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoteMesh->SetCastShadow(false);
	MoteMesh->NumCustomDataFloats = 4;
	MoteMesh->RegisterComponent();

	auto Add = [this](EMote Kind, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FMote& Mote = Motes.AddDefaulted_GetRef();
			Mote.Kind = Kind;
			Mote.Seed = FMath::FRand() * 100.f;
			RespawnMote(Mote, true);
			MoteMesh->AddInstance(FTransform(Mote.Position));
		}
	};
	Add(EMote::Dust, 70);
	Add(EMote::Ember, 24);
	Add(EMote::Wisp, 30);
}

void AGothicEnvironment::RespawnMote(FMote& Mote, bool bScatterAge)
{
	switch (Mote.Kind)
	{
	case EMote::Dust:
		// Hanging in the light over and around the board.
		Mote.Position = FVector(FMath::FRandRange(-520.f, 520.f), FMath::FRandRange(-700.f, 600.f), FMath::FRandRange(40.f, 420.f));
		Mote.Velocity = FVector(FMath::FRandRange(-6.f, 6.f), FMath::FRandRange(-6.f, 6.f), FMath::FRandRange(-3.f, 4.f));
		Mote.Life = FMath::FRandRange(6.f, 12.f);
		Mote.Size = FMath::FRandRange(2.5f, 5.f);
		break;
	case EMote::Ember:
		if (Candles.Num() > 0)
		{
			Mote.Position = Candles[FMath::RandRange(0, Candles.Num() - 1)].FlameCenter + FVector(FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-3.f, 3.f), 6.f);
		}
		Mote.Velocity = FVector(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(40.f, 80.f));
		Mote.Life = FMath::FRandRange(1.2f, 2.6f);
		Mote.Size = FMath::FRandRange(1.5f, 3.f);
		break;
	case EMote::Wisp:
		// Around the tentacles' bases, behind the candles.
		Mote.Position = FVector(FMath::FRandRange(-560.f, 560.f), FMath::FRandRange(-900.f, -720.f), FMath::FRandRange(20.f, 120.f));
		Mote.Velocity = FVector(0.f, 0.f, FMath::FRandRange(15.f, 35.f));
		Mote.Life = FMath::FRandRange(3.f, 6.f);
		Mote.Size = FMath::FRandRange(5.f, 9.f);
		break;
	}
	Mote.Age = bScatterAge ? FMath::FRandRange(0.f, Mote.Life) : 0.f;
}

void AGothicEnvironment::TickMotes(float Time, float DeltaTime, float LightScale)
{
	if (!MoteMesh || Motes.Num() == 0)
	{
		return;
	}
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraLocation = CameraManager ? CameraManager->GetCameraLocation() : FVector(0.f, 1500.f, 2000.f);

	TArray<FTransform> Transforms;
	Transforms.SetNumUninitialized(Motes.Num());
	for (int32 Index = 0; Index < Motes.Num(); ++Index)
	{
		FMote& Mote = Motes[Index];
		Mote.Age += DeltaTime;
		if (Mote.Age >= Mote.Life)
		{
			RespawnMote(Mote, false);
		}
		const float T = Mote.Age / Mote.Life;

		FLinearColor Color = FLinearColor::White;
		float Brightness = 0.f;
		switch (Mote.Kind)
		{
		case EMote::Dust:
			// Brownian drift; they only show where there is light to catch them.
			Mote.Velocity += FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.8f, 0.8f)) * 12.f * DeltaTime;
			Mote.Velocity *= FMath::Exp(-0.3f * DeltaTime);
			Color = FLinearColor(1.f, 0.78f, 0.45f);
			Brightness = 0.9f * FMath::Sin(T * PI) * (0.6f + 0.4f * FMath::Sin(Time * 3.f + Mote.Seed)) * LightScale;
			break;
		case EMote::Ember:
			Mote.Velocity.X += FMath::Sin(Time * 4.f + Mote.Seed) * 30.f * DeltaTime;
			Mote.Velocity.Z *= FMath::Exp(-0.4f * DeltaTime);
			Color = FMath::Lerp(FLinearColor(1.f, 0.55f, 0.15f), FLinearColor(0.35f, 1.f, 0.5f), Dread * 0.7f);
			Brightness = 6.f * FMath::Square(1.f - T) * (0.7f + 0.3f * FMath::Sin(Time * 25.f + Mote.Seed)) * LightScale;
			break;
		case EMote::Wisp:
		{
			// Spiral upward; only there when the dark is awake.
			const float Angle = Time * 1.3f + Mote.Seed;
			Mote.Velocity.X = FMath::Cos(Angle) * 40.f;
			Mote.Velocity.Y = FMath::Sin(Angle) * 25.f;
			Color = FLinearColor(0.25f, 1.f, 0.6f);
			Brightness = 3.f * FMath::Clamp((Dread - 0.2f) / 0.5f, 0.f, 1.f) * FMath::Sin(T * PI);
			break;
		}
		}
		Mote.Position += Mote.Velocity * DeltaTime;

		// Billboard: the quad faces the camera.
		const FVector ToCamera = (CameraLocation - Mote.Position).GetSafeNormal();
		const float Scale = Mote.Size / 100.f;
		Transforms[Index] = FTransform(FRotationMatrix::MakeFromZX(ToCamera, FVector(1.f, 0.f, 0.f)).Rotator(), Mote.Position, FVector(Scale, Scale, 1.f));
		MoteMesh->SetCustomData(Index, { Color.R, Color.G, Color.B, Brightness }, false);
	}
	MoteMesh->BatchUpdateInstancesTransforms(0, Transforms, true, true);
}

void AGothicEnvironment::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Time = GetWorld()->GetTimeSeconds();

	const APuzzleGameMode* GameMode = GetWorld()->GetAuthGameMode<APuzzleGameMode>();
	if (GameMode && GameMode->PuzzleManager)
	{
		const float Luck = GameMode->PuzzleManager->GetLuck() / static_cast<float>(UPuzzleManager::MaxLuck);
		Fortune = FMath::FInterpTo(Fortune, Luck, DeltaTime, 1.5f);
	}
	float LightScale = 1.f;
	float Eldritch = 0.f;
	TickEldritch(Time, DeltaTime, LightScale, Eldritch);

	// Fortune: guttering, reddish flames when luck runs dry; tall golden ones when it's high.
	// When the dark stirs, the flames stutter together and burn a drowned green.
	const float Burn = (0.75f + 0.55f * Fortune) * LightScale;
	const FLinearColor Warm = FMath::Lerp(FLinearColor(1.f, 0.42f, 0.16f), FLinearColor(1.f, 0.66f, 0.32f), Fortune);
	const FLinearColor FlameColor = FMath::Lerp(Warm, FLinearColor(0.35f, 1.f, 0.55f), Eldritch);
	for (const FCandle& Candle : Candles)
	{
		if (Candle.Light)
		{
			const float Gutter = (1.f - Fortune) * 0.08f * FMath::Sin(Time * 3.7f + Candle.Seed * 3.f);
			const float Flicker = 0.82f + 0.1f * FMath::Sin(Time * 9.3f + Candle.Seed)
				+ 0.06f * FMath::Sin(Time * 23.1f + Candle.Seed * 2.f) + FMath::FRandRange(-0.03f, 0.03f) + Gutter;
			Candle.Light->SetIntensity(Candle.BaseIntensity * Burn * Flicker);
			Candle.Light->SetLightColor(FlameColor);
		}
		if (Candle.Flame)
		{
			Candle.Flame->SetScalarParameterValue(TEXT("Intensity"), 4.f * FMath::Max(LightScale, 0.15f));
		}
	}
	// The board keeps most of its light, so the pieces stay readable through a stutter.
	BoardLight->SetIntensity(BoardLightIntensity * FMath::Lerp(1.f, LightScale, 0.55f));

	// Lightning: a bright strike, a weaker return stroke, then a longer flickering afterglow.
	// The ambient storm strikes more often the less luck there is.
	if (Time >= NextStrikeTime)
	{
		StrikeTime = Time;
		StrikeSeed = FMath::FRand() * 10.f;
		ThunderTime = Time + FMath::FRandRange(0.5f, 1.4f);
		const float Calm = FMath::Lerp(0.5f, 1.3f, Fortune);
		NextStrikeTime = Time + FMath::FRandRange(14.f, 26.f) * Calm;
	}
	const float S = Time - StrikeTime;
	float Flash = 0.f;
	if (S < 1.2f)
	{
		Flash = FMath::Exp(-FMath::Square(S / 0.04f))
			+ 0.55f * FMath::Exp(-FMath::Square((S - 0.14f) / 0.035f))
			+ 0.8f * FMath::Exp(-FMath::Square((S - 0.32f) / 0.09f)) * (0.7f + 0.3f * FMath::Sin(S * 90.f + StrikeSeed));
	}
	LightningLight->SetIntensity(LightningIntensity * Flash);
	LightningLight->SetVisibility(Flash > 0.01f);
	if (GlassMID)
	{
		GlassMID->SetScalarParameterValue(TEXT("Intensity"), StainedGlassIntensity * (1.f + 4.f * Flash));
	}
	if (ThunderTime > 0.f && Time >= ThunderTime)
	{
		ThunderTime = -1.f;
		if (ThunderSound)
		{
			UGameplayStatics::PlaySound2D(this, ThunderSound, ThunderVolume, FMath::FRandRange(0.85f, 1.1f));
		}
	}
}
