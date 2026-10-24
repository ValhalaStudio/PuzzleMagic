#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeskEnvironment.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UDirectionalLightComponent;
class USpotLightComponent;

// The Desk theme's scenery: a quiet wooden desk under soft daylight, with a few plain objects (mug, pencil, notebook) around the
// play area. Real geometry from the engine's basic shapes and two small materials (M_DeskWood, M_DeskProp), so it is cheap enough
// for every device and needs no pre-rendered picture in the lightweight mode.
UCLASS()
class PUZZLEGAME5X5_API ADeskEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ADeskEnvironment();

	virtual void BeginPlay() override;

	// The key light over the board, in the same place and strength as the cathedral's so the tiles read the same way.
	UPROPERTY(EditAnywhere, Category = "Desk")
	float BoardLightIntensity = 2400.f;

	// Daylight from the front left, in lux.
	UPROPERTY(EditAnywhere, Category = "Desk")
	float DaylightLux = 6.f;

private:
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bCastShadow);
	UMaterialInstanceDynamic* PropMaterial(const FLinearColor& Color, float Roughness, float Metal = 0.f);
	void BuildProps();

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> Daylight;

	UPROPERTY()
	TObjectPtr<USpotLightComponent> BoardLight;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PropBase;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Owned;
};
