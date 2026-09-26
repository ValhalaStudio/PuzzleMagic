#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PuzzleHUD.generated.h"

class UPuzzleHUDWidget;

// Host for the UMG game UI (UPuzzleHUDWidget), created as soon as the HUD exists.
UCLASS()
class PUZZLEGAME5X5_API APuzzleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UPuzzleHUDWidget* GetWidget() const { return Widget; }

private:
	UPROPERTY()
	TObjectPtr<UPuzzleHUDWidget> Widget;
};
