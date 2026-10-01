#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleFX.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// One-shot magic effects built from additive quads (no Niagara asset needed):
// twinkling star sparkles, glow sweeps, expanding rings and streams of glowing liquid that run along a cleared route.
// Destroys itself once every effect has finished.
UCLASS()
class PUZZLEGAME5X5_API APuzzleFX : public AActor
{
	GENERATED_BODY()

public:
	APuzzleFX();

	void AddSparkle(const FVector& Location, const FLinearColor& Color, float Delay);
	void AddStrip(const FVector& Center, const FVector2D& Size, float YawDegrees, const FLinearColor& Color, float Delay);
	void AddRing(const FVector& Center, float Radius, const FLinearColor& Color, float Delay);

	// A stream of glowing droplets that runs along a path of points at Speed (units per second), starting after Delay.
	void AddStream(const TArray<FVector>& Points, const FLinearColor& Color, float Delay, float Speed);


	virtual void Tick(float DeltaTime) override;

private:
	enum class EKind : uint8 { Sparkle, Strip, Ring, Drop };

	struct FStream
	{
		TArray<FVector> Points;
		TArray<float> Cumulative; // distance along the path at each point
	};
	TArray<FStream> Streams;

	struct FParticle
	{
		TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		EKind Kind = EKind::Sparkle;
		FVector Velocity = FVector::ZeroVector;
		FVector2D BaseScale = FVector2D(1.f, 1.f);
		float Delay = 0.f;
		float Life = 1.f;
		float Spin = 0.f;
		float Yaw = 0.f;
		float Intensity = 1.f;
		int32 Stream = INDEX_NONE; // the stream a Drop runs along
	};

	FParticle& AddQuad(EKind Kind, const FVector& Location, const FLinearColor& Color, float Shape, float Delay, float Life, UMaterialInterface* Material = nullptr);

	TArray<FParticle> Particles;
	float Elapsed = 0.f;
	float EndTime = 0.f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FXMaterial;

};
