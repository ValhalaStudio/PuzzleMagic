#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuUI.generated.h"

class UButton;

UCLASS()
class PUZZLEGAME5X5_API UMainMenuUI : public UUserWidget
{
	GENERATED_BODY()

public:
	UMainMenuUI(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OptionsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExitButton;

	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnOptionsClicked();

	UFUNCTION()
	void OnExitClicked();
};
