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
		CurrencySlot->SetOffsets(FMargin(-16.f, 16.f, 0.f, 0.f));
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
		// Positioned just above the gas pedal (gas pedal anchors at Y 0.5-0.75, right side)
		FireBtnSlot->SetAnchors(FAnchors(0.77f, 0.48f, 0.77f, 0.48f));
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
		MenuBtnSlot->SetOffsets(FMargin(16.f, 16.f, 80.f, 40.f));

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
		CarBtnSlot->SetOffsets(FMargin(16.f, 64.f, 80.f, 40.f));

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

		// Jump button (large, bottom-right)
		JumpButton = MakeActionButton(TEXT("JumpButton"), TEXT("Jump"), 20.f, 20.f, 80.f);

		// Sprint button (above jump)
		SprintButton = MakeActionButton(TEXT("SprintButton"), TEXT("Run"), 20.f, 120.f, 70.f);

		// Interact button (left of jump)
		InteractButton = MakeActionButton(TEXT("InteractButton"), TEXT("Use"), 120.f, 20.f, 70.f);

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
		ExitSlot->SetOffsets(FMargin(16.f, 64.f, 80.f, 40.f));

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

		VehicleSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);

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
		HostSlot->SetOffsets(FMargin(16.f, 112.f, 80.f, 40.f));

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
		JoinSlot->SetOffsets(FMargin(16.f, 160.f, 80.f, 40.f));

		// Player count display (top-right, below currency)
		PlayerCountText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("PlayerCountText"));
		PlayerCountText->SetText(FText::FromString(TEXT("Players: 1")));
		PlayerCountText->SetFont(BtnFont);
		PlayerCountText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.7f)));

		UCanvasPanelSlot* PlayerCountSlot = RootCanvas->AddChildToCanvas(PlayerCountText);
		PlayerCountSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		PlayerCountSlot->SetAlignment(FVector2D(1.f, 0.f));
		PlayerCountSlot->SetOffsets(FMargin(-16.f, 56.f, 0.f, 0.f));
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
	if (FireButton)
	{
		FireButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnFireButtonPressed);
		FireButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnFireButtonReleased);
	}
	if (DropBombButton)
	{
		DropBombButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDropBombClicked);
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
	if (JumpButton)
	{
		JumpButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnJumpButtonPressed);
		JumpButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnJumpButtonReleased);
	}
	if (SprintButton)
	{
		SprintButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnSprintButtonPressed);
		SprintButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnSprintButtonReleased);
	}
	if (InteractButton)
	{
		InteractButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnInteractButtonClicked);
	}
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
	if (JumpButton)
	{
		JumpButton->OnPressed.RemoveAll(this);
		JumpButton->OnReleased.RemoveAll(this);
	}
	if (SprintButton)
	{
		SprintButton->OnPressed.RemoveAll(this);
		SprintButton->OnReleased.RemoveAll(this);
	}
	if (InteractButton)
	{
		InteractButton->OnClicked.RemoveAll(this);
	}
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

	// Update live location monitor
	if (LocationText)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* P = PC->GetPawn())
			{
				const FVector Loc = P->GetActorLocation();
				LocationText->SetText(FText::FromString(
					FString::Printf(TEXT("X: %.0f  Y: %.0f  Z: %.0f"), Loc.X, Loc.Y, Loc.Z)));
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
	if (!bDrivingMode || !DrivenVehicle)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	int32 VPWidth = 0;
	int32 VPHeight = 0;
	PC->GetViewportSize(VPWidth, VPHeight);
	if (VPWidth == 0 || VPHeight == 0)
	{
		return;
	}

	const float ScreenHalfX = VPWidth * 0.5f;
	const float ScreenHalfY = VPHeight * 0.5f;

	float NewSteering = 0.f;
	float NewThrottle = 0.f;
	float NewPitch = 0.f;
	bool bFireTouched = false;

	// Fire button zone: centered at 77% X, from (48%-168px) to 48% Y.
	const float FireCenterX = VPWidth * 0.77f;
	const float FireBtnRadius = 84.f; // half of 168
	const float FireBottomY = VPHeight * 0.48f;
	const float FireTopY = FireBottomY - FireBtnRadius * 2.f;

	// Read all active touch points.
	// Left half of screen = steering (touch X position maps to steering angle).
	// Right half, upper region = gas, lower region = brake.
	// Fire button zone = upper right (above gas pedal).
	for (int32 i = 0; i < 5; ++i)
	{
		float TouchX = 0.f;
		float TouchY = 0.f;
		bool bPressed = false;
		PC->GetInputTouchState(static_cast<ETouchIndex::Type>(i), TouchX, TouchY, bPressed);

		if (!bPressed)
		{
			continue;
		}

		// Check fire button zone first (circle-ish region).
		if (TouchX >= FireCenterX - FireBtnRadius && TouchX <= FireCenterX + FireBtnRadius &&
			TouchY >= FireTopY && TouchY <= FireBottomY)
		{
			bFireTouched = true;
			continue; // Don't let this touch also trigger gas/brake.
		}

		if (TouchX < ScreenHalfX && TouchY > ScreenHalfY)
		{
			// Left side, bottom half = steering zone.
			// Map [0 .. ScreenHalfX] to [-1 .. +1].
			NewSteering = (TouchX / ScreenHalfX) * 2.f - 1.f;
			NewSteering = FMath::Clamp(NewSteering, -1.f, 1.f);

			// For Raptor: vertical position in this zone = pitch.
			// Map [ScreenHalfY .. VPHeight] to [+1 .. -1] (top = climb, bottom = dive).
			const float NormY = (TouchY - ScreenHalfY) / ScreenHalfY; // 0..1 (top..bottom)
			NewPitch = 1.f - NormY * 2.f; // +1 (top) .. -1 (bottom)
			NewPitch = FMath::Clamp(NewPitch, -1.f, 1.f);
		}
		else if (TouchX >= ScreenHalfX && TouchY > ScreenHalfY)
		{
			// Right side, bottom half = gas/brake.
			// Upper portion (50%-75% of screen height) = gas.
			// Lower portion (75%-100%) = brake.
			const float ThresholdY = VPHeight * 0.75f;
			if (TouchY < ThresholdY)
			{
				NewThrottle = 1.f;
			}
			else
			{
				NewThrottle = -1.f;
			}
		}
	}

	if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(DrivenVehicle))
	{
		V->SteeringInput = NewSteering;
		V->ThrottleInput = NewThrottle;
		if (!V->HasAuthority())
		{
			V->ServerSetSteering(NewSteering);
			V->ServerSetThrottle(NewThrottle);
		}
	}
	else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
	{
		B->SteeringInput = NewSteering;
		B->ThrottleInput = NewThrottle;
		if (!B->HasAuthority())
		{
			B->ServerSetSteering(NewSteering);
			B->ServerSetThrottle(NewThrottle);
		}
	}
	else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(DrivenVehicle))
	{
		R->SteeringInput = NewSteering;
		R->ThrottleInput = NewThrottle;
		R->PitchInput = NewPitch;
		if (!R->HasAuthority())
		{
			R->ServerSetSteering(NewSteering);
			R->ServerSetThrottle(NewThrottle);
			R->ServerSetPitch(NewPitch);
		}
	}

	// Fire missile/rocket while touching fire zone (mobile touch-based detection).
	if (bFireTouched && bDrivingMode)
	{
		if (APHCDuneBuggyPawn* Buggy = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
		{
			Buggy->FireMissile();
		}
		else if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle))
		{
			Raptor->FireRocket();
		}
	}
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
			(Cast<APHCDuneBuggyPawn>(Vehicle) != nullptr || Cast<APHCRaptorPawn>(Vehicle) != nullptr);
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
	if (HostButton) { HostButton->SetVisibility(WalkVis); }
	if (JoinButton) { JoinButton->SetVisibility(WalkVis); }
