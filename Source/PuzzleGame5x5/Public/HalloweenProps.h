#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HalloweenProps.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Halloween scenery on either side of the board: two iron cauldrons of bubbling lime-green liquid with steam
// curling up from them, and bats that circle above the cauldrons or cross the top of the scene.
UCLASS()
class PUZZLEGAME5X5_API AHalloweenProps : public AActor
{
	GENERATED_BODY()

public:
	AHalloweenProps();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Where the two cauldrons stand (left is minus X) and how big they are. The defaults suit the 8x8 board; the actor
	// re-fits them to the camera whenever the board size changes the framing.
	UPROPERTY(EditAnywhere, Category = "Halloween")
	float CauldronX = 780.f;

	UPROPERTY(EditAnywhere, Category = "Halloween")
	float CauldronY = 60.f;

	UPROPERTY(EditAnywhere, Category = "Halloween")
	float CauldronScale = 0.85f;

private:
	struct FPuff
	{
		TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		FVector Top = FVector::ZeroVector; // where it leaves the liquid
		float Age = 0.f;
		float Life = 3.5f;
		float Drift = 0.f;
		float Size = 100.f;
	};

	struct FBubble
	{
		TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
		FVector Base = FVector::ZeroVector;
		float Age = 0.f;
		float Period = 1.f;
		float Size = 20.f;
	};

	struct FBat
	{
		TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
		FVector Center = FVector::ZeroVector;
		FVector Radius = FVector::ZeroVector; // orbit radii; for a crossing bat X is the half-width of its pass
		float Speed = 1.f;
		float Phase = 0.f;
		float FlapRate = 9.f;
		float Size = 130.f;
		bool bCrossing = false;
	};

	struct FCauldron
	{
		TObjectPtr<UPointLightComponent> Light = nullptr;
		float Seed = 0.f;
	};

	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale, UMaterialInterface* Material, bool bCastShadow);
	void BuildCauldron(float Side);
	void BuildBats();
	void BuildAll();
	void ClearAll();

	// Looks where the left and right edges of the view meet the floor and rebuilds the scenery if they moved.
	void FitToCamera();

	UPROPERTY()
	TArray<TObjectPtr<UActorComponent>> Owned;

	float NextFitCheck = 0.f;

	TArray<FPuff> Puffs;
	TArray<FBubble> Bubbles;
	TArray<FBat> Bats;
	TArray<FCauldron> Cauldrons;
	FRotator FacingCamera = FRotator::ZeroRotator;
	float Time = 0.f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> IronMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LiquidMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SteamMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BatMaterial;
};
