#include "PuzzleHUDWidget.h"
#include "PuzzleGameMode.h"
#include "PuzzleManager.h"
#include "PuzzleInputHandler.h"
#include "PuzzleSaveGame.h"
#include "GridManager.h"
#include "TrickOrTreat.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/CompositeFont.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"

namespace UIStyle
{
	// Linear colours (Slate converts to sRGB on output).
	const FLinearColor Ink(0.02f, 0.008f, 0.04f);
	const FLinearColor Gold(1.f, 0.55f, 0.08f);
	const FLinearColor PaleGold(1.f, 0.8f, 0.4f);
	const FLinearColor Lavender(0.6f, 0.5f, 1.f);
	const FLinearColor Cyan(0.3f, 0.85f, 1.f);
	const FLinearColor Danger(1.f, 0.12f, 0.1f);
	const FLinearColor Rim(1.f, 0.62f, 0.2f);

	const FLinearColor PanelFill(0.05f, 0.02f, 0.1f);
	const FLinearColor PanelFill2(0.012f, 0.005f, 0.03f);
	const FLinearColor Emerald(0.06f, 0.42f, 0.16f);
	const FLinearColor Emerald2(0.015f, 0.14f, 0.05f);
	const FLinearColor Ruby(0.5f, 0.05f, 0.12f);
	const FLinearColor Ruby2(0.16f, 0.01f, 0.04f);
	const FLinearColor Amethyst(0.22f, 0.08f, 0.45f);
	const FLinearColor Amethyst2(0.06f, 0.02f, 0.14f);
	const FLinearColor Locked(0.03f, 0.03f, 0.04f);
	const FLinearColor Locked2(0.01f, 0.01f, 0.012f);

	const FLinearColor LuckGreen(0.45f, 1.f, 0.55f);
	const FLinearColor Pumpkin(1.f, 0.38f, 0.04f);
	const FLinearColor Pumpkin2(0.3f, 0.08f, 0.005f);

	// Icon shapes in M_UIIcon.
	constexpr float IconMoon = 2.f;
	constexpr float IconSun = 6.f;
	constexpr float IconReroll = 7.f;
	constexpr float IconLine = 9.f;
	constexpr float IconStar = 10.f;

	float BackOut(float T)
	{
		const float C1 = 1.9f;
		const float C3 = C1 + 1.f;
		const float U = FMath::Clamp(T, 0.f, 1.f) - 1.f;
		return 1.f + C3 * U * U * U + C1 * U * U;
	}
}

void UPuzzleButtonProxy::HandleClicked()
{
	if (UPuzzleHUDWidget* Widget = Owner.Get())
	{
		Widget->HandleAction(Action, Param);
	}
}

APuzzleGameMode* UPuzzleHUDWidget::GetGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<APuzzleGameMode>() : nullptr;
}

// --- Builders ------------------------------------------------------------------

FSlateFontInfo UPuzzleHUDWidget::Font(bool bTitle, float Size, float Outline) const
{
	FFontOutlineSettings OutlineSettings;
	OutlineSettings.OutlineSize = FMath::RoundToInt(Outline);
	OutlineSettings.OutlineColor = UIStyle::Ink;
	OutlineSettings.bApplyOutlineToDropShadows = true;
	return FSlateFontInfo(bTitle ? TitleFont : BodyFont, Size, NAME_None, OutlineSettings);
}

UTextBlock* UPuzzleHUDWidget::MakeText(const FString& Text, bool bTitle, float Size, const FLinearColor& Color, float Outline)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(FText::FromString(Text));
	Block->SetFont(Font(bTitle, Size, Outline));
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(FVector2D(0.f, Size * 0.08f));
	Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Block->SetJustification(ETextJustify::Center);
	return Block;
}

UMaterialInstanceDynamic* UPuzzleHUDWidget::MakePanelMID(const FLinearColor& Fill, const FLinearColor& Fill2, const FLinearColor& Rim, float Aspect, float Radius, float Glow, float Pressed)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PanelMaterial, this);
	MID->SetVectorParameterValue(TEXT("Fill"), Fill);
	MID->SetVectorParameterValue(TEXT("Fill2"), Fill2);
	MID->SetVectorParameterValue(TEXT("Rim"), Rim);
	MID->SetScalarParameterValue(TEXT("Aspect"), Aspect);
	MID->SetScalarParameterValue(TEXT("Radius"), Radius);
	MID->SetScalarParameterValue(TEXT("Glow"), Glow);
	MID->SetScalarParameterValue(TEXT("Pressed"), Pressed);
	MID->SetScalarParameterValue(TEXT("RimWidth"), 0.07f);
	MID->SetScalarParameterValue(TEXT("Opacity"), 0.94f);
	return MID;
}

UImage* UPuzzleHUDWidget::MakePanel(const FLinearColor& Fill, const FLinearColor& Fill2, const FLinearColor& Rim, float Aspect, float Radius, float Glow)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	Image->SetBrushFromMaterial(MakePanelMID(Fill, Fill2, Rim, Aspect, Radius, Glow));
	return Image;
}

UImage* UPuzzleHUDWidget::MakeIcon(float Shape, const FLinearColor& Color, float Size, float Fill, float Glow)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(IconMaterial, this);
	MID->SetScalarParameterValue(TEXT("Shape"), Shape);
	MID->SetScalarParameterValue(TEXT("Fill"), Fill);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Glow"), Glow);
	MID->SetVectorParameterValue(TEXT("GlowColor"), Color * 1.5f);

	// The brush's own ImageSize is what sizes the icon (SetBrushFromMaterial leaves it at 32x32).
	FSlateBrush Brush;
	Brush.SetResourceObject(MID);
	Brush.ImageSize = FVector2D(Size, Size);
	Brush.DrawAs = ESlateBrushDrawType::Image;

	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	Image->SetBrush(Brush);
	return Image;
}

UWidget* UPuzzleHUDWidget::Sized(UWidget* Content, const FVector2D& Size)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	if (Size.X > 0.f) { Box->SetWidthOverride(Size.X); }
	if (Size.Y > 0.f) { Box->SetHeightOverride(Size.Y); }
	Box->AddChild(Content);
	return Box;
}

UButton* UPuzzleHUDWidget::MakeButton(UWidget* Content, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Fill2, int32 Action, int32 Param)
{
	const float Aspect = Size.X / Size.Y;
	const float Radius = 0.42f;
	auto Brush = [&](UMaterialInstanceDynamic* MID)
	{
		FSlateBrush SlateBrush;
		SlateBrush.SetResourceObject(MID);
		SlateBrush.ImageSize = Size;
		SlateBrush.DrawAs = ESlateBrushDrawType::Image;
		return SlateBrush;
	};

	FButtonStyle Style;
	Style.SetNormal(Brush(MakePanelMID(Fill, Fill2, UIStyle::Rim, Aspect, Radius, 0.f)));
	Style.SetHovered(Brush(MakePanelMID(Fill * 1.35f, Fill2 * 1.3f, UIStyle::PaleGold, Aspect, Radius, 0.9f)));
	Style.SetPressed(Brush(MakePanelMID(Fill, Fill2, UIStyle::PaleGold, Aspect, Radius, 0.5f, 1.f)));
	Style.SetDisabled(Brush(MakePanelMID(UIStyle::Locked, UIStyle::Locked2, FLinearColor(0.25f, 0.22f, 0.3f), Aspect, Radius, 0.f)));
	Style.SetNormalPadding(FMargin(0.f));
	Style.SetPressedPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	UButton* Button = WidgetTree->ConstructWidget<UPuzzleUIButton>();
	Button->SetStyle(Style);
	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(Content)))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
		ButtonSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Size.Y * 0.04f));
	}

	UPuzzleButtonProxy* Proxy = NewObject<UPuzzleButtonProxy>(this);
	Proxy->Owner = this;
	Proxy->Action = Action;
	Proxy->Param = Param;
	Proxies.Add(Proxy);
	Button->OnClicked.AddDynamic(Proxy, &UPuzzleButtonProxy::HandleClicked);
	return Button;
}

UButton* UPuzzleHUDWidget::MakeTextButton(const FString& Label, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Fill2, int32 Action, int32 Param)
{
	return MakeButton(MakeText(Label, false, Size.Y * 0.36f, FLinearColor::White, 4.f), Size, Fill, Fill2, Action, Param);
}

// --- Construction ------------------------------------------------------------------

void UPuzzleHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	PanelMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UIPanel.M_UIPanel"));
	IconMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_UIIcon.M_UIIcon"));

	// Fonts ship as raw .ttf files (UI/Fonts is staged as UFS) and load lazily at runtime.
	const FString FontDir = FPaths::ProjectContentDir() / TEXT("UI/Fonts");
	BodyFont = MakeShared<FStandaloneCompositeFont>(TEXT("LilitaOne"), FontDir / TEXT("LilitaOne-Regular.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	TitleFont = MakeShared<FStandaloneCompositeFont>(TEXT("CinzelDecorative"), FontDir / TEXT("CinzelDecorative-Black.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildHUD();
}

void UPuzzleHUDWidget::BuildHUD()
{
	using namespace UIStyle;

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	// ---- Top bar: MOVES | SCORE | BEST ----
	UOverlay* TopBar = WidgetTree->ConstructWidget<UOverlay>();
	TopBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	TopBarBg = MakePanel(PanelFill, PanelFill2, Rim, 5.f, 0.22f);
	if (UOverlaySlot* BgSlot = TopBar->AddChildToOverlay(TopBarBg))
	{
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	auto AddColumn = [&](UWidget* Label, UWidget* Value, float Fill)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Column->AddChildToVerticalBox(Label)->SetHorizontalAlignment(HAlign_Center);
		Column->AddChildToVerticalBox(Value)->SetHorizontalAlignment(HAlign_Center);
		UHorizontalBoxSlot* ColumnSlot = Columns->AddChildToHorizontalBox(Column);
		FSlateChildSize ColumnSize(ESlateSizeRule::Fill);
		ColumnSize.Value = Fill;
		ColumnSlot->SetSize(ColumnSize);
		ColumnSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnSlot->SetVerticalAlignment(VAlign_Center);
	};

	MovesText = MakeText(TEXT("30"), false, 64.f, Cyan, 5.f);
	if (UPuzzleManager::bMoveBudgetEnabled)
	{
		AddColumn(MakeText(TEXT("MOVES"), true, 22.f, Lavender, 3.f), MovesText, 1.f);
	}

	ScoreText = MakeText(TEXT("0"), false, 74.f, Gold, 5.f);
	AddColumn(MakeText(TEXT("SCORE"), true, 24.f, PaleGold, 3.f), ScoreText, 1.35f);

	BestText = MakeText(TEXT("0"), false, 52.f, FLinearColor::White, 5.f);
	AddColumn(MakeText(TEXT("BEST"), true, 22.f, Lavender, 3.f), BestText, 1.f);

	if (UOverlaySlot* ColumnsSlot = TopBar->AddChildToOverlay(Columns))
	{
		ColumnsSlot->SetHorizontalAlignment(HAlign_Fill);
		ColumnsSlot->SetVerticalAlignment(VAlign_Center);
		ColumnsSlot->SetPadding(FMargin(30.f, 0.f, 30.f, 8.f));
	}

	TopBarSlot = RootCanvas->AddChildToCanvas(TopBar);
	// Landscape: a compact score panel in the top-right corner.
	TopBarSlot->SetAnchors(FAnchors(1.f, 0.f));
	TopBarSlot->SetAlignment(FVector2D(1.f, 0.f));
	TopBarSlot->SetPosition(FVector2D(-22.f, 36.f));
	TopBarSlot->SetSize(FVector2D(560.f, 174.f));

	// ---- Combo meter: "COMBO x5!" and three pips = placements left to keep it alive ----
	UOverlay* Combo = WidgetTree->ConstructWidget<UOverlay>();
	Combo->SetVisibility(ESlateVisibility::HitTestInvisible);
	ComboBg = MakePanel(FLinearColor(0.4f, 0.03f, 0.3f), FLinearColor(0.1f, 0.005f, 0.09f), Rim, 330.f / 92.f, 0.45f, 0.3f);
	if (UOverlaySlot* BgSlot = Combo->AddChildToOverlay(ComboBg))
	{
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UHorizontalBox* ComboRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	ComboText = MakeText(TEXT("COMBO x1!"), false, 34.f, FLinearColor(1.f, 0.75f, 0.95f), 3.f);
	ComboRow->AddChildToHorizontalBox(ComboText)->SetVerticalAlignment(VAlign_Center);
	for (int32 Pip = 0; Pip < UPuzzleManager::ComboWindowMoves; ++Pip)
	{
		UImage* PipImage = MakeIcon(IconStar, PaleGold, 30.f, 1.f, 0.4f);
		UHorizontalBoxSlot* PipSlot = ComboRow->AddChildToHorizontalBox(PipImage);
		PipSlot->SetVerticalAlignment(VAlign_Center);
		PipSlot->SetPadding(FMargin(Pip == 0 ? 10.f : 2.f, 0.f, 0.f, 0.f));
		ComboPips.Add(PipImage);
	}
	if (UOverlaySlot* RowSlot = Combo->AddChildToOverlay(ComboRow))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetVerticalAlignment(VAlign_Center);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}
	ComboBadge = Combo;
	Combo->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* ComboSlot = RootCanvas->AddChildToCanvas(Combo);
	// Hangs at the top of the scene between the two candle clusters, clear of the board (placed every frame).
	ComboSlot->SetAnchors(FAnchors(0.f, 0.f));
	ComboSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ComboSlot->SetPosition(FVector2D(960.f, 150.f));
	ComboSlot->SetSize(FVector2D(330.f, 92.f));

	// ---- Menu pill (top-left) ----
	UButton* Menu = MakeTextButton(TEXT("MENU"), FVector2D(170.f, 84.f), Amethyst, Amethyst2, ActPause);
	MenuButton = Menu;
	UCanvasPanelSlot* MenuSlot = RootCanvas->AddChildToCanvas(Menu);
	MenuSlot->SetAnchors(FAnchors(0.f, 0.f));
	MenuSlot->SetPosition(FVector2D(34.f, 36.f));
	MenuSlot->SetSize(FVector2D(170.f, 84.f));

	// ---- Luck (top-right, under the bar): the moon's favour. Combos raise it, relics spend it,
	// and it lights the candles. ----
	UOverlay* Luck = WidgetTree->ConstructWidget<UOverlay>();
	Luck->SetVisibility(ESlateVisibility::HitTestInvisible);
	LuckBg = MakePanel(Emerald, Emerald2, Rim, 250.f / 96.f, 0.45f, 0.2f);
	if (UOverlaySlot* BgSlot = Luck->AddChildToOverlay(LuckBg))
	{
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UHorizontalBox* LuckRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	LuckIcon = MakeIcon(IconMoon, LuckGreen, 60.f, 1.f, 0.4f);
	if (UHorizontalBoxSlot* IconSlot = LuckRow->AddChildToHorizontalBox(LuckIcon))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	UVerticalBox* LuckColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	LuckText = MakeText(TEXT("LUCK 20"), false, 32.f, FLinearColor(0.85f, 1.f, 0.85f), 4.f);
	LuckColumn->AddChildToVerticalBox(LuckText)->SetHorizontalAlignment(HAlign_Center);
	UOverlay* LuckBar = WidgetTree->ConstructWidget<UOverlay>();
	UImage* LuckTrack = MakePanel(Locked, Locked2, FLinearColor(0.25f, 0.22f, 0.3f), 140.f / 16.f, 0.5f);
	if (UOverlaySlot* TrackSlot = LuckBar->AddChildToOverlay(LuckTrack))
	{
		TrackSlot->SetHorizontalAlignment(HAlign_Fill);
		TrackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	LuckFill = MakePanel(LuckGreen, Emerald2, PaleGold, 1.f, 0.5f, 0.5f);
	LuckFillBox = WidgetTree->ConstructWidget<USizeBox>();
	LuckFillBox->SetHeightOverride(16.f);
	LuckFillBox->SetWidthOverride(28.f);
	LuckFillBox->AddChild(LuckFill);
	if (UOverlaySlot* FillSlot = LuckBar->AddChildToOverlay(LuckFillBox))
	{
		FillSlot->SetHorizontalAlignment(HAlign_Left);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UVerticalBoxSlot* BarSlot = LuckColumn->AddChildToVerticalBox(Sized(LuckBar, FVector2D(140.f, 16.f))))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Center);
		BarSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}
	LuckRow->AddChildToHorizontalBox(LuckColumn)->SetVerticalAlignment(VAlign_Center);
	if (UOverlaySlot* RowSlot = Luck->AddChildToOverlay(LuckRow))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetVerticalAlignment(VAlign_Center);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}
	LuckBadge = Luck;
	Luck->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* LuckSlot = RootCanvas->AddChildToCanvas(Luck);
	LuckSlot->SetAnchors(FAnchors(1.f, 0.f));
	LuckSlot->SetAlignment(FVector2D(1.f, 0.f));
	LuckSlot->SetPosition(FVector2D(-34.f, 250.f));
	LuckSlot->SetSize(FVector2D(250.f, 96.f));

	// ---- Relic buttons (bottom corners) ----
	for (int32 RelicIndex = 0; RelicIndex < 2; ++RelicIndex)
	{
		const bool bHoly = RelicIndex == static_cast<int32>(ERelic::HolyLight);
		UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>();
		UImage* Icon = MakeIcon(bHoly ? IconSun : IconReroll, bHoly ? Gold : Cyan, 118.f, 1.f, 0.5f);
		if (UOverlaySlot* IconSlot = Face->AddChildToOverlay(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		UButton* Button = MakeButton(Face, FVector2D(190.f, 190.f), bHoly ? FLinearColor(0.3f, 0.16f, 0.02f) : FLinearColor(0.02f, 0.14f, 0.3f),
			bHoly ? FLinearColor(0.08f, 0.03f, 0.005f) : FLinearColor(0.005f, 0.03f, 0.09f), bHoly ? ActHoly : ActReroll);

		// Count badge sits on the button's top corner.
		UOverlay* Holder = WidgetTree->ConstructWidget<UOverlay>();
		Holder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Holder->AddChildToOverlay(Sized(Button, FVector2D(190.f, 190.f)));
		UOverlay* Badge = WidgetTree->ConstructWidget<UOverlay>();
		Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
		UImage* BadgeBg = MakePanel(Ruby, Ruby2, PaleGold, 1.f, 0.5f);
		if (UOverlaySlot* BadgeBgSlot = Badge->AddChildToOverlay(BadgeBg))
		{
			BadgeBgSlot->SetHorizontalAlignment(HAlign_Fill);
			BadgeBgSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UTextBlock* Count = MakeText(TEXT("1"), false, 34.f, FLinearColor::White, 3.f);
		if (UOverlaySlot* CountSlot = Badge->AddChildToOverlay(Count))
		{
			CountSlot->SetHorizontalAlignment(HAlign_Center);
			CountSlot->SetVerticalAlignment(VAlign_Center);
			CountSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
		if (UOverlaySlot* BadgeSlot = Holder->AddChildToOverlay(Sized(Badge, FVector2D(76.f, 76.f))))
		{
			BadgeSlot->SetHorizontalAlignment(bHoly ? HAlign_Right : HAlign_Left);
			BadgeSlot->SetVerticalAlignment(VAlign_Top);
			BadgeSlot->SetPadding(bHoly ? FMargin(0.f, -8.f, -8.f, 0.f) : FMargin(-8.f, -8.f, 0.f, 0.f));
		}
		Holder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

		UCanvasPanelSlot* RelicSlot = RootCanvas->AddChildToCanvas(Holder);
		RelicSlot->SetAnchors(FAnchors(bHoly ? 0.f : 1.f, 1.f));
		RelicSlot->SetAlignment(FVector2D(bHoly ? 0.f : 1.f, 1.f));
		RelicSlot->SetPosition(FVector2D(bHoly ? 40.f : -40.f, -44.f));
		RelicSlot->SetAutoSize(true);

		RelicButtons.Add(Button);
		RelicIcons.Add(Icon);
		RelicCounts.Add(Count);
		RelicBadges.Add(Holder);
	}

	// ---- Trick-or-Treat packet (bottom-left, above Holy Light): appears when one is waiting ----
	{
		UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>();
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		PacketIcon = MakeIcon(IconStar, Pumpkin, 74.f, 1.f, 0.8f);
		Column->AddChildToVerticalBox(PacketIcon)->SetHorizontalAlignment(HAlign_Center);
		Column->AddChildToVerticalBox(MakeText(TEXT("TRICK OR"), false, 28.f, FLinearColor(1.f, 0.85f, 0.6f), 3.f))->SetHorizontalAlignment(HAlign_Center);
		Column->AddChildToVerticalBox(MakeText(TEXT("TREAT?"), false, 34.f, FLinearColor(1.f, 0.85f, 0.6f), 3.f))->SetHorizontalAlignment(HAlign_Center);
		if (UOverlaySlot* ColumnSlot = Face->AddChildToOverlay(Column))
		{
			ColumnSlot->SetHorizontalAlignment(HAlign_Center);
			ColumnSlot->SetVerticalAlignment(VAlign_Center);
		}
		UButton* Button = MakeButton(Face, FVector2D(210.f, 210.f), Pumpkin * 0.55f, Pumpkin2, ActPacket);
		UWidget* Holder = Sized(Button, FVector2D(210.f, 210.f));
		Holder->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Holder->SetVisibility(ESlateVisibility::Collapsed);
		PacketButton = Holder;
		UCanvasPanelSlot* PacketSlot = RootCanvas->AddChildToCanvas(Holder);
		PacketSlot->SetAnchors(FAnchors(0.f, 1.f));
		PacketSlot->SetAlignment(FVector2D(0.f, 1.f));
		PacketSlot->SetPosition(FVector2D(30.f, -270.f));
		PacketSlot->SetAutoSize(true);
	}

	// ---- Hint line (bottom centre, between the relics) ----
	HintText = MakeText(TEXT(""), false, 30.f, Lavender, 3.f);
	HintText->SetAutoWrapText(true);
	HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
	HintSlot = RootCanvas->AddChildToCanvas(HintText);
	// Top-left, under the MENU pill, so it is never over the tray.
	HintText->SetJustification(ETextJustify::Left);
	HintSlot->SetAnchors(FAnchors(0.f, 0.f));
	HintSlot->SetAlignment(FVector2D(0.f, 0.f));
	HintSlot->SetPosition(FVector2D(34.f, 150.f));
	HintSlot->SetSize(FVector2D(480.f, 130.f));

	// ---- "HOLD" label, projected under the reserve slot each frame ----
	HoldLabel = MakeText(TEXT("HOLD"), true, 26.f, FLinearColor(0.75f, 0.55f, 1.f), 3.f);
	HoldLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	HoldLabel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* HoldSlot = RootCanvas->AddChildToCanvas(HoldLabel);
	HoldSlot->SetAutoSize(true);
	HoldSlot->SetAlignment(FVector2D(0.5f, 0.f));

	// ---- Popups ----
	PopupLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
	PopupLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* PopupSlot = RootCanvas->AddChildToCanvas(PopupLayer);
	PopupSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	PopupSlot->SetOffsets(FMargin(0.f));

	// ---- Modal cards (menu, level intro, results) ----
	UOverlay* Cards = WidgetTree->ConstructWidget<UOverlay>();
	CardDim = WidgetTree->ConstructWidget<UImage>();
	CardDim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.01f, 0.62f));
	if (UOverlaySlot* DimSlot = Cards->AddChildToOverlay(CardDim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>();
	CardBg = MakePanel(PanelFill * 1.3f, PanelFill2, Rim, 0.7f, 0.07f, 0.35f);
	// Panel metrics are in units of the short side (the card's width), so the rim needs to be thinner than on bars.
	CardBg->GetDynamicMaterial()->SetScalarParameterValue(TEXT("RimWidth"), 0.028f);
	if (UOverlaySlot* BgSlot = Card->AddChildToOverlay(CardBg))
	{
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
	}
	CardContent = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UOverlaySlot* ContentSlot = Card->AddChildToOverlay(CardContent))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		// The panel shader draws its frame and glow about 47 units inside the card, so the padding is that much deeper.
		ContentSlot->SetPadding(FMargin(100.f, 135.f, 100.f, 145.f));   // 740 units of the 940-wide card are usable
	}
	CardPanel = Sized(Card, FVector2D(940.f, 0.f));
	CardPanel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	// The card shrinks (never grows) to fit whatever the screen leaves it, so a short landscape window still shows all of it.
	UScaleBox* CardFit = WidgetTree->ConstructWidget<UScaleBox>();
	CardFit->SetStretch(EStretch::ScaleToFit);
	CardFit->SetStretchDirection(EStretchDirection::DownOnly);
	CardFit->AddChild(CardPanel);
	if (UOverlaySlot* CardSlot = Cards->AddChildToOverlay(CardFit))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetVerticalAlignment(VAlign_Fill);
		CardSlot->SetPadding(FMargin(24.f, 36.f));
	}

	CardLayer = Cards;
	CardLayer->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* CardsSlot = RootCanvas->AddChildToCanvas(Cards);
	CardsSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	CardsSlot->SetOffsets(FMargin(0.f));
}

// --- Cards ---------------------------------------------------------------------

void UPuzzleHUDWidget::ShowCard(EPuzzleCard Card)
{
	CurrentCard = Card;
	CardAge = 0.f;
	if (Card != EPuzzleCard::Trick)
	{
		WheelSegment = -1;
		WheelSpinner = nullptr;
	}
	if (Card == EPuzzleCard::None)
	{
		CardLayer->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	BuildCardContent(Card);
	CardLayer->SetVisibility(ESlateVisibility::Visible);
}

void UPuzzleHUDWidget::RefreshCard()
{
	if (CurrentCard != EPuzzleCard::None)
	{
		BuildCardContent(CurrentCard);
	}
}

void UPuzzleHUDWidget::BuildCardContent(EPuzzleCard Card)
{
	using namespace UIStyle;

	CardContent->ClearChildren();
	CardStars.Reset();

	const APuzzleGameMode* GameMode = GetGameMode();
	const UPuzzleManager* Rules = GameMode ? GameMode->PuzzleManager.Get() : nullptr;
	const UPuzzleSaveGame* Save = GameMode ? GameMode->SaveGame.Get() : nullptr;

	auto Add = [this](UWidget* Widget, float Top = 0.f, EHorizontalAlignment Align = HAlign_Center)
	{
		UVerticalBoxSlot* ContentSlot = CardContent->AddChildToVerticalBox(Widget);
		ContentSlot->SetHorizontalAlignment(Align);
		ContentSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		return ContentSlot;
	};
	auto Btn = [this](const FString& Label, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Fill2, int32 Action, int32 Param = 0) -> UWidget*
	{
		return Sized(MakeTextButton(Label, Size, Fill, Fill2, Action, Param), Size);
	};
	auto ButtonRow = [this](UWidget* Left, UWidget* Right)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(Left)->SetPadding(FMargin(14.f, 0.f));
		Row->AddChildToHorizontalBox(Right)->SetPadding(FMargin(14.f, 0.f));
		return Row;
	};
	switch (Card)
	{
	case EPuzzleCard::Tutorial:
		BuildTutorialPage();
		break;
	case EPuzzleCard::Menu:
	{
		Add(MakeText(TEXT("Puzzle"), true, 84.f, Gold, 6.f));
		Add(MakeText(TEXT("Magic"), true, 108.f, Gold, 7.f), -30.f);
		Add(MakeText(TEXT("a gothic block puzzle"), false, 32.f, Lavender, 3.f), -6.f);

		Add(Btn(TEXT("PLAY"), FVector2D(560.f, 136.f), Emerald, Emerald2, ActPlay), 36.f);
		const AGridManager* Board = GameMode ? GameMode->GridManager.Get() : nullptr;
		Add(MakeText(FString::Printf(TEXT("course  %dx%d"), Board ? Board->GridWidth : 8, Board ? Board->GridHeight : 8), false, 30.f, Cyan, 3.f), 6.f);
		const int32 Best = Save ? Save->BestScore : 0;
		Add(MakeText(Best > 0 ? FString::Printf(TEXT("best  %d"), Best) : TEXT("no best yet"), false, 30.f, PaleGold, 3.f), 2.f);
		Add(Btn(TEXT("SELECT COURSE"), FVector2D(520.f, 100.f), Amethyst, Amethyst2, ActCourses), 22.f);
		Add(Btn(TEXT("PLAY OPTIONS"), FVector2D(520.f, 100.f), Amethyst, Amethyst2, ActOptions), 14.f);
		Add(Btn(TEXT("DEMO"), FVector2D(520.f, 100.f), Amethyst, Amethyst2, ActStartDemo), 14.f);
		Add(Btn(TEXT("HOW TO PLAY"), FVector2D(520.f, 100.f), Amethyst, Amethyst2, ActHowTo), 14.f);
		break;
	}
	case EPuzzleCard::Pause:
	{
		Add(MakeText(TEXT("Menu"), true, 84.f, Gold, 6.f));
		if (GameMode && GameMode->bAutoPlayEnabled)
		{
			Add(MakeText(TEXT("the computer is playing"), false, 32.f, Cyan, 3.f), 10.f);
			Add(Btn(TEXT("TAKE OVER"), FVector2D(560.f, 136.f), Emerald, Emerald2, ActTakeOver), 40.f);
		}
		else
		{
			Add(Btn(TEXT("RESUME"), FVector2D(560.f, 136.f), Emerald, Emerald2, ActResume), 40.f);
		}
		Add(Btn(TEXT("MAIN MENU"), FVector2D(560.f, 116.f), Amethyst, Amethyst2, ActMenu), 24.f);
		break;
	}
	case EPuzzleCard::Courses:
	{
		Add(MakeText(TEXT("Select course"), true, 52.f, Gold, 5.f));
		Add(MakeText(TEXT("board width x height"), false, 30.f, Lavender, 3.f), 6.f);
		const AGridManager* Board = GameMode ? GameMode->GridManager.Get() : nullptr;
		static const int32 Courses[15][2] = {
			{4, 4}, {4, 5}, {4, 6}, {4, 7}, {4, 8},
			{5, 5}, {5, 6}, {5, 7}, {5, 8}, {6, 6},
			{6, 7}, {6, 8}, {7, 7}, {7, 8}, {8, 8} };
		for (int32 Row = 0; Row < 3; ++Row)
		{
			UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>();
			for (int32 Col = 0; Col < 5; ++Col)
			{
				const int32 W = Courses[Row * 5 + Col][0];
				const int32 H = Courses[Row * 5 + Col][1];
				const bool bCurrent = Board && Board->GridWidth == W && Board->GridHeight == H;
				UWidget* Button = Btn(FString::Printf(TEXT("%dx%d"), W, H), FVector2D(128.f, 104.f), bCurrent ? Emerald : Amethyst, bCurrent ? Emerald2 : Amethyst2, ActCourse, W * 10 + H);
				RowBox->AddChildToHorizontalBox(Button)->SetPadding(FMargin(5.f, 0.f));
			}
			Add(RowBox, Row == 0 ? 30.f : 14.f);
		}
		Add(Btn(TEXT("BACK"), FVector2D(380.f, 110.f), Amethyst, Amethyst2, ActMenu), 40.f);
		break;
	}
	case EPuzzleCard::Options:
	{
		const bool bRelics = Rules && Rules->bRelicsEnabled;
		const bool bBonus = Rules && Rules->bBonusTilesEnabled;
		Add(MakeText(TEXT("Play options"), true, 64.f, Gold, 5.f));
		Add(Btn(FString::Printf(TEXT("RELICS  %s"), bRelics ? TEXT("ON") : TEXT("OFF")), FVector2D(620.f, 120.f), bRelics ? Emerald : Ruby, bRelics ? Emerald2 : Ruby2, ActToggleRelics), 40.f);
		Add(MakeText(TEXT("combos, relics and luck"), false, 28.f, Lavender, 3.f), 6.f);
		Add(Btn(FString::Printf(TEXT("BONUS TILES  %s"), bBonus ? TEXT("ON") : TEXT("OFF")), FVector2D(620.f, 120.f), bBonus ? Emerald : Ruby, bBonus ? Emerald2 : Ruby2, ActToggleBonus), 30.f);
		Add(MakeText(TEXT("special tiles worth extra points"), false, 28.f, Lavender, 3.f), 6.f);
		Add(Btn(TEXT("BACK"), FVector2D(380.f, 110.f), Amethyst, Amethyst2, ActMenu), 44.f);
		break;
	}
	case EPuzzleCard::Packet:
	{
		Add(MakeText(TEXT("Trick or"), true, 64.f, Pumpkin, 6.f));
		Add(MakeText(TEXT("Treat?"), true, 96.f, Gold, 7.f), -20.f);
		Add(MakeText(TEXT("someone is knocking at the door..."), false, 32.f, Lavender, 3.f), 4.f);
		Add(ButtonRow(Btn(TEXT("TREAT"), FVector2D(320.f, 150.f), Emerald, Emerald2, ActTreat), Btn(TEXT("TRICK"), FVector2D(320.f, 150.f), Ruby, Ruby2, ActTrick)), 40.f);
		UTextBlock* Explain = MakeText(TEXT("treat: buy luck, a tile or a relic\ntrick: spin the wheel of fortunes"), false, 28.f, FLinearColor(0.9f, 0.86f, 1.f), 3.f);
		Explain->SetJustification(ETextJustify::Center);
		Add(Explain, 16.f);
		Add(Btn(TEXT("LATER"), FVector2D(300.f, 96.f), Amethyst, Amethyst2, ActPacketLater), 30.f);
		break;
	}
	case EPuzzleCard::Treat:
	{
		Add(MakeText(TEXT("Treat!"), true, 72.f, Gold, 6.f));
		const TArray<FTreatOffer>& Offers = UTrickOrTreat::Offers();
		for (int32 Index = 0; Index < Offers.Num(); ++Index)
		{
			const FTreatOffer& Offer = Offers[Index];
			if (Index == 0 || Offer.bBundle != Offers[Index - 1].bBundle)
			{
				Add(MakeText(Offer.bBundle ? TEXT("HALLOWEEN BUNDLES") : TEXT("SINGLE TREATS"), true, 30.f, Offer.bBundle ? Pumpkin : Lavender, 3.f), Index == 0 ? 18.f : 22.f);
			}
			UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>();
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			UVerticalBox* Words = WidgetTree->ConstructWidget<UVerticalBox>();
			Words->AddChildToVerticalBox(MakeText(Offer.Title, false, 34.f, FLinearColor::White, 4.f));
			Words->AddChildToVerticalBox(MakeText(Offer.Detail, false, 22.f, FLinearColor(0.85f, 1.f, 0.85f), 3.f));
			UHorizontalBoxSlot* WordsSlot = Row->AddChildToHorizontalBox(Words);
			WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			WordsSlot->SetVerticalAlignment(VAlign_Center);
			Row->AddChildToHorizontalBox(MakeText(Offer.Price, false, 38.f, PaleGold, 4.f))->SetVerticalAlignment(VAlign_Center);
			if (UOverlaySlot* RowSlot = Face->AddChildToOverlay(Row))
			{
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
				RowSlot->SetVerticalAlignment(VAlign_Center);
				RowSlot->SetPadding(FMargin(40.f, 0.f, 40.f, 6.f));
			}
			const FVector2D Size(740.f, 96.f);
			Add(Sized(MakeButton(Face, Size, Offer.bBundle ? Pumpkin * 0.5f : Emerald, Offer.bBundle ? Pumpkin2 : Emerald2, ActBuyTreat, Index), Size), 8.f);
		}
		if (GameMode && GameMode->TrickOrTreat && GameMode->TrickOrTreat->IsTestStore())
		{
			Add(MakeText(TEXT("test store: no money is taken"), false, 26.f, Lavender, 3.f), 14.f);
		}
		Add(Btn(TEXT("BACK"), FVector2D(300.f, 96.f), Amethyst, Amethyst2, ActPacket), 22.f);
		break;
	}
	case EPuzzleCard::Trick:
	{
		Add(MakeText(TEXT("Trick!"), true, 84.f, FLinearColor(1.f, 0.3f, 0.25f), 6.f));

		// The wheel: a round panel, eight fortunes round its rim (good in green, bad in red) and spokes between
		// them. It turns as one; the pointer above it stays put.
		UOverlay* Wheel = WidgetTree->ConstructWidget<UOverlay>();
		UImage* Disc = MakePanel(PanelFill * 1.8f, PanelFill2, Rim, 1.f, 0.5f, 0.45f);
		if (UOverlaySlot* DiscSlot = Wheel->AddChildToOverlay(Disc))
		{
			DiscSlot->SetHorizontalAlignment(HAlign_Fill);
			DiscSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UCanvasPanel* WheelFace = WidgetTree->ConstructWidget<UCanvasPanel>();
		if (UOverlaySlot* FaceSlot = Wheel->AddChildToOverlay(WheelFace))
		{
			FaceSlot->SetHorizontalAlignment(HAlign_Fill);
			FaceSlot->SetVerticalAlignment(VAlign_Fill);
		}
		for (int32 Segment = 0; Segment < UTrickOrTreat::SegmentCount; ++Segment)
		{
			const float Angle = 360.f / UTrickOrTreat::SegmentCount * Segment;
			UImage* Spoke = MakePanel(Rim, Rim * 0.4f, Rim, 0.05f, 0.5f, 0.3f);
			UCanvasPanelSlot* SpokeSlot = WheelFace->AddChildToCanvas(Spoke);
			SpokeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			SpokeSlot->SetAlignment(FVector2D(0.5f, 1.f));
			SpokeSlot->SetSize(FVector2D(6.f, 250.f));
			Spoke->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
			Spoke->SetRenderTransformAngle(Angle + 180.f / UTrickOrTreat::SegmentCount);

			const bool bGood = UTrickOrTreat::IsGoodSegment(Segment);
			UTextBlock* Label = MakeText(UTrickOrTreat::SegmentLabel(Segment), false, 21.f, bGood ? LuckGreen : FLinearColor(1.f, 0.35f, 0.3f), 4.f);
			UCanvasPanelSlot* LabelSlot = WheelFace->AddChildToCanvas(Label);
			LabelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			LabelSlot->SetAutoSize(true);
			const float Radians = FMath::DegreesToRadians(Angle);
			LabelSlot->SetPosition(FVector2D(FMath::Sin(Radians), -FMath::Cos(Radians)) * 185.f);
			Label->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Label->SetRenderTransformAngle(Angle);
		}
		UImage* Hub = MakeIcon(IconMoon, Pumpkin, 96.f, 1.f, 0.8f);
		if (UOverlaySlot* HubSlot = Wheel->AddChildToOverlay(Hub))
		{
			HubSlot->SetHorizontalAlignment(HAlign_Center);
			HubSlot->SetVerticalAlignment(VAlign_Center);
		}
		WheelSpinner = Sized(Wheel, FVector2D(540.f, 540.f));
		WheelSpinner->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		WheelSpinner->SetRenderTransformAngle(0.f);

		UOverlay* Stand = WidgetTree->ConstructWidget<UOverlay>();
		Stand->AddChildToOverlay(WheelSpinner);
		UImage* Pointer = MakeIcon(IconStar, Gold, 70.f, 1.f, 1.2f);
		if (UOverlaySlot* PointerSlot = Stand->AddChildToOverlay(Pointer))
		{
			PointerSlot->SetHorizontalAlignment(HAlign_Center);
			PointerSlot->SetVerticalAlignment(VAlign_Top);
			PointerSlot->SetPadding(FMargin(0.f, -34.f, 0.f, 0.f));
		}
		Add(Stand, 30.f);
		WheelResult = MakeText(TEXT(" "), false, 36.f, PaleGold, 4.f);
		Add(WheelResult, 22.f);
		break;
	}
	case EPuzzleCard::GameOver:
	{
		const bool bOutOfMoves = Rules && Rules->IsOutOfMoves();
		Add(MakeText(TEXT("Game Over"), true, 70.f, FLinearColor(1.f, 0.25f, 0.3f), 6.f));
		Add(MakeText(FString::Printf(TEXT("%d"), Rules ? Rules->Score : 0), false, 120.f, Gold, 7.f), 20.f);
		if (GameMode && GameMode->bLastNewBest)
		{
			UTextBlock* NewBest = MakeText(TEXT("NEW BEST!"), false, 54.f, Cyan, 5.f);
			NewBest->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			CardStars.Add(NewBest);
			Add(NewBest, 4.f);
		}
		else
		{
			Add(MakeText(FString::Printf(TEXT("best  %d"), Save ? Save->BestScore : 0), false, 38.f, PaleGold, 3.f), 4.f);
		}
		Add(MakeText(bOutOfMoves ? TEXT("out of moves") : TEXT("no room left"), false, 32.f, Lavender, 3.f), 12.f);
		Add(ButtonRow(Sized(MakeTextButton(TEXT("MENU"), FVector2D(280.f, 124.f), Amethyst, Amethyst2, ActMenu), FVector2D(280.f, 124.f)),
			Sized(MakeTextButton(TEXT("PLAY AGAIN"), FVector2D(380.f, 124.f), Emerald, Emerald2, ActRetry), FVector2D(380.f, 124.f))), 50.f);
		break;
	}
	default:
		break;
	}
}

void UPuzzleHUDWidget::HandleAction(int32 Action, int32 Param)
{
	APuzzleGameMode* GameMode = GetGameMode();
	if (!GameMode)
	{
		return;
	}
	switch (Action)
	{
	case ActPlay:
		GameMode->SetAutoPlay(false);
		GameMode->StartEndless();
		break;
	case ActRetry:   GameMode->Retry(); break;
	case ActMenu:    GameMode->ShowMenu(); break;
	case ActPause:   GameMode->OpenPauseMenu(); break;
	case ActResume:  GameMode->ClosePauseMenu(); break;
	case ActTakeOver: GameMode->TakeOver(); break;
	case ActStartDemo: GameMode->StartDemo(); break;
	case ActCourses: ShowCard(EPuzzleCard::Courses); break;
	case ActCourse:
		GameMode->SetCourse(Param / 10, Param % 10);
		GameMode->ShowMenu();
		break;
	case ActOptions: ShowCard(EPuzzleCard::Options); break;
	case ActToggleRelics:
	case ActToggleBonus:
		if (const UPuzzleManager* Rules = GameMode->PuzzleManager.Get())
		{
			const bool bRelics = Rules->bRelicsEnabled != (Action == ActToggleRelics);
			const bool bBonus = Rules->bBonusTilesEnabled != (Action == ActToggleBonus);
			GameMode->SetOptions(bRelics, bBonus);
			RefreshCard();
		}
		break;
	case ActHoly:    GameMode->RequestRelic(ERelic::HolyLight); break;
	case ActReroll:  GameMode->RequestRelic(ERelic::Reroll); break;
	case ActHowTo:   ShowTutorial(); break;
	case ActPacket:  GameMode->OpenPacket(); break;
	case ActTreat:   GameMode->ChooseTreat(); break;
	case ActTrick:   GameMode->ChooseTrick(); break;
	case ActPacketLater: GameMode->ClosePacket(); break;
	case ActBuyTreat: GameMode->BuyTreat(Param); break;
	case ActTutorial:
		if (Param < 0)
		{
			GameMode->ShowMenu();
		}
		else
		{
			TutorialPage = Param;
			ShowCard(EPuzzleCard::Tutorial);
		}
		break;
	default: break;
	}
}

void UPuzzleHUDWidget::ShowTutorial(int32 Page)
{
	TutorialPage = Page;
	ShowCard(EPuzzleCard::Tutorial);
}

void UPuzzleHUDWidget::BuildTutorialPage()
{
	using namespace UIStyle;

	struct FSign { float Shape; FLinearColor Color; const TCHAR* Label; };
	struct FPage { const TCHAR* Title; TArray<FSign> Signs; const TCHAR* Body; };
	TArray<FPage> Pages;
	Pages.Add({ TEXT("The Rite"),
	  { { IconLine, Cyan, TEXT("ROUTE") } },
	  TEXT("Drag a piece onto the board. Build a ROUTE: a chain of tiles, each hand pointing at the next. It must START on a tile at the edge pointing straight away from that side, and END on a tile at an edge pointing straight out of it.\n\n")
	  TEXT("The whole chain breaks apart. A chain that leaves through the side it started from is a closed CIRCUIT: it scores nothing and costs one step of your combo.\n\n")
	  TEXT("The rite ends when no piece fits. The fourth slot is HOLD: park a piece there for later.") });
	if (UPuzzleManager::bMoveBudgetEnabled)
	{
		Pages.Last().Body = TEXT("Drag a piece from the tray onto the board. Fill a whole ROW or COLUMN and it breaks apart.\n\n")
			TEXT("Every piece costs a move; clears win moves back. The rite ends when the moves run out, or when no piece can fit. The fourth tray slot is HOLD: park a piece there for later.");
	}
	const APuzzleGameMode* TutorialMode = GetGameMode();
	const UPuzzleManager* TutorialRules = TutorialMode ? TutorialMode->PuzzleManager.Get() : nullptr;
	if (TutorialRules && TutorialRules->bBonusTilesEnabled)
	{
		Pages.Add({ TEXT("Bonus Tiles"),
		  { { IconStar, PuzzleTypes::BonusToColor(EPuzzleBonus::Basic), TEXT("BASIC") }, { IconStar, PuzzleTypes::BonusToColor(EPuzzleBonus::Outgoing), TEXT("OUT") }, { IconStar, PuzzleTypes::BonusToColor(EPuzzleBonus::Incoming), TEXT("IN") } },
		  TEXT("Tiles without a skeleton hand appear as your score grows: a carved PUMPKIN (BASIC) every 1000 points, and a full potion (OUT) with an empty potion (IN) every 10000. There are never more than 2 pumpkins, or 1 full or 1 empty potion, on the board.\n\n")
		  TEXT("A PUMPKIN bursts when a chain of hands joins it to a side. The full potion pours a chain out through any neighbour and empties as the chain leaves the board. The empty potion takes a chain arriving from any side, starting at a side, and fills. A chain from the full potion to the empty one is worth 1000.") });
	}
	if (TutorialRules && TutorialRules->bComboEnabled)
	{
		Pages.Add({ TEXT("Combos"),
		  { { IconStar, PaleGold, TEXT("COMBO") } },
		  TEXT("Clear routes on following moves to build a COMBO: your route points are multiplied by it. Several routes at once climb it faster.\n\n")
		  TEXT("The three stars are its lifeline: each move without a clear burns one. Every 3 combo steps grants a RELIC.") });
	}
	if (TutorialRules && TutorialRules->bRelicsEnabled)
	{
		Pages.Add({ TEXT("Relics"),
		  { { IconSun, Gold, TEXT("HOLY LIGHT") }, { IconReroll, Cyan, TEXT("REROLL") } },
		  TEXT("HOLY LIGHT: tap the board to purge a 3x3 area. It scores nothing.\nREROLL: replace the whole tray with fresh pieces. It scores nothing.") });
	}
	if (TutorialRules && TutorialRules->bLuckEnabled)
	{
		Pages.Add({ TEXT("Luck"),
		  { { IconMoon, LuckGreen, TEXT("LUCK") } },
		  TEXT("Every combo step gathers +6 LUCK. Relics spend it: Holy Light -20, Reroll -12.\n\n")
		  TEXT("The candles burn with your luck. Let it die, and the things in the dark wake.") });
	}
	if (TutorialRules && TutorialRules->bRelicsEnabled)
	{
		Pages.Add({ TEXT("Trick or Treat"),
		  { { IconStar, Pumpkin, TEXT("TREAT") }, { IconReroll, Danger, TEXT("TRICK") } },
		  TEXT("Every 300 points someone knocks: a TRICK OR TREAT packet appears by your relics. Tap it.\n\n")
		  TEXT("TREAT: buy a single treat (Lucky Candle, Sugar Skull, Holy Light) or a Halloween bundle (Witch's Brew, Pumpkin Patch, Haunted Hoard).\n")
		  TEXT("TRICK: spin the wheel for free. Half its fortunes help (luck, a reroll, a double clear, a pumpkin), half hurt (lost luck, a stray tile, a half clear, a broken combo).") });
	}
	TutorialPage = FMath::Clamp(TutorialPage, 0, Pages.Num() - 1);
	const FPage& Page = Pages[TutorialPage];
	const bool bLast = TutorialPage == Pages.Num() - 1;

	auto Add = [this](UWidget* Widget, float Top = 0.f, EHorizontalAlignment Align = HAlign_Center)
	{
		UVerticalBoxSlot* ContentSlot = CardContent->AddChildToVerticalBox(Widget);
		ContentSlot->SetHorizontalAlignment(Align);
		ContentSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
	};

	Add(MakeText(FString::Printf(TEXT("HOW TO PLAY  %d / %d"), TutorialPage + 1, Pages.Num()), false, 28.f, Lavender, 3.f));
	// Cinzel is wide: long titles step down so they stay inside the card.
	Add(MakeText(Page.Title, true, FCString::Strlen(Page.Title) > 10 ? 56.f : 72.f, Gold, 6.f), 6.f);

	// The signs of this page, each an embossed coin with its name under it.
	UHorizontalBox* Signs = WidgetTree->ConstructWidget<UHorizontalBox>();
	const float SignSize = Page.Signs.Num() > 3 ? 104.f : 150.f;
	for (int32 Index = 0; Index < Page.Signs.Num(); ++Index)
	{
		const FSign& Sign = Page.Signs[Index];
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		UImage* Icon = MakeIcon(Sign.Shape, Sign.Color, SignSize, 1.f, 0.5f);
		Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		CardStars.Add(Icon); // springs in one after another, like result stars
		Column->AddChildToVerticalBox(Icon)->SetHorizontalAlignment(HAlign_Center);
		Column->AddChildToVerticalBox(MakeText(Sign.Label, false, 26.f, Sign.Color * 0.6f + FLinearColor(0.4f, 0.4f, 0.4f), 3.f))->SetHorizontalAlignment(HAlign_Center);
		Signs->AddChildToHorizontalBox(Column)->SetPadding(FMargin(Page.Signs.Num() > 3 ? 8.f : 26.f, 0.f));
	}
	Add(Signs, 24.f);

	UTextBlock* Body = MakeText(Page.Body, false, 30.f, FLinearColor(0.9f, 0.86f, 1.f), 3.f);
	Body->SetAutoWrapText(true);
	Add(Body, 30.f, HAlign_Fill);

	// Page dots.
	UHorizontalBox* Dots = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 Index = 0; Index < Pages.Num(); ++Index)
	{
		const bool bHere = Index == TutorialPage;
		Dots->AddChildToHorizontalBox(MakeIcon(IconStar, bHere ? Gold : FLinearColor(0.3f, 0.22f, 0.35f), 34.f, bHere ? 1.f : 0.f, bHere ? 0.5f : 0.f))->SetPadding(FMargin(4.f, 0.f));
	}
	Add(Dots, 34.f);

	const FVector2D ButtonSize(330.f, 120.f);
	UWidget* Back = Sized(MakeTextButton(TutorialPage == 0 ? TEXT("SKIP") : TEXT("BACK"), ButtonSize, Amethyst, Amethyst2, ActTutorial, TutorialPage - 1), ButtonSize);
	UWidget* Next = Sized(MakeTextButton(bLast ? TEXT("PLAY") : TEXT("NEXT"), ButtonSize, Emerald, Emerald2, ActTutorial, bLast ? -1 : TutorialPage + 1), ButtonSize);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->AddChildToHorizontalBox(Back)->SetPadding(FMargin(14.f, 0.f));
	Row->AddChildToHorizontalBox(Next)->SetPadding(FMargin(14.f, 0.f));
	Add(Row, 30.f);
}

// --- Events -> popups ------------------------------------------------------------

void UPuzzleHUDWidget::ResetRound()
{
	DisplayedScore = 0.f;
	LastCombo = 0;
	LastLuck = -1;
	for (const FPopup& Popup : Popups)
	{
		if (UWidget* Widget = Popup.Widget.Get())
		{
			Widget->RemoveFromParent();
		}
	}
	Popups.Reset();
}

void UPuzzleHUDWidget::AddPopup(UWidget* Content, const FVector2D& ScreenFraction, float Life, float Scale, float Delay, float Rise)
{
	FPopup& Popup = Popups.AddDefaulted_GetRef();
	Popup.Widget = Content;
	Popup.Screen = ScreenFraction;
	Popup.Life = Life;
	Popup.Scale = Scale;
	Popup.Age = -Delay;
	Popup.Rise = Rise;

	Content->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Content->SetRenderOpacity(0.f);
	UCanvasPanelSlot* PopupSlot = PopupLayer->AddChildToCanvas(Content);
	PopupSlot->SetAutoSize(true);
	PopupSlot->SetAlignment(FVector2D(0.5f, 0.5f));
}

void UPuzzleHUDWidget::AddWorldPopup(UWidget* Content, const FVector& World, float Life, float Scale, float Delay)
{
	AddPopup(Content, FVector2D::ZeroVector, Life, Scale, Delay, 90.f);
	Popups.Last().bWorld = true;
	Popups.Last().World = World;
}

void UPuzzleHUDWidget::ShowClear(const FPuzzleClearEvent& Event)
{
	using namespace UIStyle;
	const FClearResult& Result = Event.Result;

	if (Event.bHolyLight)
	{
		AddPopup(MakeText(TEXT("Holy Light!"), true, 78.f, PaleGold, 6.f), FVector2D(0.5f, 0.2f), 1.5f, 1.f);
	}
	else if (Result.Lines == 0)
	{
		if (Result.CircuitCells > 0)
		{
			AddPopup(MakeText(TEXT("Circuit!"), false, 96.f, FLinearColor(0.85f, 0.5f, 1.f), 7.f), FVector2D(0.5f, 0.2f), 1.4f, 1.f);
		}
	}
	else
	{
		static const TCHAR* Words[] = { TEXT("Nice!"), TEXT("Great!"), TEXT("Amazing!"), TEXT("Magical!!"), TEXT("LEGENDARY!!") };
		static const FLinearColor Colors[] = { FLinearColor(0.3f, 0.9f, 1.f), FLinearColor(0.35f, 1.f, 0.25f), FLinearColor(1.f, 0.4f, 1.f), Gold, FLinearColor(1.f, 0.3f, 0.2f) };
		const int32 Tier = FMath::Clamp(Result.Lines - 1, 0, 4);
		AddPopup(MakeText(Words[Tier], false, 96.f + 12.f * Tier, Colors[Tier], 7.f), FVector2D(0.5f, 0.2f), 1.4f, 1.f);
		if (Result.Circuits > 0)
		{
			AddPopup(MakeText(TEXT("Circuit!"), false, 60.f, FLinearColor(0.85f, 0.5f, 1.f), 5.f), FVector2D(0.5f, 0.38f), 1.4f, 1.f, 0.3f);
		}
	}


	float BonusRow = 0.f;
	for (const FBonusEvent& Bonus : Result.Bonuses)
	{
		const FString Label = Bonus.bLinked ? FString::Printf(TEXT("LINKED!  +%d"), Bonus.Points) : FString::Printf(TEXT("BONUS  +%d"), Bonus.Points);
		const FLinearColor Color = Bonus.bLinked ? Gold : PuzzleTypes::BonusToColor(Bonus.Kind);
		AddPopup(MakeText(Label, true, 56.f, Color, 5.f), FVector2D(0.5f, 0.3f + BonusRow), 1.6f, 1.f, 0.25f + BonusRow * 4.f);
		BonusRow += 0.06f;
	}

	if (!Event.bHolyLight)
	{
		AddWorldPopup(MakeText(FString::Printf(TEXT("+%d"), Event.Points), false, 70.f, FLinearColor::White, 5.f), Event.Centroid, 1.2f);
	}

	if (Event.BonusMoves > 0)
	{
		const float BarBottom = 246.f / FMath::Max(CanvasSize.Y, 1.f);
		AddPopup(MakeText(FString::Printf(TEXT("+%d MOVES"), Event.BonusMoves), false, 40.f, FLinearColor(0.35f, 1.f, 0.45f), 4.f), FVector2D(0.19f, BarBottom), 1.4f, 1.f, 0.1f, 40.f);
		MovesPop = 1.f;
	}
}

void UPuzzleHUDWidget::ShowComboBroken(int32 LostCombo)
{
	if (LostCombo >= 2)
	{
		AddPopup(MakeText(FString::Printf(TEXT("combo x%d lost"), LostCombo), false, 40.f, FLinearColor(0.55f, 0.5f, 0.65f), 3.f),
			ComboAnchorFraction + FVector2D(0.f, 70.f / FMath::Max(CanvasSize.Y, 1.f)), 1.2f, 1.f, 0.f, -50.f);
	}
}

void UPuzzleHUDWidget::ShowBonusSpawned(const FVector& WorldLocation)
{
	AddWorldPopup(MakeText(TEXT("BONUS TILE"), true, 40.f, UIStyle::PaleGold, 4.f), WorldLocation + FVector(0.f, 0.f, 60.f), 1.4f);
}

void UPuzzleHUDWidget::ShowPacketArrived()
{
	AddPopup(MakeText(TEXT("Trick or Treat?"), true, 64.f, UIStyle::Pumpkin, 6.f), FVector2D(0.5f, 0.24f), 2.2f, 1.f);
	PacketPop = 1.f;
}

void UPuzzleHUDWidget::ShowTreatBought(int32 Offer)
{
	const TArray<FTreatOffer>& Offers = UTrickOrTreat::Offers();
	if (!Offers.IsValidIndex(Offer))
	{
		return;
	}
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(MakeText(TEXT("Treat!"), true, 72.f, UIStyle::Gold, 6.f))->SetHorizontalAlignment(HAlign_Center);
	Column->AddChildToVerticalBox(MakeText(Offers[Offer].Title + TEXT(":  ") + Offers[Offer].Detail, false, 44.f, UIStyle::LuckGreen, 4.f))->SetHorizontalAlignment(HAlign_Center);
	AddPopup(Column, FVector2D(0.5f, 0.26f), 2.2f, 1.f);
}

void UPuzzleHUDWidget::ShowWheel(int32 Segment)
{
	ShowCard(EPuzzleCard::Trick);
	WheelSegment = Segment;
	WheelAge = 0.f;
	bWheelDone = false;
}

void UPuzzleHUDWidget::ShowTrickResult(int32 Segment, const FString& Line)
{
	const bool bGood = UTrickOrTreat::IsGoodSegment(Segment);
	if (WheelResult)
	{
		WheelResult->SetText(FText::FromString(Line));
		WheelResult->SetColorAndOpacity(FSlateColor(bGood ? UIStyle::LuckGreen : FLinearColor(1.f, 0.4f, 0.32f)));
	}
	AddPopup(MakeText(bGood ? TEXT("Lucky!") : TEXT("Tricked!"), false, 110.f, bGood ? UIStyle::LuckGreen : UIStyle::Danger, 7.f), FVector2D(0.5f, 0.16f), 1.6f, 1.f);
}

void UPuzzleHUDWidget::ShowRelicGained(ERelic Relic)
{
	const bool bHoly = Relic == ERelic::HolyLight;
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Row->AddChildToHorizontalBox(MakeIcon(bHoly ? UIStyle::IconSun : UIStyle::IconReroll, bHoly ? UIStyle::Gold : UIStyle::Cyan, 90.f, 1.f, 1.f))->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(MakeText(bHoly ? TEXT("+1 Holy Light") : TEXT("+1 Reroll"), false, 50.f, bHoly ? UIStyle::PaleGold : UIStyle::Cyan, 4.f));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	AddPopup(Row, FVector2D(0.5f, 0.3f), 1.8f, 1.f);
	RelicPop[static_cast<int32>(Relic)] = 1.f;
}

// --- Per-frame -------------------------------------------------------------------

void UPuzzleHUDWidget::TickPopups(float DeltaTime)
{
	APlayerController* PC = GetOwningPlayer();
	for (int32 Index = Popups.Num() - 1; Index >= 0; --Index)
	{
		FPopup& Popup = Popups[Index];
		UWidget* Widget = Popup.Widget.Get();
		Popup.Age += DeltaTime;
		if (!Widget || Popup.Age >= Popup.Life)
		{
			if (Widget)
			{
				Widget->RemoveFromParent();
			}
			Popups.RemoveAt(Index);
			continue;
		}
		if (Popup.Age < 0.f)
		{
			continue;
		}

		const float T = Popup.Age / Popup.Life;
		FVector2D Position = Popup.Screen * CanvasSize;
		if (Popup.bWorld)
		{
			if (!PC || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Popup.World, Position, true))
			{
				continue;
			}
		}
		Position.Y -= Popup.Rise * T;
		if (UCanvasPanelSlot* PopupSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			PopupSlot->SetPosition(Position);
		}

		const float PopIn = UIStyle::BackOut(Popup.Age / 0.28f);
		Widget->SetRenderScale(FVector2D(Popup.Scale * PopIn));
		Widget->SetRenderOpacity(T < 0.72f ? 1.f : 1.f - (T - 0.72f) / 0.28f);
	}
}

void UPuzzleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace UIStyle;

	Time += InDeltaTime;
	CanvasSize = MyGeometry.GetLocalSize();

	const APuzzleGameMode* GameMode = GetGameMode();
	const UPuzzleManager* Rules = GameMode ? GameMode->PuzzleManager.Get() : nullptr;
	if (!Rules)
	{
		return;
	}
	const bool bPlayingView = GameMode->Flow == EPuzzleFlow::Playing || GameMode->Flow == EPuzzleFlow::Finished;

	// Keep the panel shader's rounded corners round whatever the bar's width.
	auto SyncAspect = [](UImage* Image)
	{
		const FVector2D Size = Image ? Image->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
		if (Size.Y > 1.f)
		{
			if (UMaterialInstanceDynamic* MID = Image->GetDynamicMaterial())
			{
				MID->SetScalarParameterValue(TEXT("Aspect"), Size.X / Size.Y);
			}
		}
	};
	SyncAspect(TopBarBg);
	SyncAspect(CardBg);


	// Score counts up toward the real value.
	const float TargetScore = static_cast<float>(Rules->Score);
	DisplayedScore = FMath::FInterpTo(DisplayedScore, TargetScore, InDeltaTime, 6.f);
	if (FMath::Abs(TargetScore - DisplayedScore) < 0.5f)
	{
		DisplayedScore = TargetScore;
	}
	const FText ScoreNumber = FText::AsNumber(FMath::RoundToInt(DisplayedScore));
	ScoreText->SetText(ScoreNumber);
	const int32 ScoreChars = ScoreNumber.ToString().Len();
	const float ScoreSize = ScoreChars <= 6 ? 74.f : (ScoreChars == 7 ? 58.f : 46.f);
	if (!FMath::IsNearlyEqual(ScoreSize, ScoreFontSize))
	{
		ScoreFontSize = ScoreSize;
		ScoreText->SetFont(Font(false, ScoreSize, 5.f));
	}

	// Moves: red and throbbing when low, a bounce when moves are refunded.
	if (UPuzzleManager::bMoveBudgetEnabled)
	{
		const int32 Moves = Rules->MovesLeft;
		const bool bLow = Moves <= 5;
		MovesText->SetText(FText::AsNumber(Moves));
		MovesText->SetColorAndOpacity(FSlateColor(bLow ? Danger : Cyan));
		MovesPop = FMath::Max(MovesPop - InDeltaTime * 2.5f, 0.f);
		const float MovesScale = (bLow ? 1.f + 0.08f * FMath::Sin(Time * 10.f) : 1.f) + 0.35f * MovesPop * MovesPop;
		MovesText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		MovesText->SetRenderScale(FVector2D(MovesScale));
	}

	// Best score, live once this round passes it.
	const int32 Best = GameMode->SaveGame ? FMath::Max(GameMode->SaveGame->BestScore, Rules->Score) : Rules->Score;
	const FText BestNumber = FText::AsNumber(Best);
	BestText->SetText(BestNumber);
	const int32 BestChars = BestNumber.ToString().Len();
	const float BestSize = BestChars <= 6 ? 52.f : (BestChars == 7 ? 42.f : 34.f);
	if (!FMath::IsNearlyEqual(BestSize, BestFontSize))
	{
		BestFontSize = BestSize;
		BestText->SetFont(Font(false, BestSize, 5.f));
	}

	// Combo meter: pops when it grows, trembles on its last pip, hidden at zero.
	const int32 Combo = Rules->ComboStreak;
	if (Combo > LastCombo)
	{
		ComboPop = 1.f;
	}
	LastCombo = Combo;
	ComboPop = FMath::Max(ComboPop - InDeltaTime * 2.2f, 0.f);
	// The combo badge hangs in the gap between the two candle clusters: project that spot into the view each frame.
	{
		APlayerController* ComboPC = GetOwningPlayer();
		FVector2D ComboSpot;
		if (ComboPC && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(ComboPC, FVector(10.f, -620.f, 10.f), ComboSpot, true))
		{
			ComboAnchorFraction = ComboSpot / FVector2D(FMath::Max(CanvasSize.X, 1.f), FMath::Max(CanvasSize.Y, 1.f));
			if (UCanvasPanelSlot* ComboSlotNow = Cast<UCanvasPanelSlot>(ComboBadge->Slot))
			{
				ComboSlotNow->SetPosition(ComboSpot);
			}
		}
	}
	const bool bShowCombo = Rules->bComboEnabled && Combo > 0 && bPlayingView;
	ComboBadge->SetVisibility(bShowCombo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowCombo)
	{
		ComboText->SetText(FText::FromString(FString::Printf(TEXT("COMBO x%d!"), Combo)));
		const bool bDanger = Rules->ComboWindow <= 1;
		const float Wobble = bDanger ? FMath::Sin(Time * 30.f) * 3.f : 0.f;
		FWidgetTransform Transform;
		Transform.Scale = FVector2D(1.f + 0.3f * ComboPop * ComboPop + 0.03f * FMath::Sin(Time * 6.f));
		Transform.Angle = Wobble;
		ComboBadge->SetRenderTransform(Transform);
		if (UMaterialInstanceDynamic* MID = ComboBg->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(TEXT("Glow"), 0.3f + 0.7f * ComboPop + (Combo >= 6 ? 0.3f + 0.2f * FMath::Sin(Time * 8.f) : 0.f));
		}
		for (int32 Pip = 0; Pip < ComboPips.Num(); ++Pip)
		{
			const bool bLit = Pip < Rules->ComboWindow;
			if (UMaterialInstanceDynamic* MID = ComboPips[Pip]->GetDynamicMaterial())
			{
				MID->SetScalarParameterValue(TEXT("Fill"), bLit ? 1.f : 0.f);
				MID->SetScalarParameterValue(TEXT("Glow"), bLit ? 0.6f : 0.f);
				MID->SetVectorParameterValue(TEXT("Color"), bLit ? (bDanger ? Danger : PaleGold) : FLinearColor(0.35f, 0.2f, 0.4f));
			}
		}
	}

	// Relic buttons: counts, dimmed when empty, the armed Holy Light glows.
	const bool bInput = GameMode->IsBoardInputEnabled();
	const bool bTargeting = GameMode->InputHandler && GameMode->InputHandler->IsHolyTargeting();
	for (int32 RelicIndex = 0; RelicIndex < RelicButtons.Num(); ++RelicIndex)
	{
		const int32 Charges = Rules->GetRelicCharges(static_cast<ERelic>(RelicIndex));
		RelicCounts[RelicIndex]->SetText(FText::AsNumber(Charges));
		// Never disabled (Slate's disabled look greys the icon out); clicks are ignored by the game mode instead.
		RelicBadges[RelicIndex]->SetVisibility(bPlayingView && Rules->bRelicsEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		RelicBadges[RelicIndex]->SetRenderOpacity(Charges > 0 && (bInput || GameMode->bAutoPlayEnabled) ? 1.f : 0.5f);

		RelicPop[RelicIndex] = FMath::Max(RelicPop[RelicIndex] - InDeltaTime * 2.f, 0.f);
		const bool bArmed = bTargeting && RelicIndex == static_cast<int32>(ERelic::HolyLight);
		RelicBadges[RelicIndex]->SetRenderScale(FVector2D(1.f + 0.25f * RelicPop[RelicIndex] + (bArmed ? 0.06f * FMath::Sin(Time * 9.f) : 0.f)));
		if (UMaterialInstanceDynamic* MID = RelicIcons[RelicIndex]->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(TEXT("Glow"), bArmed ? 1.5f + 0.5f * FMath::Sin(Time * 9.f) : (Charges > 0 ? 0.5f : 0.f));
		}
	}
	MenuButton->SetVisibility(bPlayingView ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// Trick-or-Treat: the packet button bobs and glows while one waits; the wheel spins down to its fortune.
	const bool bPacketWaiting = GameMode->TrickOrTreat && GameMode->TrickOrTreat->IsAvailable() && !GameMode->bPacketOpen && GameMode->Flow == EPuzzleFlow::Playing;
	PacketButton->SetVisibility(bPacketWaiting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PacketPop = FMath::Max(PacketPop - InDeltaTime * 1.5f, 0.f);
	if (bPacketWaiting)
	{
		FWidgetTransform Transform;
		Transform.Scale = FVector2D(1.f + 0.06f * FMath::Sin(Time * 5.f) + 0.4f * PacketPop * PacketPop);
		Transform.Angle = 4.f * FMath::Sin(Time * 2.6f);
		PacketButton->SetRenderTransform(Transform);
		if (UMaterialInstanceDynamic* MID = PacketIcon->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(TEXT("Glow"), 0.8f + 0.6f * FMath::Sin(Time * 5.f) + 1.5f * PacketPop);
		}
	}
	if (CurrentCard == EPuzzleCard::Trick && WheelSegment >= 0 && WheelSpinner)
	{
		WheelAge += InDeltaTime;
		const float T = FMath::Min(WheelAge / WheelSpinSeconds, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - T, 3.f);
		// Five whole turns, then the chosen segment comes to rest under the pointer.
		const float Final = 360.f * 5.f - 360.f / UTrickOrTreat::SegmentCount * WheelSegment;
		WheelSpinner->SetRenderTransformAngle(Final * Eased);
		if (T >= 1.f && !bWheelDone)
		{
			bWheelDone = true;
			if (APuzzleGameMode* SpinMode = GetGameMode())
			{
				SpinMode->FinishTrick(WheelSegment);
			}
		}
	}

	// Luck: eased number and bar, pops with a floating +/- whenever it changes.
	const int32 LuckNow = Rules->GetLuck();
	LuckBadge->SetVisibility(bPlayingView && Rules->bLuckEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (LastLuck < 0)
	{
		DisplayedLuck = static_cast<float>(LuckNow);
	}
	else if (LuckNow != LastLuck && bPlayingView)
	{
		const int32 Delta = LuckNow - LastLuck;
		const FVector2D Under(1.f - (34.f + 125.f) / FMath::Max(CanvasSize.X, 1.f), (250.f + 96.f + 26.f) / FMath::Max(CanvasSize.Y, 1.f));
		AddPopup(MakeText(FString::Printf(TEXT("%+d luck"), Delta), false, 34.f, Delta > 0 ? LuckGreen : FLinearColor(1.f, 0.4f, 0.3f), 3.f), Under, 1.3f, 1.f, 0.f, Delta > 0 ? 40.f : -40.f);
		LuckPop = 1.f;
	}
	LastLuck = LuckNow;
	DisplayedLuck = FMath::FInterpTo(DisplayedLuck, static_cast<float>(LuckNow), InDeltaTime, 5.f);
	LuckPop = FMath::Max(LuckPop - InDeltaTime * 2.f, 0.f);
	const float Luck01 = DisplayedLuck / UPuzzleManager::MaxLuck;
	LuckText->SetText(FText::FromString(FString::Printf(TEXT("LUCK %d"), FMath::RoundToInt(DisplayedLuck))));
	LuckFillBox->SetWidthOverride(FMath::Max(140.f * Luck01, 16.f));
	LuckFill->SetRenderOpacity(LuckNow > 0 ? 1.f : 0.f);
	SyncAspect(LuckFill);
	// Ember red when it's gone, gold in the middle, moonlit green when fortune favours you.
	const FLinearColor LuckColor = Luck01 < 0.5f
		? FMath::Lerp(FLinearColor(1.f, 0.25f, 0.12f), Gold, Luck01 * 2.f)
		: FMath::Lerp(Gold, LuckGreen, (Luck01 - 0.5f) * 2.f);
	if (UMaterialInstanceDynamic* MID = LuckFill->GetDynamicMaterial())
	{
		MID->SetVectorParameterValue(TEXT("Fill"), LuckColor);
		MID->SetVectorParameterValue(TEXT("Fill2"), LuckColor * 0.3f);
	}
	if (UMaterialInstanceDynamic* MID = LuckIcon->GetDynamicMaterial())
	{
		MID->SetVectorParameterValue(TEXT("Color"), LuckColor);
		MID->SetScalarParameterValue(TEXT("Glow"), 0.3f + 0.9f * Luck01 + 0.8f * LuckPop);
	}
	if (UMaterialInstanceDynamic* MID = LuckBg->GetDynamicMaterial())
	{
		MID->SetScalarParameterValue(TEXT("Glow"), 0.2f + 0.8f * LuckPop + (Luck01 >= 0.6f ? 0.25f + 0.15f * FMath::Sin(Time * 5.f) : 0.f));
	}
	LuckBadge->SetRenderScale(FVector2D(1.f + 0.22f * LuckPop * LuckPop));

	// Hint line.
	FString Hint;
	FLinearColor HintColor = Lavender;
	if (GameMode->bAutoPlayEnabled)
	{
#if PLATFORM_IOS || PLATFORM_ANDROID
		Hint = TEXT("DEMO - the computer is playing");
#else
		Hint = TEXT("DEMO - the computer is playing\n[P] menu");
#endif
		HintColor = Cyan;
	}
	else if (bTargeting)
	{
		Hint = TEXT("Tap the board to call down Holy Light");
		HintColor = PaleGold;
	}
	else if (bPacketWaiting)
	{
		Hint = TEXT("Someone is knocking: open the packet");
		HintColor = FLinearColor(1.f, 0.6f, 0.2f);
	}
	else if (GameMode->Flow == EPuzzleFlow::Playing && Rules->IsStuck())
	{
		Hint = Rules->bRelicsEnabled ? TEXT("No room! Use a relic") : TEXT("No room left");
		HintColor = FLinearColor(1.f, 0.3f, 0.25f, 0.75f + 0.25f * FMath::Sin(Time * 8.f));
	}
	else if (GameMode->Flow == EPuzzleFlow::Playing)
	{
		Hint = (GameMode->InputHandler && GameMode->InputHandler->IsHoveringReserve()) ? TEXT("Drop to hold it for later") : TEXT("Drag a piece onto the board");
	}
	HintText->SetText(FText::FromString(Hint));
	HintText->SetColorAndOpacity(FSlateColor(HintColor));

	// HOLD label under the reserve slot.
	APlayerController* PC = GetOwningPlayer();
	FVector2D HoldPosition;
	if (GameMode->GridManager && PC && bPlayingView
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, GameMode->GridManager->GetTrayAnchorWorldLocation(AGridManager::ReserveSlot) + FVector(0.f, 100.f, 0.f), HoldPosition, true))
	{
		HoldLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* HoldSlot = Cast<UCanvasPanelSlot>(HoldLabel->Slot))
		{
			HoldSlot->SetPosition(HoldPosition);
		}
		const bool bHover = GameMode->InputHandler && GameMode->InputHandler->IsHoveringReserve();
		HoldLabel->SetRenderScale(FVector2D(bHover ? 1.25f + 0.05f * FMath::Sin(Time * 10.f) : 1.f));
		HoldLabel->SetColorAndOpacity(FSlateColor(bHover ? PaleGold : FLinearColor(0.75f, 0.55f, 1.f)));
	}
	else
	{
		HoldLabel->SetVisibility(ESlateVisibility::Collapsed);
	}

	TickPopups(InDeltaTime);

	// Card entrance: dim fades in, the card springs up; result stars land one by one.
	if (CurrentCard != EPuzzleCard::None)
	{
		CardAge += InDeltaTime;
		CardDim->SetRenderOpacity(FMath::Min(CardAge / 0.25f, 1.f));
		const float Spring = BackOut(CardAge / 0.4f);
		CardPanel->SetRenderScale(FVector2D(0.7f + 0.3f * Spring));
		CardPanel->SetRenderOpacity(FMath::Min(CardAge / 0.15f, 1.f));
		for (int32 Index = 0; Index < CardStars.Num(); ++Index)
		{
			const float Local = (CardAge - 0.45f - 0.28f * Index) / 0.35f;
			CardStars[Index]->SetRenderScale(FVector2D(Local <= 0.f ? 0.f : BackOut(Local) * (1.f + 0.04f * FMath::Sin(Time * 5.f + Index))));
		}
	}
}
