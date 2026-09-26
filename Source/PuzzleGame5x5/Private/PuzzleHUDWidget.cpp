#include "PuzzleHUDWidget.h"
#include "PuzzleGameMode.h"
#include "PuzzleManager.h"
#include "PuzzleInputHandler.h"
#include "PuzzleSaveGame.h"
#include "QuestCatalog.h"
#include "GridManager.h"
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
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
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

	// Icon shapes in M_UIIcon.
	constexpr float IconMoon = 2.f;
	constexpr float IconGargoyle = 5.f;
	constexpr float IconSun = 6.f;
	constexpr float IconReroll = 7.f;
	constexpr float IconBox = 8.f;
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

UWidget* UPuzzleHUDWidget::StarRow(int32 Stars, float Size, float Gap)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bEarned = Index < Stars;
		UImage* Star = MakeIcon(UIStyle::IconStar, bEarned ? UIStyle::Gold : FLinearColor(0.3f, 0.22f, 0.35f), Size, bEarned ? 1.f : 0.f, bEarned ? 0.4f : 0.f);
		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(Star))
		{
			RowSlot->SetPadding(FMargin(Gap, 0.f));
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	return Row;
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

	// ---- Top bar: MOVES | SCORE | GOAL ----
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
	AddColumn(MakeText(TEXT("MOVES"), true, 22.f, Lavender, 3.f), MovesText, 1.f);

	ScoreText = MakeText(TEXT("0"), false, 74.f, Gold, 5.f);
	AddColumn(MakeText(TEXT("SCORE"), true, 24.f, PaleGold, 3.f), ScoreText, 1.35f);

	UHorizontalBox* GoalRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	GoalIcon = MakeIcon(IconStar, Gold, 62.f);
	GoalRow->AddChildToHorizontalBox(GoalIcon)->SetVerticalAlignment(VAlign_Center);
	GoalText = MakeText(TEXT("0"), false, 52.f, FLinearColor::White, 5.f);
	if (UHorizontalBoxSlot* GoalSlot = GoalRow->AddChildToHorizontalBox(GoalText))
	{
		GoalSlot->SetVerticalAlignment(VAlign_Center);
		GoalSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	}
	GoalLabel = MakeText(TEXT("GOAL"), true, 22.f, Lavender, 3.f);
	AddColumn(GoalLabel, GoalRow, 1.f);

	if (UOverlaySlot* ColumnsSlot = TopBar->AddChildToOverlay(Columns))
	{
		ColumnsSlot->SetHorizontalAlignment(HAlign_Fill);
		ColumnsSlot->SetVerticalAlignment(VAlign_Center);
		ColumnsSlot->SetPadding(FMargin(30.f, 0.f, 30.f, 8.f));
	}

	UCanvasPanelSlot* TopSlot = RootCanvas->AddChildToCanvas(TopBar);
	TopSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
	TopSlot->SetOffsets(FMargin(22.f, 36.f, 22.f, 210.f));

	// ---- Combo meter: "COMBO x5" and three pips = placements left to keep it alive ----
	UOverlay* Combo = WidgetTree->ConstructWidget<UOverlay>();
	Combo->SetVisibility(ESlateVisibility::HitTestInvisible);
	ComboBg = MakePanel(FLinearColor(0.4f, 0.03f, 0.3f), FLinearColor(0.1f, 0.005f, 0.09f), Rim, 480.f / 124.f, 0.45f, 0.3f);
	if (UOverlaySlot* BgSlot = Combo->AddChildToOverlay(ComboBg))
	{
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UHorizontalBox* ComboRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	ComboText = MakeText(TEXT("COMBO x1"), false, 46.f, FLinearColor(1.f, 0.75f, 0.95f), 4.f);
	ComboRow->AddChildToHorizontalBox(ComboText)->SetVerticalAlignment(VAlign_Center);
	for (int32 Pip = 0; Pip < UPuzzleManager::ComboWindowMoves; ++Pip)
	{
		UImage* PipImage = MakeIcon(IconStar, PaleGold, 42.f, 1.f, 0.4f);
		UHorizontalBoxSlot* PipSlot = ComboRow->AddChildToHorizontalBox(PipImage);
		PipSlot->SetVerticalAlignment(VAlign_Center);
		PipSlot->SetPadding(FMargin(Pip == 0 ? 16.f : 2.f, 0.f, 0.f, 0.f));
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
	ComboSlot->SetAnchors(FAnchors(0.5f, 0.f));
	ComboSlot->SetAlignment(FVector2D(0.5f, 0.f));
	ComboSlot->SetPosition(FVector2D(0.f, 250.f));
	ComboSlot->SetSize(FVector2D(480.f, 124.f));

	// ---- Menu pill (top-left, under the bar) ----
	UButton* Menu = MakeTextButton(TEXT("MENU"), FVector2D(170.f, 84.f), Amethyst, Amethyst2, ActMenu);
	MenuButton = Menu;
	UCanvasPanelSlot* MenuSlot = RootCanvas->AddChildToCanvas(Menu);
	MenuSlot->SetAnchors(FAnchors(0.f, 0.f));
	MenuSlot->SetPosition(FVector2D(34.f, 256.f));
	MenuSlot->SetSize(FVector2D(170.f, 84.f));

	// ---- Luck (top-right, under the bar): the moon's favour. Combos raise it, relics spend it,
	// and it is the chance of warding off the next storm or hex. ----
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

	// ---- Hint line (bottom centre, between the relics) ----
	HintText = MakeText(TEXT(""), false, 30.f, Lavender, 3.f);
	HintText->SetAutoWrapText(true);
	HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(HintText);
	HintSlot->SetAnchors(FAnchors(0.5f, 1.f));
	HintSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	HintSlot->SetPosition(FVector2D(0.f, -140.f));
	HintSlot->SetSize(FVector2D(620.f, 110.f));

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
		ContentSlot->SetPadding(FMargin(70.f, 90.f, 70.f, 100.f));
	}
	CardPanel = Sized(Card, FVector2D(940.f, 0.f));
	CardPanel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UOverlaySlot* CardSlot = Cards->AddChildToOverlay(CardPanel))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);
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
	if (Card == EPuzzleCard::None)
	{
		CardLayer->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	BuildCardContent(Card);
	CardLayer->SetVisibility(ESlateVisibility::Visible);
}

