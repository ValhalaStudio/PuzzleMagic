#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PuzzleTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "Components/Button.h"
#include "PuzzleHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UButton;
class UWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class APuzzleGameMode;
class UPuzzleHUDWidget;
struct FPuzzleClearEvent;

UENUM()
enum class EPuzzleCard : uint8
{
	None,
	Menu,
	GameOver,
	Tutorial,
	Pause,
	Courses,
	Options,
	Packet,   // Trick or treat?
	Treat,    // the treat shop
	Trick     // the wheel
};

// Buttons never take keyboard focus, so game keys keep reaching the player controller after a click.
UCLASS()
class UPuzzleUIButton : public UButton
{
	GENERATED_BODY()

public:
	UPuzzleUIButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
	{
		InitIsFocusable(false);
	}
};

// UButton's OnClicked carries no payload; one proxy per button remembers which action it is.
UCLASS()
class UPuzzleButtonProxy : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UPuzzleHUDWidget> Owner;
	int32 Action = 0;
	int32 Param = 0;

	UFUNCTION()
	void HandleClicked();
};

// The whole game UI, built in C++ (no Widget Blueprint): enamel-and-gold panels drawn
// by the M_UIPanel shader, embossed M_UIIcon coins, Lilita One / Cinzel Decorative type.
// It polls the rules for numbers and receives one-shot events for popups and cards.
UCLASS()
class PUZZLEGAME5X5_API UPuzzleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowCard(EPuzzleCard Card);
	void ShowClear(const FPuzzleClearEvent& Event);
	void ShowComboBroken(int32 LostCombo);
	void ShowRelicGained(ERelic Relic);
	void ShowBonusSpawned(const FVector& WorldLocation);
	// Trick-or-Treat packet (UTrickOrTreat).
	void ShowPacketArrived();
	void ShowTreatBought(int32 Offer);
	// Opens the wheel card and spins it to Segment; it calls APuzzleGameMode::FinishTrick when it stops.
	void ShowWheel(int32 Segment);
	void ShowTrickResult(int32 Segment, const FString& Line);
	void ResetRound();

	// The "How to play" pages, from Page (0-based).
	void ShowTutorial(int32 Page = 0);

	void HandleAction(int32 Action, int32 Param);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// ActTutorial's Param is the page to show; -1 closes the tutorial.
	enum EAction : int32
	{
		ActPlay, ActRetry, ActMenu, ActHoly, ActReroll, ActHowTo, ActTutorial,
		ActPause, ActResume, ActTakeOver, ActStartDemo, ActCourses, ActOptions, ActToggleRelics, ActToggleBonus,
		ActCourse,  // Param = width * 10 + height
		ActPacket, ActTreat, ActTrick, ActPacketLater,
		ActBuyTreat // Param = offer index
	};

	void BuildTutorialPage();
	int32 TutorialPage = 0;

	// --- Builders ---
	FSlateFontInfo Font(bool bTitle, float Size, float Outline = 4.f) const;
	UTextBlock* MakeText(const FString& Text, bool bTitle, float Size, const FLinearColor& Color, float Outline = 4.f);
	UMaterialInstanceDynamic* MakePanelMID(const FLinearColor& Fill, const FLinearColor& Fill2, const FLinearColor& Rim, float Aspect, float Radius = 0.3f, float Glow = 0.f, float Pressed = 0.f);
	UImage* MakePanel(const FLinearColor& Fill, const FLinearColor& Fill2, const FLinearColor& Rim, float Aspect, float Radius = 0.3f, float Glow = 0.f);
	UImage* MakeIcon(float Shape, const FLinearColor& Color, float Size, float Fill = 1.f, float Glow = 0.f);
	UButton* MakeButton(UWidget* Content, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Fill2, int32 Action, int32 Param = 0);
	UButton* MakeTextButton(const FString& Label, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Fill2, int32 Action, int32 Param = 0);
	UWidget* Sized(UWidget* Content, const FVector2D& Size);

	void BuildHUD();
	void BuildCardContent(EPuzzleCard Card);
	// Rebuilds the open card in place (no entrance animation), for buttons that change their own label.
	void RefreshCard();

	// --- Popups ---
	struct FPopup
	{
		TWeakObjectPtr<UWidget> Widget;
		FVector World = FVector::ZeroVector;
		FVector2D Screen = FVector2D::ZeroVector; // fraction of the canvas when not world-anchored
		bool bWorld = false;
		float Age = 0.f;
		float Life = 1.2f;
		float Rise = 60.f;
		float Scale = 1.f;
	};
	void AddPopup(UWidget* Content, const FVector2D& ScreenFraction, float Life, float Scale = 1.f, float Delay = 0.f, float Rise = 60.f);
	void AddWorldPopup(UWidget* Content, const FVector& World, float Life, float Scale = 1.f, float Delay = 0.f);
	void TickPopups(float DeltaTime);

	APuzzleGameMode* GetGameMode() const;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PanelMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> IconMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UPuzzleButtonProxy>> Proxies;

	TSharedPtr<const struct FCompositeFont> BodyFont;
	TSharedPtr<const struct FCompositeFont> TitleFont;

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY() TObjectPtr<UCanvasPanel> PopupLayer;
	UPROPERTY() TObjectPtr<UImage> TopBarBg;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> TopBarSlot;
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> HintSlot;
	// Where the combo badge hangs (between the candles), as a fraction of the canvas; combo popups use it too.
	FVector2D ComboAnchorFraction = FVector2D(0.5f, 0.14f);
	UPROPERTY() TObjectPtr<UTextBlock> MovesText;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreText;
	UPROPERTY() TObjectPtr<UTextBlock> BestText;
	UPROPERTY() TObjectPtr<UWidget> ComboBadge;
	UPROPERTY() TObjectPtr<UImage> ComboBg;
	UPROPERTY() TObjectPtr<UTextBlock> ComboText;
	UPROPERTY() TArray<TObjectPtr<UImage>> ComboPips;
	UPROPERTY() TArray<TObjectPtr<UButton>> RelicButtons;
	UPROPERTY() TArray<TObjectPtr<UImage>> RelicIcons;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> RelicCounts;
	UPROPERTY() TArray<TObjectPtr<UWidget>> RelicBadges;
	UPROPERTY() TObjectPtr<UWidget> LuckBadge;
	UPROPERTY() TObjectPtr<UImage> LuckBg;
	UPROPERTY() TObjectPtr<UImage> LuckIcon;
	UPROPERTY() TObjectPtr<UTextBlock> LuckText;
	UPROPERTY() TObjectPtr<class USizeBox> LuckFillBox;
	UPROPERTY() TObjectPtr<UImage> LuckFill;
	UPROPERTY() TObjectPtr<UTextBlock> HintText;
	UPROPERTY() TObjectPtr<UTextBlock> HoldLabel;
	UPROPERTY() TObjectPtr<UWidget> MenuButton;
	UPROPERTY() TObjectPtr<UWidget> CardLayer;
	UPROPERTY() TObjectPtr<UImage> CardDim;
	UPROPERTY() TObjectPtr<UImage> CardBg;
	UPROPERTY() TObjectPtr<UWidget> CardPanel;
	UPROPERTY() TObjectPtr<UVerticalBox> CardContent;
	UPROPERTY() TArray<TObjectPtr<UWidget>> CardStars;
	UPROPERTY() TObjectPtr<UWidget> PacketButton;
	UPROPERTY() TObjectPtr<UImage> PacketIcon;
	UPROPERTY() TObjectPtr<UWidget> WheelSpinner;
	UPROPERTY() TObjectPtr<UTextBlock> WheelResult;
	float PacketPop = 0.f;
	// The wheel: where it will stop (-1 = not spinning), how long it has turned, whether it has reported.
	int32 WheelSegment = -1;
	float WheelAge = 0.f;
	bool bWheelDone = false;
	static constexpr float WheelSpinSeconds = 3.4f;

	TArray<FPopup> Popups;
	EPuzzleCard CurrentCard = EPuzzleCard::None;
	float CardAge = 0.f;
	float DisplayedScore = 0.f;
	int32 LastCombo = 0;
	int32 LastMoves = 0;
	float ComboPop = 0.f;
	float MovesPop = 0.f;
	float RelicPop[2] = { 0.f, 0.f };
	int32 LastLuck = -1;
	float LuckPop = 0.f;
	float DisplayedLuck = 0.f;
	// The score and best texts shrink as their numbers get longer, so they stay inside the panel.
	float ScoreFontSize = 0.f;
	float BestFontSize = 0.f;
	FVector2D CanvasSize = FVector2D(1920.f, 1080.f);
	float Time = 0.f;
};
