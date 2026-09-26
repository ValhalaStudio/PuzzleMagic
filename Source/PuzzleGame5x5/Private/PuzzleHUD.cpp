#include "PuzzleHUD.h"
#include "PuzzleHUDWidget.h"
#include "GameFramework/PlayerController.h"

void APuzzleHUD::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		Widget = CreateWidget<UPuzzleHUDWidget>(PC, UPuzzleHUDWidget::StaticClass());
		if (Widget)
		{
			Widget->AddToViewport();
		}
	}
}
