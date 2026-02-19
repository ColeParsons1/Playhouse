// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/PHCCityHUD.h"
#include "UI/PHCMainHUD.h"
#include "Core/PHCGameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "Core/PHCPlayerController.h"
#include "Player/PHCCityCharacter.h"
#include "Vehicle/PHCVehiclePawn.h"
#include "Vehicle/PHCDuneBuggyPawn.h"
#include "Vehicle/PHCRaptorPawn.h"
#include "Vehicle/PHCTankPawn.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "PlayhouseCity.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"
#include "Core/PHCCitySubsystem.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCCityHUD::UPHCCityHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MainHUDClass = UPHCMainHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// Widget tree construction
// ---------------------------------------------------------------------------

bool UPHCCityHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// Only build programmatically if not already wired from a UMG Blueprint.
	if (!CoinText)
	{
		const FLinearColor BarBg(0.f, 0.f, 0.f, 0.5f);
		const FLinearColor White(FLinearColor::White);
		const FSlateFontInfo CurrencyFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
		const FSlateFontInfo BtnFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

		// ---- Root canvas (full screen) ----
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("CityHUDRoot"));
		WidgetTree->RootWidget = RootCanvas;

		// ============================================================
		//  TOP-RIGHT: Currency display  (semi-transparent bar)
		// ============================================================
		UBorder* CurrencyBar = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("CurrencyBar"));
		CurrencyBar->SetBrushColor(BarBg);
		CurrencyBar->SetPadding(FMargin(12.f, 6.f, 12.f, 6.f));

		UCanvasPanelSlot* CurrencySlot = RootCanvas->AddChildToCanvas(CurrencyBar);
		CurrencySlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f)); // top-right
		CurrencySlot->SetAlignment(FVector2D(1.f, 0.f));
		CurrencySlot->SetOffsets(FMargin(-80.f, 16.f, 0.f, 0.f));
		CurrencySlot->SetAutoSize(true);

		UHorizontalBox* CurrencyHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("CurrencyHBox"));
		CurrencyBar->SetContent(CurrencyHBox);

		// Coins
		CoinText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CoinText"));
		CoinText->SetText(FText::FromString(TEXT("Coins: 0")));
		CoinText->SetFont(CurrencyFont);
		CoinText->SetColorAndOpacity(FSlateColor(White));

		UHorizontalBoxSlot* CoinSlot = CurrencyHBox->AddChildToHorizontalBox(CoinText);
		CoinSlot->SetVerticalAlignment(VAlign_Center);
		CoinSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

		// Gems
		GemText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("GemText"));
		GemText->SetText(FText::FromString(TEXT("Gems: 0")));
		GemText->SetFont(CurrencyFont);
		GemText->SetColorAndOpacity(FSlateColor(White));

		UHorizontalBoxSlot* GemSlot = CurrencyHBox->AddChildToHorizontalBox(GemText);
		GemSlot->SetVerticalAlignment(VAlign_Center);

		// ============================================================
		//  TOP-CENTER: Live location monitor (X, Y, Z coordinates)
		// ============================================================
		{
			const FSlateFontInfo LocFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 12);

			UBorder* LocBar = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("LocationBar"));
			LocBar->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
			LocBar->SetPadding(FMargin(10.f, 4.f, 10.f, 4.f));

			UCanvasPanelSlot* LocSlot = RootCanvas->AddChildToCanvas(LocBar);
			LocSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f)); // top-center
			LocSlot->SetAlignment(FVector2D(0.5f, 0.f));
			LocSlot->SetOffsets(FMargin(0.f, 10.f, 0.f, 0.f));
			LocSlot->SetAutoSize(true);

			LocationText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("LocationText"));
			LocationText->SetText(FText::FromString(TEXT("X: 0  Y: 0  Z: 0")));
			LocationText->SetFont(LocFont);
			LocationText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 1.f, 0.8f)));
			LocBar->SetContent(LocationText);
		}

		// ============================================================
		//  TOP-RIGHT (below currency): "Fire" button -- missile launcher
		// ============================================================
		FireButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("FireButton"));

		// Large circular red button, positioned just above the gas pedal.
		const float FireBtnSize = 168.f;
		const float CornerR = FireBtnSize * 0.5f; // half size = circle

		auto MakeRoundBrush = [&](const FLinearColor& Color) -> FSlateBrush
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings.CornerRadii = FVector4(CornerR, CornerR, CornerR, CornerR);
			Brush.OutlineSettings.Color = FSlateColor(FLinearColor::Transparent);
			Brush.OutlineSettings.Width = 0.f;
			return Brush;
		};

		FButtonStyle FireBtnStyle = FireButton->GetStyle();
		FireBtnStyle.Normal  = MakeRoundBrush(FLinearColor(0.7f, 0.1f, 0.1f, 0.85f));
		FireBtnStyle.Hovered = MakeRoundBrush(FLinearColor(0.85f, 0.15f, 0.15f, 0.95f));
		FireBtnStyle.Pressed = MakeRoundBrush(FLinearColor(0.5f, 0.08f, 0.08f, 1.0f));
		FireButton->SetStyle(FireBtnStyle);
		FireButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
		FireButton->SetClickMethod(EButtonClickMethod::MouseDown);
		FireButton->SetPressMethod(EButtonPressMethod::ButtonPress);

		UTextBlock* FireLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("FireLabel"));
		const FSlateFontInfo FireFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);
		FireLabel->SetText(FText::FromString(TEXT("FIRE")));
		FireLabel->SetFont(FireFont);
		FireLabel->SetColorAndOpacity(FSlateColor(White));
		FireLabel->SetJustification(ETextJustify::Center);
		FireButton->AddChild(FireLabel);

		UCanvasPanelSlot* FireBtnSlot = RootCanvas->AddChildToCanvas(FireButton);
		// Positioned on the left side, just above the steering panel (steering anchors at Y 0.5)
		FireBtnSlot->SetAnchors(FAnchors(0.15f, 0.48f, 0.15f, 0.48f));
		FireBtnSlot->SetAlignment(FVector2D(0.5f, 1.f)); // bottom-center of button at anchor
		FireBtnSlot->SetOffsets(FMargin(0.f, 0.f, FireBtnSize, FireBtnSize));

		FireButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  RIGHT (next to Fire): "Drop" button -- bomb drop
		// ============================================================
		DropBombButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("DropBombButton"));

		const float DropBtnSize = 140.f;
		const float DropCornerR = DropBtnSize * 0.5f;

		auto MakeRoundBrushDrop = [&](const FLinearColor& Color) -> FSlateBrush
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings.CornerRadii = FVector4(DropCornerR, DropCornerR, DropCornerR, DropCornerR);
			Brush.OutlineSettings.Color = FSlateColor(FLinearColor::Transparent);
			Brush.OutlineSettings.Width = 0.f;
			return Brush;
		};

		FButtonStyle DropBtnStyle = DropBombButton->GetStyle();
		DropBtnStyle.Normal  = MakeRoundBrushDrop(FLinearColor(0.15f, 0.08f, 0.45f, 0.85f));
		DropBtnStyle.Hovered = MakeRoundBrushDrop(FLinearColor(0.22f, 0.12f, 0.55f, 0.95f));
		DropBtnStyle.Pressed = MakeRoundBrushDrop(FLinearColor(0.10f, 0.05f, 0.35f, 1.0f));
		DropBombButton->SetStyle(DropBtnStyle);

		UTextBlock* DropLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("DropBombLabel"));
		const FSlateFontInfo DropFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
		DropLabel->SetText(FText::FromString(TEXT("DROP")));
		DropLabel->SetFont(DropFont);
		DropLabel->SetColorAndOpacity(FSlateColor(White));
		DropLabel->SetJustification(ETextJustify::Center);
		DropBombButton->AddChild(DropLabel);

		UCanvasPanelSlot* DropBtnSlot = RootCanvas->AddChildToCanvas(DropBombButton);
		// To the right of Fire button (Fire at 0.77, Drop at 0.91)
		DropBtnSlot->SetAnchors(FAnchors(0.91f, 0.48f, 0.91f, 0.48f));
		DropBtnSlot->SetAlignment(FVector2D(0.5f, 1.f));
		DropBtnSlot->SetOffsets(FMargin(0.f, 0.f, DropBtnSize, DropBtnSize));

		DropBombButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  TOP-LEFT: Menu button (opens lobby HUD as overlay)
		// ============================================================
		MenuButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("MenuButton"));

		FButtonStyle MenuBtnStyle = MenuButton->GetStyle();
		MenuBtnStyle.Normal.TintColor  = FSlateColor(BarBg);
		MenuBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
		MenuBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		MenuButton->SetStyle(MenuBtnStyle);

		UTextBlock* MenuLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("MenuLabel"));
		MenuLabel->SetText(FText::FromString(TEXT("Menu")));
		MenuLabel->SetFont(BtnFont);
		MenuLabel->SetColorAndOpacity(FSlateColor(White));
		MenuLabel->SetJustification(ETextJustify::Center);
		MenuButton->AddChild(MenuLabel);

		UCanvasPanelSlot* MenuBtnSlot = RootCanvas->AddChildToCanvas(MenuButton);
		MenuBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f)); // top-left
		MenuBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		MenuBtnSlot->SetOffsets(FMargin(80.f, 16.f, 80.f, 40.f));

		// ============================================================
		//  TOP-LEFT (below Menu): "Car" button -- spawns Aventador
		// ============================================================
		CarButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("CarButton"));

		FButtonStyle CarBtnStyle = CarButton->GetStyle();
		CarBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.55f, 0.22f, 0.02f, 0.85f));
		CarBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.70f, 0.28f, 0.03f, 0.95f));
		CarBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.40f, 0.16f, 0.01f, 1.0f));
		CarButton->SetStyle(CarBtnStyle);

		UTextBlock* CarLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CarLabel"));
		CarLabel->SetText(FText::FromString(TEXT("Car")));
		CarLabel->SetFont(BtnFont);
		CarLabel->SetColorAndOpacity(FSlateColor(White));
		CarLabel->SetJustification(ETextJustify::Center);
		CarButton->AddChild(CarLabel);

		UCanvasPanelSlot* CarBtnSlot = RootCanvas->AddChildToCanvas(CarButton);
		CarBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f)); // top-left
		CarBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		CarBtnSlot->SetOffsets(FMargin(80.f, 64.f, 80.f, 40.f));

		// ============================================================
		//  TOP-LEFT (below Car): "Weapons" button
		// ============================================================
		WeaponsButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("WeaponsButton"));

		FButtonStyle WepBtnStyle = WeaponsButton->GetStyle();
		WepBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.6f, 0.1f, 0.1f, 0.85f));
		WepBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.75f, 0.15f, 0.15f, 0.95f));
		WepBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.45f, 0.07f, 0.07f, 1.0f));
		WeaponsButton->SetStyle(WepBtnStyle);

		UTextBlock* WepLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WeaponsLabel"));
		WepLabel->SetText(FText::FromString(TEXT("Weapons")));
		WepLabel->SetFont(BtnFont);
		WepLabel->SetColorAndOpacity(FSlateColor(White));
		WepLabel->SetJustification(ETextJustify::Center);
		WeaponsButton->AddChild(WepLabel);

		UCanvasPanelSlot* WepBtnSlot = RootCanvas->AddChildToCanvas(WeaponsButton);
		WepBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		WepBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		WepBtnSlot->SetOffsets(FMargin(170.f, 64.f, 100.f, 40.f));

		// ============================================================
		//  TOP-LEFT (below Weapons): "Night" toggle button
		// ============================================================
		NightButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("NightButton"));

		FButtonStyle NightBtnStyle = NightButton->GetStyle();
		NightBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.1f, 0.35f, 0.85f));
		NightBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.45f, 0.95f));
		NightBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.07f, 0.07f, 0.25f, 1.0f));
		NightButton->SetStyle(NightBtnStyle);

		NightButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("NightButtonLabel"));
		NightButtonLabel->SetText(FText::FromString(TEXT("Night")));
		NightButtonLabel->SetFont(BtnFont);
		NightButtonLabel->SetColorAndOpacity(FSlateColor(White));
		NightButtonLabel->SetJustification(ETextJustify::Center);
		NightButton->AddChild(NightButtonLabel);

		UCanvasPanelSlot* NightBtnSlot = RootCanvas->AddChildToCanvas(NightButton);
		NightBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		NightBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		NightBtnSlot->SetOffsets(FMargin(170.f, 120.f, 100.f, 40.f));

		// ============================================================
		//  Disasters button (below Night)
		// ============================================================
		DisastersButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("DisastersButton"));

		FButtonStyle DisBtnStyle = DisastersButton->GetStyle();
		DisBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.5f, 0.15f, 0.05f, 0.85f));
		DisBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.6f, 0.20f, 0.08f, 0.95f));
		DisBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.35f, 0.10f, 0.03f, 1.0f));
		DisastersButton->SetStyle(DisBtnStyle);

		DisastersButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("DisastersButtonLabel"));
		DisastersButtonLabel->SetText(FText::FromString(TEXT("Disasters")));
		DisastersButtonLabel->SetFont(BtnFont);
		DisastersButtonLabel->SetColorAndOpacity(FSlateColor(White));
		DisastersButtonLabel->SetJustification(ETextJustify::Center);
		DisastersButton->AddChild(DisastersButtonLabel);

		UCanvasPanelSlot* DisBtnSlot = RootCanvas->AddChildToCanvas(DisastersButton);
		DisBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		DisBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		DisBtnSlot->SetOffsets(FMargin(170.f, 176.f, 100.f, 40.f));

		// ============================================================
		//  MOBILE-ONLY: On-foot action buttons (bottom-right)
		// ============================================================
