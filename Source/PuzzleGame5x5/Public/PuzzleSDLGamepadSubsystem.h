#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "PuzzleTypes.h"
#include "PuzzleSDLGamepadSubsystem.generated.h"

struct SDL_Gamepad;

// Wraps SDL3's gamepad API to give the puzzle game broader controller support
// than Unreal's native input mapping (SDL3 maintains a large community
// controller database and normalizes button layouts across vendors).
UCLASS()
class PUZZLEGAME5X5_API UPuzzleSDLGamepadSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPuzzleSDLGamepadSubsystem, STATGROUP_Tickables); }

	UFUNCTION(BlueprintCallable, Category = "Puzzle|Gamepad")
	bool IsGamepadConnected() const;

	UFUNCTION(BlueprintCallable, Category = "Puzzle|Gamepad")
	FVector2D GetLeftStick() const;

	UFUNCTION(BlueprintCallable, Category = "Puzzle|Gamepad")
	bool WasButtonJustPressed(EPuzzleGamepadButton Button) const;

private:
	void OpenFirstAvailableGamepad();
	void CloseActiveGamepad();
	void PollEvents();
	void RefreshButtonStates();

	SDL_Gamepad* ActiveGamepad = nullptr;

	TMap<EPuzzleGamepadButton, bool> CurrentButtonState;
	TMap<EPuzzleGamepadButton, bool> PreviousButtonState;
};
