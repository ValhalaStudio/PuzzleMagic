#include "PuzzleGameUI.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

UPuzzleGameUI::UPuzzleGameUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPuzzleGameUI::NativeConstruct()
{
	Super::NativeConstruct();
	HideMenu();
}

void UPuzzleGameUI::UpdateHUD(int32 Score, int32 ComboStreak)
{
	if (ScoreText)
	{
		ScoreText->SetText(FText::Format(NSLOCTEXT("Puzzle", "ScoreFmt", "Score: {0}"), FText::AsNumber(Score)));
	}

	if (ComboText)
	{
		ComboText->SetText(ComboStreak > 0
			? FText::Format(NSLOCTEXT("Puzzle", "ComboFmt", "Combo x{0}"), FText::AsNumber(ComboStreak))
			: FText::GetEmpty());
	}
}

void UPuzzleGameUI::ShowMenu()
{
	bMenuVisible = true;
	if (PauseMenuPanel)
	{
		PauseMenuPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UPuzzleGameUI::HideMenu()
{
	bMenuVisible = false;
	if (PauseMenuPanel)
	{
		PauseMenuPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}