#if PLATFORM_IOS || PLATFORM_ANDROID
		const FLinearColor ActionBtnBg(0.15f, 0.15f, 0.15f, 0.7f);
		const FLinearColor ActionBtnHover(0.25f, 0.25f, 0.25f, 0.85f);
		const FLinearColor ActionBtnPress(0.35f, 0.35f, 0.35f, 0.95f);

		auto MakeActionButton = [&](const FName& Name, const FString& Label, float RightOffset, float BottomOffset, float Size) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
			FButtonStyle Style = Btn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(ActionBtnBg);
			Style.Hovered.TintColor = FSlateColor(ActionBtnHover);
			Style.Pressed.TintColor = FSlateColor(ActionBtnPress);
			Btn->SetStyle(Style);

			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("_Label"))));
			BtnLabel->SetText(FText::FromString(Label));
			BtnLabel->SetFont(BtnFont);
			BtnLabel->SetColorAndOpacity(FSlateColor(White));
			BtnLabel->SetJustification(ETextJustify::Center);
			Btn->AddChild(BtnLabel);

			UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Btn);
			Slot->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f)); // bottom-right
			Slot->SetAlignment(FVector2D(1.f, 1.f));
			Slot->SetOffsets(FMargin(-RightOffset, -BottomOffset, Size, Size));
			return Btn;
		};

		// ============================================================
		//  MOBILE-ONLY: Walking touch panels (left = MOVE, right = LOOK)
		// ============================================================
		const FSlateFontInfo WalkZoneFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

		// Move panel (left 45%, bottom 50%)
		WalkMovePanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WalkMovePanel"));
		WalkMovePanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		WalkMovePanel->SetPadding(FMargin(0.f));

		WalkMoveLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WalkMoveLabel"));
		WalkMoveLabel->SetText(FText::FromString(TEXT("MOVE")));
		WalkMoveLabel->SetFont(WalkZoneFont);
		WalkMoveLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.12f)));
		WalkMoveLabel->SetJustification(ETextJustify::Center);
		WalkMovePanel->SetContent(WalkMoveLabel);

		{
			UCanvasPanelSlot* WalkMoveSlot = RootCanvas->AddChildToCanvas(WalkMovePanel);
			WalkMoveSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.45f, 1.f));
			WalkMoveSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		}

		// Look panel (right 55%, bottom 50%)
		WalkLookPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WalkLookPanel"));
		WalkLookPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		WalkLookPanel->SetPadding(FMargin(0.f));

		WalkLookLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WalkLookLabel"));
		WalkLookLabel->SetText(FText::FromString(TEXT("LOOK")));
		WalkLookLabel->SetFont(WalkZoneFont);
		WalkLookLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.12f)));
		WalkLookLabel->SetJustification(ETextJustify::Center);
		WalkLookPanel->SetContent(WalkLookLabel);

		{
			UCanvasPanelSlot* WalkLookSlot = RootCanvas->AddChildToCanvas(WalkLookPanel);
			WalkLookSlot->SetAnchors(FAnchors(0.45f, 0.5f, 1.f, 1.f));
			WalkLookSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		}

		// ============================================================
		//  MOBILE-ONLY: Driving controls (hidden by default)
		// ============================================================

		const FSlateFontInfo DriveFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
		const FSlateFontInfo DriveFontSmall = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

		// -- Steering panel (left side, bottom half) --
		// Touch position within this panel controls steering:
		//   Left side of panel = steer left, right side = steer right.
		SteeringPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("SteeringPanel"));
		SteeringPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		SteeringPanel->SetPadding(FMargin(0.f));

		SteeringLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("SteeringLabel"));
		SteeringLabel->SetText(FText::FromString(FString::Printf(TEXT("%s   STEER   %s"),
			UTF8_TO_TCHAR("\xe2\x97\x84"), UTF8_TO_TCHAR("\xe2\x96\xba"))));
		SteeringLabel->SetFont(DriveFont);
		SteeringLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		SteeringLabel->SetJustification(ETextJustify::Center);
		SteeringPanel->SetContent(SteeringLabel);

		UCanvasPanelSlot* SteerSlot = RootCanvas->AddChildToCanvas(SteeringPanel);
		// Left 45% of screen, bottom 50% height
		SteerSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.45f, 1.f));
		SteerSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		SteeringPanel->SetVisibility(ESlateVisibility::Collapsed);

		// -- Gas pedal (right side, lower portion) --
		GasPedalPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("GasPedalPanel"));
		GasPedalPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		GasPedalPanel->SetPadding(FMargin(0.f));

		GasLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("GasLabel"));
		GasLabel->SetText(FText::FromString(TEXT("GAS")));
		GasLabel->SetFont(DriveFont);
		GasLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		GasLabel->SetJustification(ETextJustify::Center);
		GasPedalPanel->SetContent(GasLabel);

		UCanvasPanelSlot* GasSlot = RootCanvas->AddChildToCanvas(GasPedalPanel);
		// Right 45% of screen, bottom 35% height (above brake)
		GasSlot->SetAnchors(FAnchors(0.55f, 0.5f, 1.f, 0.75f));
		GasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		GasPedalPanel->SetVisibility(ESlateVisibility::Collapsed);

		// -- Brake pedal (right side, very bottom) --
		BrakePedalPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("BrakePedalPanel"));
		BrakePedalPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		BrakePedalPanel->SetPadding(FMargin(0.f));

		BrakeLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BrakeLabel"));
		BrakeLabel->SetText(FText::FromString(TEXT("BRAKE")));
		BrakeLabel->SetFont(DriveFontSmall);
		BrakeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		BrakeLabel->SetJustification(ETextJustify::Center);
		BrakePedalPanel->SetContent(BrakeLabel);

		UCanvasPanelSlot* BrakeSlot = RootCanvas->AddChildToCanvas(BrakePedalPanel);
		// Right 45%, bottom 25% height
		BrakeSlot->SetAnchors(FAnchors(0.55f, 0.75f, 1.f, 1.f));
		BrakeSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		BrakePedalPanel->SetVisibility(ESlateVisibility::Collapsed);

		// -- Exit Vehicle button (top-left, where "Car" button is in walk mode) --
		ExitVehicleButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("ExitVehicleButton"));

		FButtonStyle ExitBtnStyle = ExitVehicleButton->GetStyle();
		ExitBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.6f, 0.1f, 0.1f, 0.85f));
		ExitBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.8f, 0.15f, 0.15f, 0.95f));
		ExitBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.4f, 0.08f, 0.08f, 1.0f));
		ExitVehicleButton->SetStyle(ExitBtnStyle);

		UTextBlock* ExitLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ExitVehicleLabel"));
		ExitLabel->SetText(FText::FromString(TEXT("Exit")));
		ExitLabel->SetFont(BtnFont);
		ExitLabel->SetColorAndOpacity(FSlateColor(White));
		ExitLabel->SetJustification(ETextJustify::Center);
		ExitVehicleButton->AddChild(ExitLabel);

		UCanvasPanelSlot* ExitSlot = RootCanvas->AddChildToCanvas(ExitVehicleButton);
		ExitSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		ExitSlot->SetAlignment(FVector2D(0.f, 0.f));
		ExitSlot->SetOffsets(FMargin(80.f, 64.f, 80.f, 40.f));

		ExitVehicleButton->SetVisibility(ESlateVisibility::Collapsed);
