#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GothicEnvironment.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UMaterialInterface;

// Cathedral backdrop around the board: stone wall with stained-glass lancet
// windows (real openings, so moonlight can shaft through volumetric fog),
// columns, and flickering candles. The play area gets its own soft key light.
UCLASS()
class PUZZLEGAME5X5_API AGothicEnvironment : public AActor
{
	GENERATED_BODY()

public:
	AGothicEnvironment();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// An omen's lightning: the windows flash after Delay (the strike's own sound is played by the game mode).
	void TriggerStrike(float Delay);

	// Curses and omens jolt the dread up; it eases back to what the player's luck warrants.
	void AddDread(float Amount);

	// Flips the wall/glass UVs if the plane's UV orientation doesn't match the window layout.
	UPROPERTY(EditAnywhere, Category = "Gothic")
	FVector2D WallUVFlip = FVector2D(0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Gothic")
	float CandleIntensity = 40.f;

	UPROPERTY(EditAnywhere, Category = "Gothic")
	float BoardLightIntensity = 2400.f;

	// Kept low so the windows glow behind the HUD score without drowning it out.
	UPROPERTY(EditAnywhere, Category = "Gothic")
	float StainedGlassIntensity = 2.2f;

private:
	struct FCandle
	{
		TObjectPtr<UPointLightComponent> Light = nullptr;
		TObjectPtr<class UMaterialInstanceDynamic> Flame = nullptr;
		FVector FlameCenter = FVector::ZeroVector;
		float Seed = 0.f;
		float BaseIntensity = 1.f;
	};

	// --- The things in the dark (Lovecraftian layer). Dread (0-1) rises as luck drains and
	// spikes with omens; it drives breathing and stuttering light, tentacles rising from the
	// mist behind the board, eyes opening along the wall, a madness post-process and the abyss drone.
	struct FTentacle
	{
		TObjectPtr<class URealtimeMeshComponent> Mesh = nullptr;
		TObjectPtr<class UMaterialInstanceDynamic> MID = nullptr;
		FVector Base = FVector::ZeroVector;
		float Height = 500.f;
		// Dread at which it starts to rise.
		float Threshold = 0.f;
		float Emerge = 0.f;
	};
	struct FEye
	{
		TObjectPtr<UStaticMeshComponent> Mesh = nullptr;
		TObjectPtr<class UMaterialInstanceDynamic> MID = nullptr;
		float Threshold = 0.f;
		float Open = 0.f;
		float Target = 0.f;
		float NextChange = 0.f;
		float Look = 0.f;
		float LookTarget = 0.f;
		float BlinkUntil = -1.f;
	};

	void BuildEldritch();
	// Returns the light multiplier (breathing x flicker) and how far flames turn eldritch green.
	void TickEldritch(float Time, float DeltaTime, float& OutLightScale, float& OutEldritch);

	TArray<FTentacle> Tentacles;
	TArray<FEye> Eyes;
	float Dread = 0.f;
	float DreadSpike = 0.f;
	float FlickerStart = -100.f;
	float FlickerDuration = 1.f;
	float NextFlickerTime = 25.f;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> MadnessMID;
	bool bMadnessAttached = false;

	// Local fog volumes pooled on the floor; they thicken and take on a green glow with dread.
	UPROPERTY()
	TArray<TObjectPtr<class ALocalFogVolume>> FogVolumes;

	// Instanced motes (one draw call, M_Mote): dust drifting in the candlelight, embers rising from
	// the flames, and green wisps circling the tentacles as dread rises.
	enum class EMote : uint8 { Dust, Ember, Wisp };
	struct FMote
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 1.f;
		float Size = 1.f;
		float Seed = 0.f;
		EMote Kind = EMote::Dust;
	};
	TArray<FMote> Motes;

	UPROPERTY()
	TObjectPtr<class UInstancedStaticMeshComponent> MoteMesh;

	void BuildMotes();
	void RespawnMote(FMote& Mote, bool bScatterAge);
	void TickMotes(float Time, float DeltaTime, float LightScale);

	UPROPERTY()
	TObjectPtr<class UAudioComponent> AbyssAudio;
	bool bAbyssIsMetaSound = false;

	// Convolution reverb on SM_Cathedral (every SFX sends to it) with an impulse response synthesized
	// here at startup, so no impulse file needs importing or shipping.
	void BuildAcoustics();

	UPROPERTY()
	TObjectPtr<class UAudioImpulseResponse> CathedralIR;

	UPROPERTY()
	TObjectPtr<class USubmixEffectConvolutionReverbPreset> CathedralReverb;

	UPROPERTY()
	TObjectPtr<class USoundBase> FlickerSound;

	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bCastShadow);
	void AddCandle(const FVector& Base, float Height, float Radius, bool bCastShadows);

	TArray<FCandle> Candles;

	UPROPERTY(VisibleAnywhere, Category = "Gothic")
	TObjectPtr<USpotLightComponent> BoardLight;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WallMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> GlassMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MarbleMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FlameMaterial;

	// --- Storm: lightning behind the stained glass (its shadows throw the window tracery
	// across the room), a glass flash, and thunder a beat later. ---
	UPROPERTY(EditAnywhere, Category = "Gothic")
	float LightningIntensity = 6000.f;

	UPROPERTY(VisibleAnywhere, Category = "Gothic")
	TObjectPtr<UPointLightComponent> LightningLight;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> GlassMID;

	UPROPERTY()
	TObjectPtr<class USoundBase> ThunderSound;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MistMaterial;

	float NextStrikeTime = 5.f;
	float StrikeTime = -100.f;
	float ThunderTime = -1.f;
	float StrikeSeed = 0.f;
	float PendingStrikeTime = -1.f;

	UPROPERTY(EditAnywhere, Category = "Gothic")
	float ThunderVolume = 0.4f;

	// The player's luck (0-1), eased: candles burn brighter and warmer with it, and the storm
	// outside grows restless as it runs out.
	float Fortune = 0.2f;
};
