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

// Rounded bevelled enamel block with a gold bezel border and a gold triangle inlay that
// shows the direction a route flows through it. The box collision root is what simulates physics when the tile is cleared.
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

	// The way this tile's triangle points (the way a route flows through it).
	void SetDirection(EPuzzleDir NewDir);

	// Turns this tile into a bonus tile: its own colour and emblem (none, a diamond, or a fisheye), no arrow,
	// with the golden sheen sweeping over it.
	void SetBonus(EPuzzleBonus NewBonus);

	// A bonus tile's own clear: after Delay a potion drains (outgoing) or fills (incoming), or the pumpkin swells and
	// shakes, then it pops like any tile (the pumpkin with a much bigger burst).
	void PlayBonusClear(float Delay, float InLaunchStrength);

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
	enum class EAnim : uint8 { None, Arriving, Squash, BonusPre, ClearPending, Popping, Flying };

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
	EPuzzleDir TileDirection = EPuzzleDir::Up;
	bool bHasDirection = false;
	EPuzzleBonus Bonus = EPuzzleBonus::None;
	// Liquid level of a potion bottle emblem (1 full, 0 empty).
	float BonusFill = 0.f;
	FVector BonusRestLocation = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BonusMaterial;
};