#endif

		// ============================================================
		//  Vehicle selection popup (shown when "Car" button is tapped)
		// ============================================================
		VehicleSelectOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("VehicleSelectOverlay"));
		VehicleSelectOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		VehicleSelectOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(VehicleSelectOverlay);
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		OverlaySlot->SetOffsets(FMargin(0.f));

		// Inner panel (centered container for the two buttons).
		UBorder* VehicleSelectPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("VehicleSelectPanel"));
		VehicleSelectPanel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.95f));
		VehicleSelectPanel->SetPadding(FMargin(24.f, 16.f, 24.f, 16.f));
		VehicleSelectPanel->SetHorizontalAlignment(HAlign_Center);
		VehicleSelectPanel->SetVerticalAlignment(VAlign_Center);
		VehicleSelectOverlay->SetContent(VehicleSelectPanel);

		UHorizontalBox* SelectHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("VehicleSelectHBox"));
		VehicleSelectPanel->SetContent(SelectHBox);

		// -- Aventador button (burnt orange) --
		SelectAventadorBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectAventadorBtn"));
		{
			FButtonStyle Style = SelectAventadorBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.55f, 0.22f, 0.02f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.70f, 0.28f, 0.03f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.40f, 0.16f, 0.01f, 1.0f));
			SelectAventadorBtn->SetStyle(Style);
		}
		UTextBlock* AventadorLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("AventadorSelectLabel"));
		AventadorLabel->SetText(FText::FromString(TEXT("Lamborghini")));
		AventadorLabel->SetFont(BtnFont);
		AventadorLabel->SetColorAndOpacity(FSlateColor(White));
		AventadorLabel->SetJustification(ETextJustify::Center);
		SelectAventadorBtn->AddChild(AventadorLabel);

		UHorizontalBoxSlot* AvSlot = SelectHBox->AddChildToHorizontalBox(SelectAventadorBtn);
		AvSlot->SetPadding(FMargin(8.f));
		AvSlot->SetVerticalAlignment(VAlign_Center);
		AvSlot->SetHorizontalAlignment(HAlign_Center);

		// Spacer between buttons.
		USpacer* BtnSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("VehicleBtnSpacer"));
		BtnSpacer->SetSize(FVector2D(20.f, 0.f));
		SelectHBox->AddChildToHorizontalBox(BtnSpacer);

		// -- Dune Buggy button (olive green) --
		SelectBuggyBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectBuggyBtn"));
		{
			FButtonStyle Style = SelectBuggyBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.28f, 0.35f, 0.12f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.35f, 0.42f, 0.18f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.20f, 0.26f, 0.08f, 1.0f));
			SelectBuggyBtn->SetStyle(Style);
		}
		UTextBlock* BuggyLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BuggySelectLabel"));
		BuggyLabel->SetText(FText::FromString(TEXT("Dune Buggy")));
		BuggyLabel->SetFont(BtnFont);
		BuggyLabel->SetColorAndOpacity(FSlateColor(White));
		BuggyLabel->SetJustification(ETextJustify::Center);
		SelectBuggyBtn->AddChild(BuggyLabel);

		UHorizontalBoxSlot* BgSlot = SelectHBox->AddChildToHorizontalBox(SelectBuggyBtn);
		BgSlot->SetPadding(FMargin(8.f));
		BgSlot->SetVerticalAlignment(VAlign_Center);
		BgSlot->SetHorizontalAlignment(HAlign_Center);

		// Spacer between Buggy and Raptor buttons.
		USpacer* BtnSpacer2 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("VehicleBtnSpacer2"));
		BtnSpacer2->SetSize(FVector2D(20.f, 0.f));
		SelectHBox->AddChildToHorizontalBox(BtnSpacer2);

		// -- F-22 Raptor button (stealth gray) --
		SelectRaptorBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectRaptorBtn"));
		{
			FButtonStyle Style = SelectRaptorBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.28f, 0.30f, 0.32f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.35f, 0.37f, 0.40f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.20f, 0.22f, 0.24f, 1.0f));
			SelectRaptorBtn->SetStyle(Style);
		}
		UTextBlock* RaptorLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("RaptorSelectLabel"));
		RaptorLabel->SetText(FText::FromString(TEXT("F-22 Raptor")));
		RaptorLabel->SetFont(BtnFont);
		RaptorLabel->SetColorAndOpacity(FSlateColor(White));
		RaptorLabel->SetJustification(ETextJustify::Center);
		SelectRaptorBtn->AddChild(RaptorLabel);

		UHorizontalBoxSlot* RpSlot = SelectHBox->AddChildToHorizontalBox(SelectRaptorBtn);
		RpSlot->SetPadding(FMargin(8.f));
		RpSlot->SetVerticalAlignment(VAlign_Center);
		RpSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* VehSpacer3 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("VehBtnSpacer3"));
		VehSpacer3->SetSize(FVector2D(20.f, 0.f));
		SelectHBox->AddChildToHorizontalBox(VehSpacer3);

		// -- Jetpack button (sky blue) --
		SelectJetpackBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectJetpackBtn"));
		{
			FButtonStyle Style = SelectJetpackBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.10f, 0.30f, 0.55f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.40f, 0.70f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.07f, 0.20f, 0.40f, 1.0f));
			SelectJetpackBtn->SetStyle(Style);
		}
		UTextBlock* JetpackLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("JetpackSelectLabel"));
		JetpackLabel->SetText(FText::FromString(TEXT("AirBoots")));
		JetpackLabel->SetFont(BtnFont);
		JetpackLabel->SetColorAndOpacity(FSlateColor(White));
		JetpackLabel->SetJustification(ETextJustify::Center);
		SelectJetpackBtn->AddChild(JetpackLabel);

		UHorizontalBoxSlot* JpSlot = SelectHBox->AddChildToHorizontalBox(SelectJetpackBtn);
		JpSlot->SetPadding(FMargin(8.f));
		JpSlot->SetVerticalAlignment(VAlign_Center);
		JpSlot->SetHorizontalAlignment(HAlign_Center);

		// -- M60 Tank button (olive green) --
		SelectTankBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectTankBtn"));
		{
			FButtonStyle Style = SelectTankBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.22f, 0.28f, 0.14f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.30f, 0.38f, 0.20f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.15f, 0.20f, 0.10f, 1.0f));
			SelectTankBtn->SetStyle(Style);
		}
		UTextBlock* TankLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("TankSelectLabel"));
		TankLabel->SetText(FText::FromString(TEXT("M60 Tank")));
		TankLabel->SetFont(BtnFont);
		TankLabel->SetColorAndOpacity(FSlateColor(White));
		TankLabel->SetJustification(ETextJustify::Center);
		SelectTankBtn->AddChild(TankLabel);

		UHorizontalBoxSlot* TankSlot = SelectHBox->AddChildToHorizontalBox(SelectTankBtn);
		TankSlot->SetPadding(FMargin(8.f));
		TankSlot->SetVerticalAlignment(VAlign_Center);
		TankSlot->SetHorizontalAlignment(HAlign_Center);

		VehicleSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Weapon selection popup
		// ============================================================
		WeaponSelectOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WeaponSelectOverlay"));
		WeaponSelectOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		WeaponSelectOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* WepOverlaySlot = RootCanvas->AddChildToCanvas(WeaponSelectOverlay);
		WepOverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		WepOverlaySlot->SetOffsets(FMargin(0.f));

		UBorder* WepSelectPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WeaponSelectPanel"));
		WepSelectPanel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.95f));
		WepSelectPanel->SetPadding(FMargin(24.f, 16.f, 24.f, 16.f));
		WepSelectPanel->SetHorizontalAlignment(HAlign_Center);
		WepSelectPanel->SetVerticalAlignment(VAlign_Center);
		WeaponSelectOverlay->SetContent(WepSelectPanel);

		UHorizontalBox* WepHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("WeaponSelectHBox"));
		WepSelectPanel->SetContent(WepHBox);

		// -- Gun button (steel gray) --
		SelectGunBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectGunBtn"));
		{
			FButtonStyle Style = SelectGunBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.35f, 0.35f, 0.40f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.45f, 0.45f, 0.50f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.25f, 0.25f, 0.30f, 1.0f));
			SelectGunBtn->SetStyle(Style);
		}
		UTextBlock* GunLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("GunSelectLabel"));
		GunLabel->SetText(FText::FromString(TEXT("Gun")));
		GunLabel->SetFont(BtnFont);
		GunLabel->SetColorAndOpacity(FSlateColor(White));
		GunLabel->SetJustification(ETextJustify::Center);
		SelectGunBtn->AddChild(GunLabel);

		UHorizontalBoxSlot* GunSlot = WepHBox->AddChildToHorizontalBox(SelectGunBtn);
		GunSlot->SetPadding(FMargin(8.f));
		GunSlot->SetVerticalAlignment(VAlign_Center);
		GunSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer1 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer1"));
		WepSpacer1->SetSize(FVector2D(20.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer1);

		// -- RPG button (dark red) --
		SelectRPGBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectRPGBtn"));
		{
			FButtonStyle Style = SelectRPGBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.55f, 0.12f, 0.08f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.70f, 0.18f, 0.12f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.40f, 0.08f, 0.05f, 1.0f));
			SelectRPGBtn->SetStyle(Style);
		}
		UTextBlock* RPGLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("RPGSelectLabel"));
		RPGLabel->SetText(FText::FromString(TEXT("RPG")));
		RPGLabel->SetFont(BtnFont);
		RPGLabel->SetColorAndOpacity(FSlateColor(White));
		RPGLabel->SetJustification(ETextJustify::Center);
		SelectRPGBtn->AddChild(RPGLabel);

		UHorizontalBoxSlot* RPGSlot = WepHBox->AddChildToHorizontalBox(SelectRPGBtn);
		RPGSlot->SetPadding(FMargin(8.f));
		RPGSlot->SetVerticalAlignment(VAlign_Center);
		RPGSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer2 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer2"));
		WepSpacer2->SetSize(FVector2D(20.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer2);

		// -- Sword button (gold) --
		SelectSwordBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectSwordBtn"));
		{
			FButtonStyle Style = SelectSwordBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.55f, 0.45f, 0.08f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.70f, 0.58f, 0.12f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.40f, 0.32f, 0.05f, 1.0f));
			SelectSwordBtn->SetStyle(Style);
		}
		UTextBlock* SwordLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("SwordSelectLabel"));
		SwordLabel->SetText(FText::FromString(TEXT("Sword")));
		SwordLabel->SetFont(BtnFont);
		SwordLabel->SetColorAndOpacity(FSlateColor(White));
		SwordLabel->SetJustification(ETextJustify::Center);
		SelectSwordBtn->AddChild(SwordLabel);

		UHorizontalBoxSlot* SwordSlot = WepHBox->AddChildToHorizontalBox(SelectSwordBtn);
		SwordSlot->SetPadding(FMargin(8.f));
		SwordSlot->SetVerticalAlignment(VAlign_Center);
		SwordSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer3 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer3"));
		WepSpacer3->SetSize(FVector2D(20.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer3);

		// -- Scythe button (dark purple) --
		SelectScytheBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectScytheBtn"));
		{
			FButtonStyle Style = SelectScytheBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.30f, 0.10f, 0.40f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.40f, 0.15f, 0.55f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.20f, 0.06f, 0.28f, 1.0f));
			SelectScytheBtn->SetStyle(Style);
		}
		UTextBlock* ScytheLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ScytheSelectLabel"));
		ScytheLabel->SetText(FText::FromString(TEXT("Scythe")));
		ScytheLabel->SetFont(BtnFont);
		ScytheLabel->SetColorAndOpacity(FSlateColor(White));
		ScytheLabel->SetJustification(ETextJustify::Center);
		SelectScytheBtn->AddChild(ScytheLabel);

		UHorizontalBoxSlot* ScytheSlot = WepHBox->AddChildToHorizontalBox(SelectScytheBtn);
		ScytheSlot->SetPadding(FMargin(8.f));
		ScytheSlot->SetVerticalAlignment(VAlign_Center);
		ScytheSlot->SetHorizontalAlignment(HAlign_Center);

		WeaponSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Disaster selection overlay
		// ============================================================
		DisasterSelectOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("DisasterSelectOverlay"));
		DisasterSelectOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		DisasterSelectOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* DisOverlaySlot = RootCanvas->AddChildToCanvas(DisasterSelectOverlay);
		DisOverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DisOverlaySlot->SetOffsets(FMargin(0.f));

		UBorder* DisSelectPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("DisasterSelectPanel"));
		DisSelectPanel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.95f));
		DisSelectPanel->SetPadding(FMargin(24.f, 16.f, 24.f, 16.f));
		DisSelectPanel->SetHorizontalAlignment(HAlign_Center);
		DisSelectPanel->SetVerticalAlignment(VAlign_Center);
		DisasterSelectOverlay->SetContent(DisSelectPanel);

		UHorizontalBox* DisHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("DisasterSelectHBox"));
		DisSelectPanel->SetContent(DisHBox);

		// -- Tornado button (gray) --
		SelectTornadoBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectTornadoBtn"));
		{
			FButtonStyle Style = SelectTornadoBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.40f, 0.40f, 0.38f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.50f, 0.50f, 0.48f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.30f, 0.30f, 0.28f, 1.0f));
			SelectTornadoBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TornadoLabel"));
			L->SetText(FText::FromString(TEXT("Tornado")));
			L->SetFont(BtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectTornadoBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectTornadoBtn);
			S->SetPadding(FMargin(8.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp1")); Sp->SetSize(FVector2D(16.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Earthquake button (brown) --
		SelectEarthquakeBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectEarthquakeBtn"));
		{
			FButtonStyle Style = SelectEarthquakeBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.45f, 0.30f, 0.15f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.55f, 0.38f, 0.20f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.35f, 0.22f, 0.10f, 1.0f));
			SelectEarthquakeBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuakeLabel"));
			L->SetText(FText::FromString(TEXT("Quake")));
			L->SetFont(BtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectEarthquakeBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectEarthquakeBtn);
			S->SetPadding(FMargin(8.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp2")); Sp->SetSize(FVector2D(16.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Thunderstorm button (purple) --
		SelectThunderstormBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectThunderstormBtn"));
		{
			FButtonStyle Style = SelectThunderstormBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.20f, 0.10f, 0.35f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.30f, 0.15f, 0.50f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.15f, 0.07f, 0.25f, 1.0f));
			SelectThunderstormBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ThunderLabel"));
			L->SetText(FText::FromString(TEXT("Thunder")));
			L->SetFont(BtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectThunderstormBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectThunderstormBtn);
			S->SetPadding(FMargin(8.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp3")); Sp->SetSize(FVector2D(16.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Meteor button (orange-red) --
		SelectMeteorBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectMeteorBtn"));
		{
			FButtonStyle Style = SelectMeteorBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.55f, 0.20f, 0.05f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.70f, 0.28f, 0.08f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.40f, 0.14f, 0.03f, 1.0f));
			SelectMeteorBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MeteorLabel"));
			L->SetText(FText::FromString(TEXT("Meteor")));
			L->SetFont(BtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectMeteorBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectMeteorBtn);
			S->SetPadding(FMargin(8.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp4")); Sp->SetSize(FVector2D(16.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Stop button (dark gray) --
		StopDisasterBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("StopDisasterBtn"));
		{
			FButtonStyle Style = StopDisasterBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.25f, 0.25f, 0.25f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.35f, 0.35f, 0.35f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));
			StopDisasterBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StopDisLabel"));
			L->SetText(FText::FromString(TEXT("Stop")));
			L->SetFont(BtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			StopDisasterBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(StopDisasterBtn);
			S->SetPadding(FMargin(8.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		DisasterSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Weapon fire panel (large, barely visible — like steering panel)
		//  Right 45% of screen, bottom 50% height
		// ============================================================
		WeaponFireButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("WeaponFireButton"));
		{
			FButtonStyle Style = WeaponFireButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.03f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.06f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.6f, 0.1f, 1.f, 0.12f));
			WeaponFireButton->SetStyle(Style);
		}
		WeaponFireButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
		WeaponFireButton->SetClickMethod(EButtonClickMethod::MouseDown);
		WeaponFireButton->SetPressMethod(EButtonPressMethod::ButtonPress);

		WeaponFireLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WeaponFireLabel"));
		WeaponFireLabel->SetText(FText::FromString(TEXT("SPRAY")));
		const FSlateFontInfo WepFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
		WeaponFireLabel->SetFont(WepFont);
		WeaponFireLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		WeaponFireLabel->SetJustification(ETextJustify::Center);
		WeaponFireButton->AddChild(WeaponFireLabel);

		UCanvasPanelSlot* WepFireSlot = RootCanvas->AddChildToCanvas(WeaponFireButton);
		WepFireSlot->SetAnchors(FAnchors(0.55f, 0.75f, 1.f, 1.f));
		WepFireSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		WeaponFireButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  TOP-LEFT: Multiplayer buttons (below Car button)
		// ============================================================
		HostButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("HostButton"));
		{
			FButtonStyle Style = HostButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.35f, 0.1f, 0.85f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.45f, 0.15f, 0.95f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.25f, 0.08f, 1.0f));
			HostButton->SetStyle(Style);
		}

		UTextBlock* HostLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("HostLabel"));
		HostLabel->SetText(FText::FromString(TEXT("Host")));
		HostLabel->SetFont(BtnFont);
		HostLabel->SetColorAndOpacity(FSlateColor(White));
		HostLabel->SetJustification(ETextJustify::Center);
		HostButton->AddChild(HostLabel);

		UCanvasPanelSlot* HostSlot = RootCanvas->AddChildToCanvas(HostButton);
		HostSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		HostSlot->SetAlignment(FVector2D(0.f, 0.f));
		HostSlot->SetOffsets(FMargin(80.f, 112.f, 80.f, 40.f));

		JoinButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("JoinButton"));
		{
			FButtonStyle Style = JoinButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.2f, 0.45f, 0.85f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.28f, 0.55f, 0.95f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.15f, 0.35f, 1.0f));
			JoinButton->SetStyle(Style);
		}

		UTextBlock* JoinLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("JoinLabel"));
		JoinLabel->SetText(FText::FromString(TEXT("Join")));
		JoinLabel->SetFont(BtnFont);
		JoinLabel->SetColorAndOpacity(FSlateColor(White));
		JoinLabel->SetJustification(ETextJustify::Center);
		JoinButton->AddChild(JoinLabel);

		UCanvasPanelSlot* JoinSlot = RootCanvas->AddChildToCanvas(JoinButton);
		JoinSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		JoinSlot->SetAlignment(FVector2D(0.f, 0.f));
		JoinSlot->SetOffsets(FMargin(80.f, 160.f, 80.f, 40.f));

		// Player count display (top-right, below currency)
		PlayerCountText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("PlayerCountText"));
		PlayerCountText->SetText(FText::FromString(TEXT("Players: 1")));
		PlayerCountText->SetFont(BtnFont);
		PlayerCountText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.7f)));

		UCanvasPanelSlot* PlayerCountSlot = RootCanvas->AddChildToCanvas(PlayerCountText);
		PlayerCountSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		PlayerCountSlot->SetAlignment(FVector2D(1.f, 0.f));
		PlayerCountSlot->SetOffsets(FMargin(-80.f, 56.f, 0.f, 0.f));
		PlayerCountSlot->SetAutoSize(true);

		// -- Join IP input overlay (hidden by default) --
		JoinOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("JoinOverlay"));
		JoinOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
		JoinOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* JoinOverlaySlot = RootCanvas->AddChildToCanvas(JoinOverlay);
		JoinOverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		JoinOverlaySlot->SetOffsets(FMargin(0.f));

		UBorder* JoinPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("JoinPanel"));
		JoinPanel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.95f));
		JoinPanel->SetPadding(FMargin(24.f, 16.f, 24.f, 16.f));
		JoinPanel->SetHorizontalAlignment(HAlign_Center);
		JoinPanel->SetVerticalAlignment(VAlign_Center);
		JoinOverlay->SetContent(JoinPanel);

		UHorizontalBox* JoinHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("JoinHBox"));
		JoinPanel->SetContent(JoinHBox);

		UTextBlock* IPLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("IPLabel"));
		IPLabel->SetText(FText::FromString(TEXT("IP: ")));
		IPLabel->SetFont(BtnFont);
		IPLabel->SetColorAndOpacity(FSlateColor(White));
		UHorizontalBoxSlot* IPLabelSlot = JoinHBox->AddChildToHorizontalBox(IPLabel);
		IPLabelSlot->SetVerticalAlignment(VAlign_Center);
		IPLabelSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

		IPInputText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("IPInputText"));
		IPInputText->SetText(FText::FromString(TEXT("GAME_SERVER_IP")));
		IPInputText->SetFont(BtnFont);
		IPInputText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.f, 0.5f)));
		UHorizontalBoxSlot* IPInputSlot = JoinHBox->AddChildToHorizontalBox(IPInputText);
		IPInputSlot->SetVerticalAlignment(VAlign_Center);
		IPInputSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

		// Number pad buttons for IP input (0-9 + backspace + connect + cancel)
		// For V1 simplicity, just use Connect and Cancel buttons with a hardcoded IP
		JoinConnectButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("JoinConnectButton"));
		{
			FButtonStyle Style = JoinConnectButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.35f, 0.1f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.45f, 0.15f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.25f, 0.08f, 1.0f));
			JoinConnectButton->SetStyle(Style);
		}
		UTextBlock* ConnectLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ConnectLabel"));
		ConnectLabel->SetText(FText::FromString(TEXT("Connect")));
		ConnectLabel->SetFont(BtnFont);
		ConnectLabel->SetColorAndOpacity(FSlateColor(White));
		ConnectLabel->SetJustification(ETextJustify::Center);
		JoinConnectButton->AddChild(ConnectLabel);

		UHorizontalBoxSlot* ConnectSlot = JoinHBox->AddChildToHorizontalBox(JoinConnectButton);
		ConnectSlot->SetPadding(FMargin(4.f));
		ConnectSlot->SetVerticalAlignment(VAlign_Center);

		JoinCancelButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("JoinCancelButton"));
		{
			FButtonStyle Style = JoinCancelButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.5f, 0.1f, 0.1f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.65f, 0.15f, 0.15f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.35f, 0.08f, 0.08f, 1.0f));
			JoinCancelButton->SetStyle(Style);
		}
		UTextBlock* CancelLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CancelLabel"));
		CancelLabel->SetText(FText::FromString(TEXT("Cancel")));
		CancelLabel->SetFont(BtnFont);
		CancelLabel->SetColorAndOpacity(FSlateColor(White));
		CancelLabel->SetJustification(ETextJustify::Center);
		JoinCancelButton->AddChild(CancelLabel);

		UHorizontalBoxSlot* CancelSlot = JoinHBox->AddChildToHorizontalBox(JoinCancelButton);
		CancelSlot->SetPadding(FMargin(4.f));
		CancelSlot->SetVerticalAlignment(VAlign_Center);

		JoinOverlay->SetVisibility(ESlateVisibility::Collapsed);
		EnteredIP = TEXT("GAME_SERVER_IP");

		UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Widget tree built programmatically."));
	}

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UPHCCityHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (MenuButton)
	{
		MenuButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnMenuButtonClicked);
	}
	if (CarButton)
	{
		CarButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnCarButtonClicked);
	}
	if (SelectAventadorBtn)
	{
		SelectAventadorBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectAventador);
	}
	if (SelectBuggyBtn)
	{
		SelectBuggyBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectBuggy);
	}
	if (SelectRaptorBtn)
	{
		SelectRaptorBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectRaptor);
	}
	if (SelectJetpackBtn)
	{
		SelectJetpackBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectJetpack);
	}
	if (SelectTankBtn)
	{
		SelectTankBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectTank);
	}
	if (FireButton)
	{
		FireButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnFireButtonPressed);
		FireButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnFireButtonReleased);
	}
	if (DropBombButton)
	{
		DropBombButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDropBombClicked);
	}
	if (WeaponsButton)
	{
		WeaponsButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnWeaponsButtonClicked);
	}
	if (NightButton)
	{
		NightButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnNightButtonClicked);
	}
	if (DisastersButton)
	{
		DisastersButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDisastersButtonClicked);
	}
	if (SelectTornadoBtn)
	{
		SelectTornadoBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectTornado);
	}
	if (SelectEarthquakeBtn)
	{
		SelectEarthquakeBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectEarthquake);
	}
	if (SelectThunderstormBtn)
	{
		SelectThunderstormBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectThunderstorm);
	}
	if (SelectMeteorBtn)
	{
		SelectMeteorBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectMeteor);
	}
	if (StopDisasterBtn)
	{
		StopDisasterBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnStopDisaster);
	}
	if (SelectGunBtn)
	{
		SelectGunBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectGun);
	}
	if (SelectRPGBtn)
	{
		SelectRPGBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectRPG);
	}
	if (SelectSwordBtn)
	{
		SelectSwordBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectSword);
	}
	if (SelectScytheBtn)
	{
		SelectScytheBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectScythe);
	}
	if (WeaponFireButton)
	{
		WeaponFireButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnWeaponFirePressed);
		WeaponFireButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnWeaponFireReleased);
		// Scythe is default equipped — show swing button immediately
		if (WeaponFireLabel)
		{
			WeaponFireLabel->SetText(FText::FromString(TEXT("Swing")));
		}
		WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnHostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnJoinButtonClicked);
	}
	if (JoinConnectButton)
	{
		JoinConnectButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnJoinConnectClicked);
	}
	if (JoinCancelButton)
	{
		JoinCancelButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnJoinCancelClicked);
	}

