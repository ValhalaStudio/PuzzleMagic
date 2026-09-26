#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleTypes.h"
#include "PuzzleTile.generated.h"

class UBoxComponent;
class URealtimeMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Rounded bevelled enamel block with a gold bezel border and a gold symbol inlay
// per colour. The box collision root is what simulates physics when the tile is cleared.
UCLASS()
class PUZZLEGAME5X5_API APuzzleTile : public AActor
{
	GENERATED_BODY()

public:
	APuzzleTile();

	// Distance from the actor origin (box centre) down to the tile's base, at scale 1.
	static constexpr float HalfHeight = 13.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	int32 GridX = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	int32 GridY = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	EPuzzleTileColor TileColor = EPuzzleTileColor::Red;

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SetTileColor(EPuzzleTileColor NewColor);

	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void MoveToPosition(int32 NewGridX, int32 NewGridY);

	// Resting tiles collide (so flying tiles bounce off them); previews don't.
	void SetCollidable(bool bCollidable);

	// Flies from a start location/scale to its current location/scale along an arc, then squash-bounces.
	void PlayArrive(const FVector& FromLocation, float FromScale, float Duration, float ArcHeight, float Delay = 0.f);

	// After Delay: flash, pop, then launch with physics and self-destruct.
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void PlayClearEffectAndDestroy(float Delay = 0.f, float LaunchStrength = 1.f);

	void SetGhost(bool bValid);

	// Gargoyle stone look: 2 = intact, 1 = cracked, 0 = normal tile.
	void SetStone(int32 Level);

	// Golden sheen sweeping over tiles inside the blessed box.
	void SetShimmer(float Amount);

	// A clear hit an intact stone: after Delay it shudders and cracks instead of popping.
	void PlayStoneHit(float Delay);

	// Lightning: after Delay the tile flashes and turns into an intact gargoyle stone.
	void PlayPetrify(float Delay);

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<UBoxComponent> Box;

	// Every tile shares one mesh (MeshBuffers::GetSharedMesh): one set of GPU buffers for the whole board.
	UPROPERTY(VisibleAnywhere, Category = "Puzzle")
	TObjectPtr<URealtimeMeshComponent> TileMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> TileMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BezelMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

private:
	enum class EAnim : uint8 { None, Arriving, Squash, ClearPending, Popping, Flying, StoneHit };

	void ApplyVisualState();
	void Launch();
	void SetGlow(float Glow);

	EAnim Anim = EAnim::None;
	float AnimTime = 0.f;
	float AnimDelay = 0.f;
	float AnimDuration = 0.f;

	FVector ArriveFrom = FVector::ZeroVector;
	FVector ArriveTo = FVector::ZeroVector;
	float ArriveFromScale = 1.f;
	float RestScale = 1.f;
	float ArcHeight = 0.f;

	float LaunchStrength = 1.f;
	float CurrentGlow = 0.f;
	float CurrentStone = 0.f;
	float CurrentShimmer = 0.f;
	// The StoneHit animation is turning this tile to stone rather than cracking it.
	bool bPetrifying = false;
};
