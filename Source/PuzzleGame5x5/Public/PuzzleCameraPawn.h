#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PuzzleCameraPawn.generated.h"

class UCameraComponent;

// Fixed camera that frames the board + tray for whatever screen shape it runs on
// (the game is landscape only), with an optional screen shake. It positions itself
// because the GameMode only copies a PlayerStart's yaw, never its pitch.
UCLASS()
class PUZZLEGAME5X5_API APuzzleCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	APuzzleCameraPawn();

	// Area on the ground plane (world XY) that must stay in view.
	void SetFramingBounds(const FBox2D& Bounds, float GroundZ);

	void AddShake(float Strength);

	// Shallow enough that the cathedral wall and its windows show above the board.
	UPROPERTY(EditAnywhere, Category = "Puzzle|Camera")
	float ViewPitch = -56.f;

	// Looking toward -Y makes world +X screen-right, matching the board's cell layout.
	UPROPERTY(EditAnywhere, Category = "Puzzle|Camera")
	float ViewYaw = -90.f;

	UPROPERTY(EditAnywhere, Category = "Puzzle|Camera")
	float LandscapeFOV = 50.f;

	UPROPERTY(EditAnywhere, Category = "Puzzle|Camera")
	float FramingMargin = 1.08f;

	// Fixed (manual) exposure, so the look doesn't pump as the board fills.
	UPROPERTY(EditAnywhere, Category = "Puzzle|Camera")
	float ExposureCompensation = 0.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<class UMaterialInterface> InkOutlineMaterial;

private:
	void UpdateFraming();

	FBox2D FramingBounds = FBox2D(FVector2D(-460.f, -460.f), FVector2D(460.f, 760.f));
	float FramingGroundZ = 30.f;
	FVector2D LastViewportSize = FVector2D::ZeroVector;
	FVector RestLocation = FVector::ZeroVector;
	float ShakeAmplitude = 0.f;
};