#if PLATFORM_IOS || PLATFORM_ANDROID
	// Jump/Sprint/Interact buttons removed — mobile uses touch panels.
	if (ExitVehicleButton)
	{
		ExitVehicleButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnExitVehicleClicked);
	}
#endif
}

void UPHCCityHUD::NativeDestruct()
{
	if (MenuButton)
	{
		MenuButton->OnClicked.RemoveAll(this);
	}
	if (CloseMenuButton)
	{
		CloseMenuButton->OnClicked.RemoveAll(this);
	}
	if (CarButton)
	{
		CarButton->OnClicked.RemoveAll(this);
	}
	if (SelectAventadorBtn)
	{
		SelectAventadorBtn->OnClicked.RemoveAll(this);
	}
	if (SelectBuggyBtn)
	{
		SelectBuggyBtn->OnClicked.RemoveAll(this);
	}
	if (SelectRaptorBtn)
	{
		SelectRaptorBtn->OnClicked.RemoveAll(this);
	}
	if (FireButton)
	{
		FireButton->OnPressed.RemoveAll(this);
		FireButton->OnReleased.RemoveAll(this);
	}
	if (DropBombButton)
	{
		DropBombButton->OnClicked.RemoveAll(this);
	}
	if (HostButton) { HostButton->OnClicked.RemoveAll(this); }
	if (JoinButton) { JoinButton->OnClicked.RemoveAll(this); }
	if (JoinConnectButton) { JoinConnectButton->OnClicked.RemoveAll(this); }
	if (JoinCancelButton) { JoinCancelButton->OnClicked.RemoveAll(this); }

#if PLATFORM_IOS || PLATFORM_ANDROID
	// Jump/Sprint/Interact buttons removed.
	if (ExitVehicleButton)
	{
		ExitVehicleButton->OnClicked.RemoveAll(this);
	}
#endif

	CloseMenu();

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// NativeTick -- apply mobile driving touch input
// ---------------------------------------------------------------------------

void UPHCCityHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update speedometer + live location monitor
	if (LocationText)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* P = PC->GetPawn())
			{
				const FVector Loc = P->GetActorLocation();

				// Frame-to-frame speed calculation
				if (!bSpeedInitialized)
				{
					PrevPawnLocation = Loc;
					bSpeedInitialized = true;
				}
				const float Dt = FMath::Max(InDeltaTime, 0.001f);
				const float FrameSpeed = (Loc - PrevPawnLocation).Size() / Dt;
				SmoothedSpeed = FMath::FInterpTo(SmoothedSpeed, FrameSpeed, InDeltaTime, 5.f);
				PrevPawnLocation = Loc;

				// Convert UU/s to mph (1 UU = 1 cm; 1 cm/s = 0.02237 mph)
				const float SpeedMPH = SmoothedSpeed * 0.02237f;
				const TCHAR* Mode = bDrivingMode ? TEXT("DRIVING") : TEXT("WALKING");

				LocationText->SetText(FText::FromString(
					FString::Printf(TEXT("%.1f mph  %s  |  X: %.0f  Y: %.0f  Z: %.0f"),
						SpeedMPH, Mode, Loc.X, Loc.Y, Loc.Z)));
			}
		}
	}

	// Update player count display
	if (PlayerCountText)
	{
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState())
			{
				const int32 NumPlayers = GS->PlayerArray.Num();
				PlayerCountText->SetText(FText::FromString(
					FString::Printf(TEXT("Players: %d"), NumPlayers)));
			}
		}
	}

