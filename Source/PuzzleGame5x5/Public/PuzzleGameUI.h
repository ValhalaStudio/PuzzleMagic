#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PuzzleGameUI.generated.h"

class UTextBlock;

UCLASS()
class PUZZLEGAME5X5_API UPuzzleGameUI : public UUserWidget
{
	GENERATED_BODY()

public:
	UPuzzleGameUI(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Puzzle UI")
	void UpdateHUD(int32 Score, int32 ComboStreak);

	UFUNCTION(BlueprintCallable, Category = "Puzzle UI")
	void ShowMenu();

	UFUNCTION(BlueprintCallable, Category = "Puzzle UI")
	void HideMenu();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ComboText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> PauseMenuPanel;

	bool bMenuVisible = false;
};
