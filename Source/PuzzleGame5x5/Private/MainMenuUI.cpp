#include "MainMenuUI.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/UserWidget.h"

UMainMenuUI::UMainMenuUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMainMenuUI::NativeConstruct()
{
	Super::NativeConstruct();

	if (StartGameButton)
	{
		StartGameButton->OnClicked.AddDynamic(this, &UMainMenuUI::OnStartGameClicked);
	}
	if (OptionsButton)
	{
		OptionsButton->OnClicked.AddDynamic(this, &UMainMenuUI::OnOptionsClicked);
	}
	if (ExitButton)
	{
		ExitButton->OnClicked.AddDynamic(this, &UMainMenuUI::OnExitClicked);
	}
}

void UMainMenuUI::OnStartGameClicked()
{
	// The board is already live behind the menu (GameMode builds it at StartPlay);
	// starting just dismisses the overlay.
	RemoveFromParent();
}

void UMainMenuUI::OnOptionsClicked()
{
	// Options menu is not implemented yet; placeholder for future settings UI.
}

void UMainMenuUI::OnExitClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
