#include "PuzzleFX.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ShapeSparkle = 0.f;
	constexpr float ShapeStrip = 1.f;
	constexpr float ShapeRing = 2.f;
}

APuzzleFX::APuzzleFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Game/Materials/M_FXAdd.M_FXAdd"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AuraFinder(TEXT("/Game/Materials/M_HolyAura.M_HolyAura"));
	PlaneMesh = PlaneFinder.Object;
	FXMaterial = MaterialFinder.Object;
	AuraMaterial = AuraFinder.Object;
	// Loaded on first use rather than via ConstructorHelpers, which would root it in the content
	// commandlet and make Tools/build_storm_fx.py crash when it rebuilds the material.
	BoltMaterial = nullptr;
}

float APuzzleFX::StrokeCurve(float S)
{
	if (S < 0.f)
	{
		return 0.f;
	}
	// Main stroke, a weaker return stroke, then a flickering afterglow.
	return FMath::Exp(-FMath::Square(S / 0.05f))
		+ 0.6f * FMath::Exp(-FMath::Square((S - 0.14f) / 0.04f))
		+ 0.45f * FMath::Exp(-FMath::Square((S - 0.3f) / 0.07f)) * (0.7f + 0.3f * FMath::Sin(S * 90.f));
}

APuzzleFX::FParticle& APuzzleFX::AddQuad(EKind Kind, const FVector& Location, const FLinearColor& Color, float Shape, float Delay, float Life, UMaterialInterface* Material)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Mesh->SetStaticMesh(PlaneMesh);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->RegisterComponent();
	Mesh->SetWorldLocation(Location);
	Mesh->SetVisibility(false);

	UMaterialInterface* Base = Material ? Material : FXMaterial.Get();
	UMaterialInstanceDynamic* MID = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Shape"), Shape);
		MID->SetScalarParameterValue(TEXT("Intensity"), 0.f);
		Mesh->SetMaterial(0, MID);
	}

	FParticle& Particle = Particles.AddDefaulted_GetRef();
	Particle.Mesh = Mesh;
	Particle.MID = MID;
	Particle.Kind = Kind;
	Particle.Delay = Delay;
	Particle.Life = Life;
	EndTime = FMath::Max(EndTime, Delay + Life);
	return Particle;
}

void APuzzleFX::AddSparkle(const FVector& Location, const FLinearColor& Color, float Delay)
{
	FParticle& P = AddQuad(EKind::Sparkle, Location, Color, ShapeSparkle, Delay, FMath::FRandRange(0.8f, 1.3f));
	P.Velocity = FVector(FMath::FRandRange(-110.f, 110.f), FMath::FRandRange(-110.f, 110.f), FMath::FRandRange(80.f, 180.f));
	const float Size = FMath::FRandRange(0.35f, 0.7f);
	P.BaseScale = FVector2D(Size, Size);
	P.Spin = FMath::FRandRange(-220.f, 220.f);
	P.Yaw = FMath::FRandRange(0.f, 90.f);
	P.Intensity = FMath::FRandRange(2.5f, 4.f);
}

void APuzzleFX::AddStrip(const FVector& Center, const FVector2D& Size, float YawDegrees, const FLinearColor& Color, float Delay)
{
	FParticle& P = AddQuad(EKind::Strip, Center, Color, ShapeStrip, Delay, 0.6f);
	P.BaseScale = Size / 100.f;
	P.Yaw = YawDegrees;
	P.Intensity = 3.f;
}

void APuzzleFX::AddRing(const FVector& Center, float Radius, const FLinearColor& Color, float Delay)
{
	FParticle& P = AddQuad(EKind::Ring, Center, Color, ShapeRing, Delay, 0.7f);
	P.BaseScale = FVector2D(Radius * 2.f / 100.f, Radius * 2.f / 100.f);
	P.Intensity = 2.5f;
}

void APuzzleFX::AddBolt(const FVector& Ground, float Height, float Width, const FLinearColor& Color, float Delay, float Seed)
{
	// Upright quad facing the camera (which looks toward -Y from above). Plane local X = width,
	// local Y = height; the shader pins both ends to the centre line, so which end is "up" doesn't matter.
	const FVector Normal = FVector(0.f, 1.f, 0.3f).GetSafeNormal();
	const FMatrix Frame = FRotationMatrix::MakeFromZX(Normal, FVector(1.f, 0.f, 0.f));
	if (!BoltMaterial)
	{
		BoltMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_FXBolt.M_FXBolt"));
	}
	FVector Up = Frame.GetUnitAxis(EAxis::Y);
	if (Up.Z < 0.f)
	{
		Up = -Up;
	}
	FParticle& P = AddQuad(EKind::Bolt, Ground + Up * Height * 0.5f, Color, 0.f, Delay, 0.55f, BoltMaterial);
	P.bUpright = true;
	P.BaseScale = FVector2D(Width / 100.f, Height / 100.f);
	P.Intensity = 9.f;
	P.Mesh->SetWorldRotation(Frame.Rotator());
	P.Mesh->SetWorldScale3D(FVector(P.BaseScale.X, P.BaseScale.Y, 1.f));
	if (P.MID)
	{
		P.MID->SetScalarParameterValue(TEXT("Seed"), Seed);
	}
}

