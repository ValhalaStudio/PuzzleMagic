#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleFX.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// One-shot magic effects built from additive quads (no Niagara asset needed):
// twinkling star sparkles, a glow sweep over a cleared line, and expanding rings.
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

	// Lightning bolt from the sky down to Ground: stroke, return stroke, flickering afterglow.
	void AddBolt(const FVector& Ground, float Height, float Width, const FLinearColor& Color, float Delay, float Seed);

	// A real (shadow-casting, ray-traced) point light. Flicker = lightning's triple stroke, else a smooth fade.
	void AddFlash(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius, float Delay, float Life, bool bFlicker);

	// Spinning rune circle lying flat (M_HolyAura), grows in and fades out. Spin in degrees/second.
	void AddAura(const FVector& Center, float Size, const FLinearColor& Color, float Delay, float Life, float Spin);

	virtual void Tick(float DeltaTime) override;

private:
	enum class EKind : uint8 { Sparkle, Strip, Ring, Bolt, Aura };

	struct FFlash
	{
		TObjectPtr<class UPointLightComponent> Light = nullptr;
		float Delay = 0.f;
		float Life = 1.f;
		float Candelas = 0.f;
		bool bFlicker = false;
	};
	TArray<FFlash> Flashes;

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
		// Bolts stand upright facing the camera instead of lying flat with a yaw.
		bool bUpright = false;
	};

	FParticle& AddQuad(EKind Kind, const FVector& Location, const FLinearColor& Color, float Shape, float Delay, float Life, UMaterialInterface* Material = nullptr);

	// Lightning's brightness S seconds after the strike.
	static float StrokeCurve(float S);

	TArray<FParticle> Particles;
	float Elapsed = 0.f;
	float EndTime = 0.f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FXMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BoltMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> AuraMaterial;
};