#if PLATFORM_IOS || PLATFORM_ANDROID
	do {
		APlayerController* PC = GetOwningPlayer();
		if (!PC) break;

		int32 VPWidth = 0;
		int32 VPHeight = 0;
		PC->GetViewportSize(VPWidth, VPHeight);
		if (VPWidth == 0 || VPHeight == 0) break;

		const float ScreenHalfX = VPWidth * 0.5f;
		const float ScreenHalfY = VPHeight * 0.5f;

		if (bDrivingMode && DrivenVehicle)
		{
			// ---- DRIVING TOUCH INPUT (unchanged) ----
			float NewSteering = 0.f;
			float NewThrottle = 0.f;
			float NewPitch = 0.f;
			bool bFireTouched = false;

			const float FireCenterX = VPWidth * 0.77f;
			const float FireBtnRadius = 84.f;
			const float FireBottomY = VPHeight * 0.48f;
			const float FireTopY = FireBottomY - FireBtnRadius * 2.f;

			for (int32 i = 0; i < 5; ++i)
			{
				float TouchX = 0.f;
				float TouchY = 0.f;
				bool bPressed = false;
				PC->GetInputTouchState(static_cast<ETouchIndex::Type>(i), TouchX, TouchY, bPressed);
				if (!bPressed) { continue; }

				if (TouchX >= FireCenterX - FireBtnRadius && TouchX <= FireCenterX + FireBtnRadius &&
					TouchY >= FireTopY && TouchY <= FireBottomY)
				{
					bFireTouched = true;
					continue;
				}

				if (TouchX < ScreenHalfX && TouchY > ScreenHalfY)
				{
					NewSteering = (TouchX / ScreenHalfX) * 2.f - 1.f;
					NewSteering = FMath::Clamp(NewSteering, -1.f, 1.f);

					const float NormY = (TouchY - ScreenHalfY) / ScreenHalfY;
					NewPitch = 1.f - NormY * 2.f;
					NewPitch = FMath::Clamp(NewPitch, -1.f, 1.f);
				}
				else if (TouchX >= ScreenHalfX && TouchY > ScreenHalfY)
				{
					const float ThresholdY = VPHeight * 0.75f;
					NewThrottle = (TouchY < ThresholdY) ? 1.f : -1.f;
				}
			}

			if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(DrivenVehicle))
			{
				V->SteeringInput = NewSteering;
				V->ThrottleInput = NewThrottle;
				if (!V->HasAuthority()) { V->ServerSetSteering(NewSteering); V->ServerSetThrottle(NewThrottle); }
			}
			else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
			{
				B->SteeringInput = NewSteering;
				B->ThrottleInput = NewThrottle;
				if (!B->HasAuthority()) { B->ServerSetSteering(NewSteering); B->ServerSetThrottle(NewThrottle); }
			}
			else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(DrivenVehicle))
			{
				R->SteeringInput = NewSteering;
				R->ThrottleInput = NewThrottle;
				R->PitchInput = NewPitch;
				if (!R->HasAuthority()) { R->ServerSetSteering(NewSteering); R->ServerSetThrottle(NewThrottle); R->ServerSetPitch(NewPitch); }
			}
			else if (APHCTankPawn* T = Cast<APHCTankPawn>(DrivenVehicle))
			{
				T->SteeringInput = NewSteering;
				T->ThrottleInput = NewThrottle;
				// Use pitch axis (left panel vertical) for turret rotation
				T->TurretYawInput = NewPitch;
				if (!T->HasAuthority()) { T->ServerSetSteering(NewSteering); T->ServerSetThrottle(NewThrottle); T->ServerSetTurretYaw(NewPitch); }
			}

			if (bFireTouched)
			{
				if (APHCDuneBuggyPawn* Buggy = Cast<APHCDuneBuggyPawn>(DrivenVehicle)) { Buggy->FireMissile(); }
				else if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle)) { Raptor->FireRocket(); }
				else if (APHCTankPawn* Tank = Cast<APHCTankPawn>(DrivenVehicle)) { Tank->FireCannon(); }
			}
		}
		else
		{
			// ---- WALKING TOUCH INPUT ----
			// Same pattern as driving: GetInputTouchState each frame.
			// Left side = virtual joystick (drag from initial touch).
			// Right side = camera look (drag delta).

			APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn());
			if (!Char) break;

			const float ScreenMidX = VPWidth * 0.5f;  // 50/50 split
			const float JoystickMaxRadius = VPWidth * 0.10f;
			const float DeadZone = VPWidth * 0.015f;
			const float LookSensitivity = 0.15f;
			const float ButtonZoneY = VPHeight * 0.25f; // top 25% reserved for buttons

			float MoveX = 0.f;
			float MoveY = 0.f;
			float LookDeltaX = 0.f;
			float LookDeltaY = 0.f;
			bool bMoveFingStillDown = false;
			bool bLookFingStillDown = false;

			for (int32 i = 0; i < 5; ++i)
			{
				float TouchX = 0.f;
				float TouchY = 0.f;
				bool bPressed = false;
				PC->GetInputTouchState(static_cast<ETouchIndex::Type>(i), TouchX, TouchY, bPressed);

				if (!bPressed)
				{
					if (i == WalkMoveFingerIdx)  { WalkMoveFingerIdx = -1; WalkMoveOrigin = FVector2D::ZeroVector; }
					if (i == WalkLookFingerIdx)  { WalkLookFingerIdx = -1; WalkLookPrev = FVector2D::ZeroVector; }
					continue;
				}

				// Skip top 25% of screen where HUD buttons live
				if (TouchY < ButtonZoneY) { continue; }

				const FVector2D TouchPos(TouchX, TouchY);

				if (TouchX < ScreenMidX)
				{
					// --- Left half: movement joystick ---
					if (WalkMoveFingerIdx < 0)
					{
						WalkMoveFingerIdx = i;
						WalkMoveOrigin = TouchPos;
					}
					if (i == WalkMoveFingerIdx)
					{
						bMoveFingStillDown = true;
						const FVector2D Offset = TouchPos - WalkMoveOrigin;
						const float Dist = Offset.Size();
						if (Dist > DeadZone)
						{
							FVector2D Normalized = Offset / JoystickMaxRadius;
							if (Normalized.Size() > 1.f) { Normalized = Normalized.GetSafeNormal(); }
							MoveX = Normalized.X;
							MoveY = -Normalized.Y; // screen Y is inverted
						}
					}
				}
				else
				{
					// --- Right half: camera look ---
					if (WalkLookFingerIdx < 0)
					{
						WalkLookFingerIdx = i;
						WalkLookPrev = TouchPos;
					}
					if (i == WalkLookFingerIdx)
					{
						bLookFingStillDown = true;
						const FVector2D Delta = (TouchPos - WalkLookPrev) * LookSensitivity;
						LookDeltaX = Delta.X;
						LookDeltaY = Delta.Y;
						WalkLookPrev = TouchPos;
					}
				}
			}

			if (!bMoveFingStillDown && WalkMoveFingerIdx >= 0)
			{
				WalkMoveFingerIdx = -1;
				WalkMoveOrigin = FVector2D::ZeroVector;
			}
			if (!bLookFingStillDown && WalkLookFingerIdx >= 0)
			{
				WalkLookFingerIdx = -1;
				WalkLookPrev = FVector2D::ZeroVector;
			}

			Char->TouchMoveX = MoveX;
			Char->TouchMoveY = MoveY;
			Char->TouchLookX = LookDeltaX;
			Char->TouchLookY = LookDeltaY;
			Char->bHUDTouchActive = (WalkMoveFingerIdx >= 0 || WalkLookFingerIdx >= 0);
		}
	} while (false);