void APuzzleFX::AddFlash(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius, float Delay, float Life, bool bFlicker)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
	Light->SetupAttachment(RootComponent);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(Radius);
	Light->SetSourceRadius(bFlicker ? 6.f : 30.f);
	Light->SetCastShadows(true);
	Light->SetVisibility(false);
	Light->RegisterComponent();
	Light->SetWorldLocation(Location);

	FFlash& Flash = Flashes.AddDefaulted_GetRef();
	Flash.Light = Light;
	Flash.Delay = Delay;
	Flash.Life = Life;
	Flash.Candelas = Candelas;
	Flash.bFlicker = bFlicker;
	EndTime = FMath::Max(EndTime, Delay + Life);
}

void APuzzleFX::AddAura(const FVector& Center, float Size, const FLinearColor& Color, float Delay, float Life, float Spin)
{
	FParticle& P = AddQuad(EKind::Aura, Center, Color, 0.f, Delay, Life, AuraMaterial);
	P.BaseScale = FVector2D(Size / 100.f, Size / 100.f);
	P.Spin = Spin;
	P.Yaw = FMath::FRandRange(0.f, 360.f);
	P.Intensity = 1.f;
}

void APuzzleFX::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Elapsed += DeltaTime;

	for (const FFlash& Flash : Flashes)
	{
		if (!Flash.Light)
		{
			continue;
		}
		const float S = Elapsed - Flash.Delay;
		float Amount = 0.f;
		if (S >= 0.f && S < Flash.Life)
		{
			const float T = S / Flash.Life;
			Amount = Flash.bFlicker ? StrokeCurve(S) : FMath::Min(S / 0.08f, 1.f) * FMath::Square(1.f - T);
		}
		Flash.Light->SetVisibility(Amount > 0.01f);
		Flash.Light->SetIntensity(Flash.Candelas * Amount);
	}

	for (FParticle& P : Particles)
	{
		if (!P.Mesh || Elapsed < P.Delay)
		{
			continue;
		}

		const float T = (Elapsed - P.Delay) / P.Life;
		if (T >= 1.f)
		{
			P.Mesh->SetVisibility(false);
			continue;
		}
		P.Mesh->SetVisibility(true);

		float Intensity = P.Intensity;
		FVector2D Scale = P.BaseScale;
		switch (P.Kind)
		{
		case EKind::Sparkle:
			P.Velocity *= FMath::Exp(-2.2f * DeltaTime);
			P.Mesh->AddWorldOffset(P.Velocity * DeltaTime);
			P.Yaw += P.Spin * DeltaTime;
			// Pop in, twinkle, shrink out.
			Scale *= FMath::Sin(T * PI) * (0.85f + 0.15f * FMath::Sin(Elapsed * 30.f));
			break;
		case EKind::Strip:
			Intensity *= FMath::Square(1.f - T);
			Scale.Y *= 1.f + 0.6f * T;
			break;
		case EKind::Ring:
			Intensity *= 1.f - T;
			Scale *= 0.3f + 1.1f * FMath::Sqrt(T);
			break;
		case EKind::Bolt:
			Intensity *= StrokeCurve(Elapsed - P.Delay);
			break;
		case EKind::Aura:
		{
			// Swell in over the first fifth, hold, fade out over the last third.
			const float In = FMath::Min(T / 0.2f, 1.f);
			const float Out = FMath::Clamp((1.f - T) / 0.35f, 0.f, 1.f);
			Intensity *= In * Out;
			Scale *= 0.6f + 0.4f * FMath::Sin(In * HALF_PI);
			P.Yaw += P.Spin * DeltaTime;
			break;
		}
		}

		if (!P.bUpright)
		{
			P.Mesh->SetWorldRotation(FRotator(0.f, P.Yaw, 0.f));
			P.Mesh->SetWorldScale3D(FVector(FMath::Max(Scale.X, 0.001f), FMath::Max(Scale.Y, 0.001f), 1.f));
		}
		if (P.MID)
		{
			P.MID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		}
	}

	if (Elapsed >= EndTime)
	{
		Destroy();
	}
}
