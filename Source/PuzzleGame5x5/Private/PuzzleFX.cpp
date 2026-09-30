#include "PuzzleFX.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
	PlaneMesh = PlaneFinder.Object;
	FXMaterial = MaterialFinder.Object;
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

void APuzzleFX::AddStream(const TArray<FVector>& Points, const FLinearColor& Color, float Delay, float Speed)
{
	if (Points.Num() < 2 || Speed <= 0.f)
	{
		return;
	}
	FStream NewStream;
	NewStream.Points = Points;
	NewStream.Cumulative.Add(0.f);
	for (int32 I = 1; I < Points.Num(); ++I)
	{
		NewStream.Cumulative.Add(NewStream.Cumulative.Last() + FVector::Dist(Points[I - 1], Points[I]));
	}
	const float Total = NewStream.Cumulative.Last();
	if (Total < 1.f)
	{
		return;
	}
	const int32 StreamIndex = Streams.Add(MoveTemp(NewStream));

	// Droplets leave the start one after another, each running the whole path: a continuous flow.
	const float Life = Total / Speed;
	const float Gap = 0.05f;
	const int32 Drops = FMath::Clamp(FMath::CeilToInt(Life / Gap) + 4, 6, 70);
	for (int32 I = 0; I < Drops; ++I)
	{
		FParticle& P = AddQuad(EKind::Drop, Points[0], Color, ShapeStrip, Delay + I * Gap, Life);
		P.Stream = StreamIndex;
		P.BaseScale = FVector2D(0.95f, 0.5f);
		P.Intensity = 3.2f;
	}
}

void APuzzleFX::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Elapsed += DeltaTime;

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
		case EKind::Drop:
		{
			const FStream& Path = Streams[P.Stream];
			const float Distance = T * Path.Cumulative.Last();
			int32 Segment = 1;
			while (Segment < Path.Cumulative.Num() - 1 && Path.Cumulative[Segment] < Distance)
			{
				++Segment;
			}
			const float SegmentLength = FMath::Max(Path.Cumulative[Segment] - Path.Cumulative[Segment - 1], 0.01f);
			const float Along = FMath::Clamp((Distance - Path.Cumulative[Segment - 1]) / SegmentLength, 0.f, 1.f);
			const FVector Heading = Path.Points[Segment] - Path.Points[Segment - 1];
			P.Mesh->SetWorldLocation(FMath::Lerp(Path.Points[Segment - 1], Path.Points[Segment], Along));
			P.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Heading.Y, Heading.X));
			Intensity *= FMath::Min(1.f, T * 6.f) * FMath::Min(1.f, (1.f - T) * 6.f);
			break;
		}
		}

		P.Mesh->SetWorldRotation(FRotator(0.f, P.Yaw, 0.f));
		P.Mesh->SetWorldScale3D(FVector(FMath::Max(Scale.X, 0.001f), FMath::Max(Scale.Y, 0.001f), 1.f));
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