void UPuzzleHUDWidget::BuildCardContent(EPuzzleCard Card)
{
	using namespace UIStyle;

	CardContent->ClearChildren();
	CardStars.Reset();

	const APuzzleGameMode* GameMode = GetGameMode();
	const UPuzzleManager* Rules = GameMode ? GameMode->PuzzleManager.Get() : nullptr;
	const UPuzzleSaveGame* Save = GameMode ? GameMode->SaveGame.Get() : nullptr;
	const FQuestLevel& Level = QuestCatalog::Get(GameMode ? GameMode->CurrentLevel : 1);

	auto Add = [this](UWidget* Widget, float Top = 0.f, EHorizontalAlignment Align = HAlign_Center)
	{
		UVerticalBoxSlot* ContentSlot = CardContent->AddChildToVerticalBox(Widget);
		ContentSlot->SetHorizontalAlignment(Align);
		ContentSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		return ContentSlot;
	};
	auto ButtonRow = [this](UWidget* Left, UWidget* Right)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(Left)->SetPadding(FMargin(14.f, 0.f));
		Row->AddChildToHorizontalBox(Right)->SetPadding(FMargin(14.f, 0.f));
		return Row;
	};
	auto GoalIconFor = [this](const FQuestLevel& Quest, float Size) -> UImage*
	{
		switch (Quest.Goal)
		{
		case EQuestGoal::CollectSymbol: return MakeIcon(static_cast<float>(Quest.Color), PuzzleTypes::ToLinearColor(Quest.Color), Size, 1.f, 0.3f);
		case EQuestGoal::ClearLines:    return MakeIcon(IconLine, Cyan, Size);
		case EQuestGoal::ClearBoxes:    return MakeIcon(IconBox, FLinearColor(0.8f, 0.4f, 1.f), Size);
		case EQuestGoal::BreakStones:   return MakeIcon(IconGargoyle, FLinearColor(0.5f, 0.5f, 0.55f), Size);
		default:                        return MakeIcon(IconStar, Gold, Size);
		}
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

		Add(Sized(MakeTextButton(TEXT("ENDLESS"), FVector2D(560.f, 136.f), Emerald, Emerald2, ActEndless), FVector2D(560.f, 136.f)), 44.f);
		const int32 Best = Save ? Save->BestEndlessScore : 0;
		Add(MakeText(Best > 0 ? FString::Printf(TEXT("best  %d"), Best) : TEXT("no best yet"), false, 30.f, PaleGold, 3.f), 8.f);
		Add(Sized(MakeTextButton(TEXT("HOW TO PLAY"), FVector2D(460.f, 100.f), Amethyst, Amethyst2, ActHowTo), FVector2D(460.f, 100.f)), 22.f);

		Add(MakeText(TEXT("Quests"), true, 50.f, PaleGold, 5.f), 40.f);
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
		Grid->SetSlotPadding(FMargin(7.f));
		const int32 Unlocked = Save ? Save->HighestUnlockedLevel : 1;
		const TArray<FQuestLevel>& Levels = QuestCatalog::Levels();
		for (int32 Index = 0; Index < Levels.Num(); ++Index)
		{
			const int32 Number = Index + 1;
			const bool bOpen = Number <= Unlocked;
			UVerticalBox* Face = WidgetTree->ConstructWidget<UVerticalBox>();
			Face->AddChildToVerticalBox(MakeText(FString::FromInt(Number), false, 46.f, bOpen ? FLinearColor::White : FLinearColor(0.2f, 0.2f, 0.25f), 4.f))->SetHorizontalAlignment(HAlign_Center);
			Face->AddChildToVerticalBox(StarRow(Save ? Save->GetStars(Number) : 0, 30.f, 1.f))->SetHorizontalAlignment(HAlign_Center);
			UButton* Button = MakeButton(Face, FVector2D(134.f, 134.f), Amethyst, Amethyst2, ActLevel, Number);
			Button->SetIsEnabled(bOpen);
			Grid->AddChildToUniformGrid(Sized(Button, FVector2D(134.f, 134.f)), Index / 5, Index % 5);
		}
		Add(Grid, 10.f);
		break;
	}
	case EPuzzleCard::LevelIntro:
	{
		Add(MakeText(FString::Printf(TEXT("Level %d"), Level.Number), true, 84.f, Gold, 6.f));
		Add(GoalIconFor(Level, 190.f), 26.f);
		Add(MakeText(QuestCatalog::Describe(Level), false, 60.f, FLinearColor::White, 5.f), 14.f);
		Add(MakeText(FString::Printf(TEXT("in %d moves"), Level.Moves), false, 38.f, Cyan, 3.f), 4.f);
		if (Level.StoneInterval > 0)
		{
			UHorizontalBox* Warning = WidgetTree->ConstructWidget<UHorizontalBox>();
			Warning->AddChildToHorizontalBox(MakeIcon(IconGargoyle, FLinearColor(0.5f, 0.5f, 0.55f), 64.f))->SetVerticalAlignment(VAlign_Center);
			UHorizontalBoxSlot* WarningText = Warning->AddChildToHorizontalBox(MakeText(FString::Printf(TEXT("a gargoyle lands every %d moves"), Level.StoneInterval), false, 30.f, FLinearColor(0.75f, 0.6f, 1.f), 3.f));
			WarningText->SetVerticalAlignment(VAlign_Center);
			WarningText->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
			Add(Warning, 26.f);
		}
		if (Level.Number >= 5)
		{
			UHorizontalBox* Storm = WidgetTree->ConstructWidget<UHorizontalBox>();
			Storm->AddChildToHorizontalBox(MakeIcon(IconMoon, LuckGreen, 64.f, 1.f, 0.4f))->SetVerticalAlignment(VAlign_Center);
			UHorizontalBoxSlot* StormText = Storm->AddChildToHorizontalBox(MakeText(TEXT("storms and hexes roam here:\ncombos build LUCK to ward them off"), false, 30.f, FLinearColor(0.7f, 1.f, 0.75f), 3.f));
			StormText->SetVerticalAlignment(VAlign_Center);
			StormText->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
			Add(Storm, 18.f);
		}
		UTextBlock* Tip = MakeText(TEXT("Same-symbol lines are BLESSED: double points and +2 moves.\nThe glowing box scores x3."), false, 28.f, Lavender, 3.f);
		Tip->SetAutoWrapText(true);
		Add(Tip, 26.f, HAlign_Fill);
		Add(Sized(MakeTextButton(TEXT("BEGIN"), FVector2D(520.f, 136.f), Emerald, Emerald2, ActBegin), FVector2D(520.f, 136.f)), 44.f);
		break;
	}
	case EPuzzleCard::LevelComplete:
	{
		Add(MakeText(TEXT("Level"), true, 64.f, Gold, 5.f));
		Add(MakeText(TEXT("Complete!"), true, 76.f, Gold, 6.f), -16.f);
		UHorizontalBox* Stars = WidgetTree->ConstructWidget<UHorizontalBox>();
		const int32 Earned = GameMode ? GameMode->LastStars : 0;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const bool bEarned = Index < Earned;
			UImage* Star = MakeIcon(IconStar, bEarned ? Gold : FLinearColor(0.3f, 0.22f, 0.35f), Index == 1 ? 200.f : 160.f, bEarned ? 1.f : 0.f, bEarned ? 0.8f : 0.f);
			Star->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			UHorizontalBoxSlot* StarSlot = Stars->AddChildToHorizontalBox(Star);
			StarSlot->SetVerticalAlignment(Index == 1 ? VAlign_Top : VAlign_Bottom);
			StarSlot->SetPadding(FMargin(6.f, 0.f));
			CardStars.Add(Star);
		}
		Add(Stars, 24.f);
		Add(MakeText(FString::Printf(TEXT("score  %d"), Rules ? Rules->Score : 0), false, 50.f, FLinearColor::White, 4.f), 20.f);
		Add(MakeText(FString::Printf(TEXT("%d moves to spare"), Rules ? Rules->MovesLeft : 0), false, 32.f, Cyan, 3.f), 4.f);
		const bool bLast = Level.Number >= QuestCatalog::Levels().Num();
		Add(ButtonRow(Sized(MakeTextButton(TEXT("MENU"), FVector2D(300.f, 124.f), Amethyst, Amethyst2, ActMenu), FVector2D(300.f, 124.f)),
			Sized(MakeTextButton(bLast ? TEXT("RETRY") : TEXT("NEXT"), FVector2D(380.f, 124.f), Emerald, Emerald2, bLast ? ActRetry : ActNext), FVector2D(380.f, 124.f))), 44.f);
		break;
	}
	case EPuzzleCard::LevelFailed:
	case EPuzzleCard::EndlessOver:
	{
		const bool bOutOfMoves = Rules && Rules->IsOutOfMoves();
		const bool bEndless = Card == EPuzzleCard::EndlessOver;
		Add(MakeText(bEndless ? TEXT("Game Over") : (bOutOfMoves ? TEXT("Out of Moves") : TEXT("No Room Left")), true, 70.f, FLinearColor(1.f, 0.25f, 0.3f), 6.f));
		if (bEndless)
		{
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
				Add(MakeText(FString::Printf(TEXT("best  %d"), Save ? Save->BestEndlessScore : 0), false, 38.f, PaleGold, 3.f), 4.f);
			}
			Add(MakeText(bOutOfMoves ? TEXT("out of moves") : TEXT("no room left"), false, 32.f, Lavender, 3.f), 12.f);
		}
		else
		{
			UHorizontalBox* Progress = WidgetTree->ConstructWidget<UHorizontalBox>();
			Progress->AddChildToHorizontalBox(GoalIconFor(Level, 110.f))->SetVerticalAlignment(VAlign_Center);
			UHorizontalBoxSlot* ProgressText = Progress->AddChildToHorizontalBox(MakeText(FString::Printf(TEXT("%d / %d"), Rules ? FMath::Min(Rules->GetQuestProgress(), Level.Target) : 0, Level.Target), false, 70.f, FLinearColor::White, 5.f));
			ProgressText->SetVerticalAlignment(VAlign_Center);
			ProgressText->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
			Add(Progress, 30.f);
			Add(MakeText(QuestCatalog::Describe(Level), false, 34.f, Lavender, 3.f), 8.f);
		}
		Add(ButtonRow(Sized(MakeTextButton(TEXT("MENU"), FVector2D(300.f, 124.f), Amethyst, Amethyst2, ActMenu), FVector2D(300.f, 124.f)),
			Sized(MakeTextButton(bEndless ? TEXT("PLAY AGAIN") : TEXT("RETRY"), FVector2D(420.f, 124.f), Emerald, Emerald2, ActRetry), FVector2D(420.f, 124.f))), 50.f);
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
	case ActEndless: GameMode->StartEndless(); break;
	case ActLevel:   GameMode->SelectLevel(Param); break;
	case ActBegin:   GameMode->BeginLevel(); break;
	case ActNext:    GameMode->NextLevel(); break;
	case ActRetry:   GameMode->Retry(); break;
	case ActMenu:    GameMode->ShowMenu(); break;
	case ActHoly:    GameMode->RequestRelic(ERelic::HolyLight); break;
	case ActReroll:  GameMode->RequestRelic(ERelic::Reroll); break;
	case ActDemo:    GameMode->ToggleAutoPlay(); break;
	case ActHowTo:   ShowTutorial(); break;
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
	const auto Sigil = [](EPuzzleTileColor Color) { return PuzzleTypes::ToLinearColor(Color); };
	const FLinearColor Stone(0.55f, 0.55f, 0.6f);
	const TArray<FPage> Pages = {
		{ TEXT("The Rite"),
		  { { IconLine, Cyan, TEXT("ROW / COLUMN") }, { IconBox, FLinearColor(0.8f, 0.4f, 1.f), TEXT("3x3 BOX") } },
		  TEXT("Drag a piece from the tray onto the board. Fill a whole ROW, COLUMN or 3x3 BOX and it breaks apart.\n\n")
		  TEXT("Every piece costs a move; clears win moves back. The rite ends when the moves run out, or when no piece can fit.") },
		{ TEXT("Combos"),
		  { { IconStar, PaleGold, TEXT("COMBO") } },
		  TEXT("Clear lines on following moves to build a COMBO. Several lines at once climb it faster.\n\n")
		  TEXT("The three stars are its lifeline: each move without a clear burns one. Every 3 combo steps grants a RELIC.") },
		{ TEXT("The Five Sigils"),
		  { { 0.f, Sigil(EPuzzleTileColor::Red), TEXT("BLOOD") }, { 1.f, Sigil(EPuzzleTileColor::Green), TEXT("SKULL") },
		    { 2.f, Sigil(EPuzzleTileColor::Blue), TEXT("MOON") }, { 3.f, Sigil(EPuzzleTileColor::Yellow), TEXT("CROSS") },
		    { 4.f, Sigil(EPuzzleTileColor::Purple), TEXT("BAT") } },
		  TEXT("Complete a line of ONE sigil and it is BLESSED: bonus points and +2 moves.\n\n")
		  TEXT("The 3x3 box inside the golden rune circle is HOLY: clearing it scores x3.") },
		{ TEXT("Relics"),
		  { { IconSun, Gold, TEXT("HOLY LIGHT") }, { IconReroll, Cyan, TEXT("REROLL") } },
		  TEXT("HOLY LIGHT: tap the board to purge a 3x3 area. It even shatters stone.\nREROLL: summon a fresh tray of pieces.\n\n")
		  TEXT("The fourth tray slot is HOLD: park a piece there for later. It costs no move.") },
		{ TEXT("Curses"),
		  { { IconGargoyle, Stone, TEXT("GARGOYLE") }, { IconStar, FLinearColor(0.55f, 0.75f, 1.f), TEXT("LIGHTNING") },
		    { 4.f, FLinearColor(0.7f, 0.25f, 1.f), TEXT("HEX") } },
		  TEXT("GARGOYLES fall onto the board: the first clear only cracks one, the second shatters it.\n\n")
		  TEXT("LIGHTNING turns a tile to stone. A HEX steals 2 moves.") },
		{ TEXT("Luck"),
		  { { IconMoon, LuckGreen, TEXT("LUCK") } },
		  TEXT("Every combo step gathers +6 LUCK. Relics spend it: Holy Light -20, Reroll -12.\n\n")
		  TEXT("When lightning or a hex comes, your luck is the chance to WARD it off (up to 90%). A ward burns 10 luck.\n\n")
		  TEXT("The candles burn with your luck. Let it die, and the things in the dark wake.") },
	};
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

	UTextBlock* Body = MakeText(Page.Body, false, 34.f, FLinearColor(0.9f, 0.86f, 1.f), 3.f);
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

	const FVector2D ButtonSize(360.f, 120.f);
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
	else
	{
		static const TCHAR* Words[] = { TEXT("Nice!"), TEXT("Great!"), TEXT("Amazing!"), TEXT("Magical!!"), TEXT("LEGENDARY!!") };
		static const FLinearColor Colors[] = { FLinearColor(0.3f, 0.9f, 1.f), FLinearColor(0.35f, 1.f, 0.25f), FLinearColor(1.f, 0.4f, 1.f), Gold, FLinearColor(1.f, 0.3f, 0.2f) };
		const int32 Tier = FMath::Clamp(Result.Lines - 1, 0, 4);
		AddPopup(MakeText(Words[Tier], false, 96.f + 12.f * Tier, Colors[Tier], 7.f), FVector2D(0.5f, 0.2f), 1.4f, 1.f);
	}

	float Stagger = 0.18f;
	if (Result.Blessings > 0)
	{
		const FString Text = Result.Blessings > 1 ? FString::Printf(TEXT("BLESSED x%d!"), Result.Blessings) : TEXT("BLESSED!");
		AddPopup(MakeText(Text, true, 60.f, PaleGold, 5.f), FVector2D(0.5f, 0.2f + 0.045f), 1.5f, 1.f, Stagger);
		Stagger += 0.18f;
	}
	if (Result.bBlessedBox)
	{
		AddPopup(MakeText(TEXT("HOLY BOX  x3"), true, 56.f, FLinearColor(1.f, 0.85f, 0.45f), 5.f), FVector2D(0.5f, 0.2f + 0.09f), 1.5f, 1.f, Stagger);
		Stagger += 0.18f;
	}
	if (Result.StonesBroken > 0)
	{
		AddPopup(MakeText(TEXT("Shattered!"), false, 58.f, FLinearColor(0.75f, 0.6f, 1.f), 5.f), FVector2D(0.5f, 0.2f + 0.135f), 1.3f, 1.f, Stagger);
	}
	else if (Result.StonesCracked > 0)
	{
		AddPopup(MakeText(TEXT("Cracked!"), false, 48.f, FLinearColor(0.6f, 0.55f, 0.7f), 4.f), FVector2D(0.5f, 0.2f + 0.135f), 1.2f, 1.f, Stagger);
	}

	AddWorldPopup(MakeText(FString::Printf(TEXT("+%d"), Event.Points), false, 70.f, FLinearColor::White, 5.f), Event.Centroid, 1.2f);

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
			FVector2D(0.5f, (250.f + 62.f) / FMath::Max(CanvasSize.Y, 1.f)), 1.2f, 1.f, 0.f, -50.f);
	}
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

