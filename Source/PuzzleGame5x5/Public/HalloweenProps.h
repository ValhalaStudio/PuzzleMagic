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

	// For rendering the backdrop picture (-backdropcapture): hides the animated sprites and puffs.
	void SetCaptureMode();

	// For measuring: hide or show one layer of the lightweight scenery ("backdrop", "bats" or "steam").
	void SetLiteLayerHidden(const FString& Layer, bool bHide);

	// For measuring power: switch the automatic resolution off (or on again) and, if Resolution is above 0, set it (0.35 to 1).
	void SetPerfOverride(bool bAdapt, float Resolution);

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

	// --- Lightweight mode (phones and tablets, or -lite): a pre-rendered backdrop picture and flat sprites riding on the
	// camera replace the 3D cauldrons, mist and flying quads. See HalloweenLite.cpp.
	struct FLiteBat
	{
		TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		FVector2D Centre = FVector2D::ZeroVector; // in the backdrop picture's -1..1 space
		FVector2D Radius = FVector2D::ZeroVector;
		float Speed = 1.f;
		float Phase = 0.f;
		float FlapRate = 9.f;
		float Size = 0.09f; // as a fraction of the picture's height
		bool bCrossing = false;
		bool bFacingRight = true;
	};

	struct FLiteSteam
	{
		TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
		TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;
		FVector2D Anchor = FVector2D::ZeroVector;
		float Age = 0.f;
		float Life = 3.5f;
		float Size = 0.22f;
		float Drift = 0.f;
	};

	void BuildLite();
	void TickLite(float DeltaTime);

	// The lightweight mode holds about 30 frames a second on weak GPUs by changing the 3D render resolution
	// (the interface stays sharp): it drops fast when frames are slow, creeps back up when there is room, and
	// never goes back above a level that proved too slow.
	void SetupLitePerformance();
	void AdaptResolution(float DeltaTime);
	// Back in High graphics on a PC after Low: puts the screen-wide settings Low changed back as they were.
	void RestoreAfterLite();
	void ApplyLiteResolution();
	float LiteResolution = 0.7f;
	float LiteCeiling = 1.f;
	float LiteLastRaiseFrom = -1.f;
	float LiteWindowTime = 0.f;
	float LiteWindowWorst = 0.f;
	int32 LiteWindowFrames = 0;
	float LiteWarmup = 0.f;
	// The scenery pictures sit behind the board, so their distances grow with the camera's (a wide screen with a big board
	// pulls the camera back past the fixed distances, and the backdrop would cover the board).
	float LiteDepthScale = 1.f;
	float ComputeLiteDepthScale() const;
	bool bAdaptResolution = true;
	void RefreshLiteFrame();
	UStaticMeshComponent* AddLiteQuad(UMaterialInterface* Material);
	void PlaceOnBackdrop(UStaticMeshComponent* Quad, const FVector2D& ImageUV, float Depth, float HeightFraction, float WidthOverHeight, bool bFlipX);

	bool bLite = false;
	bool bCapture = false;
	float LiteAspect = 16.f / 9.f;
	float LiteFOV = 50.f;
	float LiteImageAspect = 16.f / 9.f;
	TArray<FLiteBat> LiteBats;
	TArray<FLiteSteam> LiteSteam;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LiteBackdrop;

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