#endif

	// Continuous fire while Fire UButton is held (desktop/fallback).
	if (bFireHeld && bDrivingMode)
	{
		if (APHCDuneBuggyPawn* Buggy = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
		{
			Buggy->FireMissile();
		}
		else if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle))
		{
			Raptor->FireRocket();
		}
		else if (APHCTankPawn* Tank = Cast<APHCTankPawn>(DrivenVehicle))
		{
			Tank->FireCannon();
		}
	}
}

// ---------------------------------------------------------------------------
// Driving mode
// ---------------------------------------------------------------------------

void UPHCCityHUD::SetDrivingMode(bool bDriving, APawn* Vehicle)
{
	bDrivingMode = bDriving;
	DrivenVehicle = Vehicle;

	const ESlateVisibility WalkVis = bDriving ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	const ESlateVisibility DriveVis = bDriving ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	// Show fire button for dune buggy and raptor.
	if (FireButton)
	{
		const bool bShowFire = bDriving &&
			(Cast<APHCDuneBuggyPawn>(Vehicle) != nullptr || Cast<APHCRaptorPawn>(Vehicle) != nullptr || Cast<APHCTankPawn>(Vehicle) != nullptr);
		FireButton->SetVisibility(bShowFire ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Show drop bomb button only for raptor.
	if (DropBombButton)
	{
		const bool bShowDrop = bDriving && Cast<APHCRaptorPawn>(Vehicle) != nullptr;
		DropBombButton->SetVisibility(bShowDrop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Toggle on-foot controls.
	if (CarButton) { CarButton->SetVisibility(WalkVis); }
	if (WeaponsButton) { WeaponsButton->SetVisibility(WalkVis); }
	if (WeaponFireButton)
	{
		// Hide fire button while driving; restore if weapon is equipped on exit
		if (bDriving)
		{
			WeaponFireButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (EquippedWeaponType >= 0)
		{
			WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
		}
	}
	if (HostButton) { HostButton->SetVisibility(WalkVis); }
	if (JoinButton) { JoinButton->SetVisibility(WalkVis); }
#if PLATFORM_IOS || PLATFORM_ANDROID
	// Jump/Sprint/Interact buttons removed.
	if (WalkMovePanel) { WalkMovePanel->SetVisibility(WalkVis); }
	if (WalkLookPanel) { WalkLookPanel->SetVisibility(WalkVis); }

	// Toggle driving controls.
	if (SteeringPanel) { SteeringPanel->SetVisibility(DriveVis); }
	if (GasPedalPanel) { GasPedalPanel->SetVisibility(DriveVis); }
	if (BrakePedalPanel) { BrakePedalPanel->SetVisibility(DriveVis); }
	if (ExitVehicleButton) { ExitVehicleButton->SetVisibility(DriveVis); }

	// Reset walking touch state.
	WalkMoveFingerIdx = -1;
	WalkLookFingerIdx = -1;
	WalkMoveOrigin = FVector2D::ZeroVector;
	WalkLookPrev = FVector2D::ZeroVector;
#endif

	// Zero out vehicle input when exiting.
	if (!bDriving && Vehicle)
	{
		if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(Vehicle))
		{
			V->ThrottleInput = 0.f;
			V->SteeringInput = 0.f;
		}
		else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(Vehicle))
		{
			B->ThrottleInput = 0.f;
			B->SteeringInput = 0.f;
		}
		else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(Vehicle))
		{
			R->ThrottleInput = 0.f;
			R->SteeringInput = 0.f;
			R->PitchInput = 0.f;
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Driving mode %s."),
		bDriving ? TEXT("ON") : TEXT("OFF"));
}

// ---------------------------------------------------------------------------
// Currency display
// ---------------------------------------------------------------------------

void UPHCCityHUD::UpdateCurrencyDisplay(int32 Coins, int32 Gems)
{
	if (CoinText)
	{
		CoinText->SetText(FText::FromString(FString::Printf(TEXT("Coins: %d"), Coins)));
	}
	if (GemText)
	{
		GemText->SetText(FText::FromString(FString::Printf(TEXT("Gems: %d"), Gems)));
	}
}

// ---------------------------------------------------------------------------
// Menu overlay
// ---------------------------------------------------------------------------

void UPHCCityHUD::OpenMenu()
{
	if (bMenuOpen)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC || !MainHUDClass)
	{
		return;
	}

	MenuOverlay = CreateWidget<UPHCMainHUD>(PC, MainHUDClass);
	if (MenuOverlay)
	{
		MenuOverlay->AddToViewport(50); // Above the city HUD, below popups
		bMenuOpen = true;

		// Switch to Game+UI input so the menu is interactable while world is visible.
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Menu opened."));
	}
}

void UPHCCityHUD::CloseMenu()
{
	if (!bMenuOpen || !MenuOverlay)
	{
		return;
	}

	MenuOverlay->RemoveFromParent();
	MenuOverlay = nullptr;
	bMenuOpen = false;

	// Restore game+UI input mode for the 3D world.
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Menu closed."));
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnMenuButtonClicked()
{
	if (bMenuOpen)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

void UPHCCityHUD::OnCloseMenuButtonClicked()
{
	CloseMenu();
}

void UPHCCityHUD::OnFireButtonPressed()
{
	bFireHeld = true;
	// Fire immediately on press
	if (APHCDuneBuggyPawn* Buggy = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
	{
		Buggy->FireMissile();
	}
	else if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle))
	{
		Raptor->FireRocket();
	}
}

void UPHCCityHUD::OnFireButtonReleased()
{
	bFireHeld = false;
}

void UPHCCityHUD::OnCarButtonClicked()
{
	ShowVehicleSelection();
}

void UPHCCityHUD::ShowVehicleSelection()
{
	if (VehicleSelectOverlay)
	{
		VehicleSelectOverlay->SetVisibility(ESlateVisibility::Visible);
		bVehicleSelectOpen = true;
	}
}

void UPHCCityHUD::HideVehicleSelection()
{
	if (VehicleSelectOverlay)
	{
		VehicleSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
		bVehicleSelectOpen = false;
	}
}

void UPHCCityHUD::OnSelectAventador()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->SpawnAndEnterVehicle(0);
		SetDrivingMode(true, PC->GetPawn());
	}
}

void UPHCCityHUD::OnSelectBuggy()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->SpawnAndEnterVehicle(1);
		SetDrivingMode(true, PC->GetPawn());
	}
}