void UPuzzleHUDWidget::ShowStoneLanded(const FVector& WorldLocation)
{
	AddWorldPopup(MakeText(TEXT("Gargoyle!"), true, 44.f, FLinearColor(0.7f, 0.45f, 1.f), 4.f), WorldLocation + FVector(0.f, 0.f, 40.f), 1.3f);
}

void UPuzzleHUDWidget::ShowOmen(EOmen Omen, bool bWarded, const FVector& WorldLocation)
{
	using namespace UIStyle;
	const bool bLightning = Omen == EOmen::Lightning;
	if (bWarded)
	{
		// The hex's ward lands a beat later (see AGridManager::PlayHex).
		const float WardDelay = bLightning ? 0.f : 0.55f;
		AddPopup(MakeText(TEXT("WARDED!"), true, 76.f, PaleGold, 6.f), FVector2D(0.5f, 0.24f), 1.6f, 1.f, WardDelay);
		AddPopup(MakeText(bLightning ? TEXT("luck turned the lightning aside") : TEXT("luck broke the hex"), false, 36.f, FLinearColor(0.95f, 0.88f, 0.6f), 3.f),
			FVector2D(0.5f, 0.28f), 1.8f, 1.f, WardDelay + 0.2f, 30.f);
		LuckPop = 1.f;
		return;
	}
	if (bLightning)
	{
		AddPopup(MakeText(TEXT("LIGHTNING!"), true, 72.f, FLinearColor(0.6f, 0.8f, 1.f), 6.f), FVector2D(0.5f, 0.24f), 1.4f);
		AddWorldPopup(MakeText(TEXT("Petrified!"), false, 44.f, FLinearColor(0.75f, 0.8f, 0.9f), 4.f), WorldLocation + FVector(0.f, 0.f, 40.f), 1.3f, 1.f, 0.15f);
	}
	else
	{
		AddPopup(MakeText(TEXT("HEXED!"), true, 76.f, FLinearColor(0.75f, 0.3f, 1.f), 6.f), FVector2D(0.5f, 0.24f), 1.6f);
		const float BarBottom = 246.f / FMath::Max(CanvasSize.Y, 1.f);
		AddPopup(MakeText(FString::Printf(TEXT("-%d MOVES"), UPuzzleManager::HexMoveCost), false, 40.f, Danger, 4.f), FVector2D(0.19f, BarBottom), 1.6f, 1.f, 0.2f, -40.f);
		MovesPop = 1.f;
	}
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
	ScoreText->SetText(FText::AsNumber(FMath::RoundToInt(DisplayedScore)));

	// Moves: red and throbbing when low, a bounce when moves are refunded.
	const int32 Moves = Rules->MovesLeft;
	const bool bLow = Moves <= 5;
	MovesText->SetText(FText::AsNumber(Moves));
	MovesText->SetColorAndOpacity(FSlateColor(bLow ? Danger : Cyan));
	MovesPop = FMath::Max(MovesPop - InDeltaTime * 2.5f, 0.f);
	const float MovesScale = (bLow ? 1.f + 0.08f * FMath::Sin(Time * 10.f) : 1.f) + 0.35f * MovesPop * MovesPop;
	MovesText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	MovesText->SetRenderScale(FVector2D(MovesScale));

	// Goal (quests) or best score (endless).
	if (Rules->Mode == EPlayMode::Quest)
	{
		const FQuestLevel& Quest = Rules->Quest;
		GoalLabel->SetText(FText::FromString(FString::Printf(TEXT("LEVEL %d"), Quest.Number)));
		GoalText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::Min(Rules->GetQuestProgress(), Quest.Target), Quest.Target)));
		float Shape = IconStar;
		FLinearColor Color = Gold;
		switch (Quest.Goal)
		{
		case EQuestGoal::CollectSymbol: Shape = static_cast<float>(Quest.Color); Color = PuzzleTypes::ToLinearColor(Quest.Color); break;
		case EQuestGoal::ClearLines:    Shape = IconLine; Color = Cyan; break;
		case EQuestGoal::ClearBoxes:    Shape = IconBox; Color = FLinearColor(0.8f, 0.4f, 1.f); break;
		case EQuestGoal::BreakStones:   Shape = IconGargoyle; Color = FLinearColor(0.5f, 0.5f, 0.55f); break;
		default: break;
		}
		if (UMaterialInstanceDynamic* MID = GoalIcon->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(TEXT("Shape"), Shape);
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
		GoalIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		const int32 Best = GameMode->SaveGame ? FMath::Max(GameMode->SaveGame->BestEndlessScore, Rules->Score) : Rules->Score;
		GoalLabel->SetText(FText::FromString(TEXT("BEST")));
		GoalText->SetText(FText::AsNumber(Best));
		GoalIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Combo meter: pops when it grows, trembles on its last pip, hidden at zero.
	const int32 Combo = Rules->ComboStreak;
	if (Combo > LastCombo)
	{
		ComboPop = 1.f;
	}
	LastCombo = Combo;
	ComboPop = FMath::Max(ComboPop - InDeltaTime * 2.2f, 0.f);
	const bool bShowCombo = Combo > 0 && bPlayingView;
	ComboBadge->SetVisibility(bShowCombo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShowCombo)
	{
		ComboText->SetText(FText::FromString(FString::Printf(TEXT("COMBO x%d"), Combo)));
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
		RelicBadges[RelicIndex]->SetVisibility(bPlayingView ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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

	// Luck: eased number and bar, pops with a floating +/- whenever it changes.
	const int32 LuckNow = Rules->GetLuck();
	LuckBadge->SetVisibility(bPlayingView ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
		Hint = TEXT("DEMO - the computer is playing\n[P] take over");
#endif
		HintColor = Cyan;
	}
	else if (bTargeting)
	{
		Hint = TEXT("Tap the board to call down Holy Light");
		HintColor = PaleGold;
	}
	else if (GameMode->Flow == EPuzzleFlow::Playing && Rules->IsStuck())
	{
		Hint = TEXT("No room! Use a relic");
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
