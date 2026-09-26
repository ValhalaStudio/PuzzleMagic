#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "PuzzleTypes.h"
#include "PuzzleInputHandler.generated.h"

class AGridManager;
class UPuzzleManager;
class UPuzzleSDLGamepadSubsystem;

UCLASS()
class PUZZLEGAME5X5_API APuzzleInputHandler : public AActor
{
	GENERATED_BODY()

public:
	APuzzleInputHandler();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	TObjectPtr<AGridManager> GridManager;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	TObjectPtr<UPuzzleManager> PuzzleManager;

	// Drops any drag, targeting and ghost preview (menus opening, new round, bot taking over).
	void CancelInteraction();

	// Holy Light relic: the next tap on the board picks the 3x3 area to clear.
	void ToggleHolyTargeting();
	bool IsHolyTargeting() const { return bHolyTargeting; }

	// True while a dragged piece hovers over the hold slot.
	bool IsHoveringReserve() const { return bOverReserve; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// --- Drag-to-place (mouse and touch) ---
	void HandlePressed(FVector2D ScreenPosition);
	void HandleMoved(FVector2D ScreenPosition);
	void HandleReleased(FVector2D ScreenPosition);

	bool DeprojectToPlane(FVector2D ScreenPosition, float PlaneZ, FVector& OutPoint) const;
	bool ScreenPositionToBoardOrigin(FVector2D ScreenPosition, const FPuzzlePieceShape& Shape, int32& OutOriginX, int32& OutOriginY) const;
	int32 FindTraySlotUnderScreenPosition(FVector2D ScreenPosition) const;

	// How far (in cells) a touch-dragged piece is lifted above the fingertip.
	UPROPERTY(EditAnywhere, Category = "Puzzle")
	float TouchLiftCells = 1.8f;

	bool bTouchDrag = false;

	void OnMousePressed();
	void OnMouseReleased();
	void OnTouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
	void OnTouchRepeat(ETouchIndex::Type FingerIndex, FVector Location);
	void OnTouchReleased(ETouchIndex::Type FingerIndex, FVector Location);

	bool bDragging = false;
	int32 DraggedSlot = -1;
	bool bOverReserve = false;

	bool bHolyTargeting = false;
	bool ScreenPositionToCell(FVector2D ScreenPosition, int32& OutX, int32& OutY) const;
	void ShowHolyGhost(FVector2D ScreenPosition);
	bool IsInputAllowed() const;
	void OnCancelPressed();

	// --- Gamepad (SDL3) alternative control scheme ---
	void TickGamepadNavigation();

	UPROPERTY()
	TObjectPtr<UPuzzleSDLGamepadSubsystem> GamepadSubsystem;

	int32 CursorX = 0;
	int32 CursorY = 0;
	int32 ActiveTraySlot = 0;

	void RefreshGamepadGhost();

	// --- Demo / auto-play toggle and restart ---
	void OnToggleAutoPlayPressed();
	void OnRestartPressed();
};