void UPHCCityHUD::OnSelectRaptor()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->SpawnAndEnterVehicle(2);
		SetDrivingMode(true, PC->GetPawn());
	}
}

void UPHCCityHUD::OnSelectJetpack()
{
	HideVehicleSelection();
	APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer());
	if (!PC) return;
	APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn());
	if (!Char) return;

	if (Char->bJetpackEquipped)
		PC->UnequipJetpack();
	else
		PC->EquipJetpack();
}

void UPHCCityHUD::OnSelectTank()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->SpawnAndEnterVehicle(3);
		SetDrivingMode(true, PC->GetPawn());
	}
}

void UPHCCityHUD::OnDropBombClicked()
{
	if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle))
	{
		Raptor->DropBomb();
	}
}

// Jump/Sprint/Interact button callbacks removed — mobile uses touch panels.

// ---------------------------------------------------------------------------
// Driving mode button callback
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnExitVehicleClicked()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->OnVehicleExitRequested();
		SetDrivingMode(false, PC->GetPawn());
	}
}

// ---------------------------------------------------------------------------
// Multiplayer button callbacks
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnHostButtonClicked()
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		if (GI->HostGame())
		{
			// Visual feedback: update button to show hosting state.
			if (HostButton)
			{
				if (UTextBlock* Label = Cast<UTextBlock>(HostButton->GetChildAt(0)))
				{
					Label->SetText(FText::FromString(TEXT("Hosting")));
				}
				HostButton->SetIsEnabled(false);
			}
			if (JoinButton)
			{
				JoinButton->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UPHCCityHUD::OnJoinButtonClicked()
{
	if (JoinOverlay)
	{
		JoinOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void UPHCCityHUD::OnJoinConnectClicked()
{
	if (JoinOverlay)
	{
		JoinOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI && !EnteredIP.IsEmpty())
	{
		GI->JoinGame(EnteredIP);
	}
}

void UPHCCityHUD::OnJoinCancelClicked()
{
	if (JoinOverlay)
	{
		JoinOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnWeaponsButtonClicked()
{
	ShowWeaponSelection();
}

void UPHCCityHUD::OnNightButtonClicked()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;

	if (!CitySub->bNightModeLocked)
	{
		// Switch to night and freeze
		CitySub->DayNightTime = 0.75f; // midnight
		CitySub->bNightModeLocked = true;
		if (NightButtonLabel)
		{
			NightButtonLabel->SetText(FText::FromString(TEXT("Cycle")));
		}
	}
	else
	{
		// Resume normal cycle from current time
		CitySub->bNightModeLocked = false;
		if (NightButtonLabel)
		{
			NightButtonLabel->SetText(FText::FromString(TEXT("Night")));
		}
	}
}

// ---------------------------------------------------------------------------
// Disaster selection
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnDisastersButtonClicked()
{
	if (bDisasterSelectOpen)
	{
		HideDisasterSelection();
	}
	else
	{
		ShowDisasterSelection();
	}
}

void UPHCCityHUD::ShowDisasterSelection()
{
	if (DisasterSelectOverlay)
	{
		DisasterSelectOverlay->SetVisibility(ESlateVisibility::Visible);
		bDisasterSelectOpen = true;
	}
}

void UPHCCityHUD::HideDisasterSelection()
{
	if (DisasterSelectOverlay)
	{
		DisasterSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
		bDisasterSelectOpen = false;
	}
}

void UPHCCityHUD::OnSelectTornado()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::Tornado;
	CitySub->DisasterTimer = 30.f;
}

void UPHCCityHUD::OnSelectEarthquake()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::Earthquake;
	CitySub->DisasterTimer = 20.f;
}

void UPHCCityHUD::OnSelectThunderstorm()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::Thunderstorm;
	CitySub->DisasterTimer = 45.f;
}

void UPHCCityHUD::OnSelectMeteor()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::MeteorShower;
	CitySub->DisasterTimer = 40.f;
}

void UPHCCityHUD::OnStopDisaster()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::None;
	CitySub->DisasterTimer = 0.f;
}