#if PLATFORM_IOS || PLATFORM_ANDROID
	if (JumpButton) { JumpButton->SetVisibility(WalkVis); }
	if (SprintButton) { SprintButton->SetVisibility(WalkVis); }
	if (InteractButton) { InteractButton->SetVisibility(WalkVis); }

	// Toggle driving controls.
	if (SteeringPanel) { SteeringPanel->SetVisibility(DriveVis); }
	if (GasPedalPanel) { GasPedalPanel->SetVisibility(DriveVis); }
	if (BrakePedalPanel) { BrakePedalPanel->SetVisibility(DriveVis); }
	if (ExitVehicleButton) { ExitVehicleButton->SetVisibility(DriveVis); }
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

void UPHCCityHUD::OnDropBombClicked()
{
	if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle))
	{
		Raptor->DropBomb();
	}
}

// ---------------------------------------------------------------------------
// Mobile on-foot button callbacks
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnJumpButtonPressed()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->Jump();
		}
	}
}

void UPHCCityHUD::OnJumpButtonReleased()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->StopJumping();
		}
	}
}

void UPHCCityHUD::OnSprintButtonPressed()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->bIsSprinting = true;
			Char->GetCharacterMovement()->MaxWalkSpeed = Char->SprintSpeed;
		}
	}
}

void UPHCCityHUD::OnSprintButtonReleased()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->bIsSprinting = false;
			Char->GetCharacterMovement()->MaxWalkSpeed = Char->WalkSpeed;
		}
	}
}

void UPHCCityHUD::OnInteractButtonClicked()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->EnterNearestVehicle();
	}
}

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