// ---------------------------------------------------------------------------

void UPHCCityHUD::ShowWeaponSelection()
{
	if (WeaponSelectOverlay)
	{
		WeaponSelectOverlay->SetVisibility(ESlateVisibility::Visible);
		bWeaponSelectOpen = true;
	}
}

void UPHCCityHUD::HideWeaponSelection()
{
	if (WeaponSelectOverlay)
	{
		WeaponSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
		bWeaponSelectOpen = false;
	}
}

void UPHCCityHUD::OnSelectGun()
{
	HideWeaponSelection();
	EquippedWeaponType = 0;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 0;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("SPRAY")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectRPG()
{
	HideWeaponSelection();
	EquippedWeaponType = 1;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 1;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("Fire RPG")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectSword()
{
	HideWeaponSelection();
	EquippedWeaponType = 2;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 2;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("Swing")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectScythe()
{
	HideWeaponSelection();
	EquippedWeaponType = 3;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 3;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("Swing")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnWeaponFirePressed()
{
	bWeaponFireHeld = true;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (EquippedWeaponType == 0) // Minigun — hold to spray
			{
				Char->bMinigunFiring = true;
			}
			else // All other weapons — single fire on press
			{
				Char->FireWeapon();
			}
		}
	}
}

void UPHCCityHUD::OnWeaponFireReleased()
{
	bWeaponFireHeld = false;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->bMinigunFiring = false;
		}
	}
}
