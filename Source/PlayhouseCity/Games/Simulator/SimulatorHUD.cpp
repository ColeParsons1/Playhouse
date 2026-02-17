// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Simulator/SimulatorHUD.h"
#include "Games/Simulator/SimulatorGameMode.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimulatorHUD, Log, All);

// -- Colors used outside Initialize() (wrapped to avoid unity build conflicts) --
namespace
{
const FLinearColor ObstacleCarColor(0.9f, 0.15f, 0.15f, 1.f);
const FLinearColor BarrierColor(0.6f, 0.6f, 0.6f, 1.f);
const FLinearColor CoinColor(1.f, 0.85f, 0.f, 1.f);
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

bool USimulatorHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (RoadCanvas)
	{
		return true; // Already built.
	}

	// -- Colors ---------------------------------------------------------------
	const FLinearColor RoadColor(0.15f, 0.15f, 0.18f, 1.f);
	const FLinearColor GrassColor(0.08f, 0.25f, 0.08f, 1.f);
	const FLinearColor LaneLineColor(1.f, 1.f, 1.f, 0.5f);
	const FLinearColor PlayerCarColor(0.1f, 0.5f, 0.9f, 1.f);
	const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
	const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
	const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);
	const FLinearColor DarkBg(0.04f, 0.04f, 0.08f, 1.f);

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo StatsFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);

	// ---- Game area (below top bar) ----
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DrivingGameArea"));
	GameArea->SetBrushColor(GrassColor);
	GameArea->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	// Inner canvas for absolute positioning of road elements.
	UCanvasPanel* InnerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("InnerCanvas"));
	GameArea->SetContent(InnerCanvas);

	// ---- Road background (centered strip) ----
	UBorder* RoadBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("RoadBg"));
	RoadBg->SetBrushColor(RoadColor);
	UCanvasPanelSlot* RoadBgSlot = InnerCanvas->AddChildToCanvas(RoadBg);
	RoadBgSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 1.f));
	RoadBgSlot->SetAlignment(FVector2D(0.5f, 0.f));
	RoadBgSlot->SetOffsets(FMargin(-ROAD_WIDTH * 0.5f, 0.f, ROAD_WIDTH, 0.f));
	RoadBgSlot->SetAutoSize(false);

	// ---- Road canvas (for car, obstacles) ----
	RoadCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("RoadCanvas"));
	UCanvasPanelSlot* RCSlot = InnerCanvas->AddChildToCanvas(RoadCanvas);
	RCSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 1.f));
	RCSlot->SetAlignment(FVector2D(0.5f, 0.f));
	RCSlot->SetOffsets(FMargin(-ROAD_WIDTH * 0.5f, 0.f, ROAD_WIDTH, 0.f));
	RCSlot->SetAutoSize(false);

	// ---- Lane divider lines ----
	for (int32 i = 1; i < NUM_LANES; ++i)
	{
		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("LaneDivider_%d"), i));
		Divider->SetBrushColor(LaneLineColor);
		UCanvasPanelSlot* DSlot = RoadCanvas->AddChildToCanvas(Divider);
		const float X = (ROAD_WIDTH / NUM_LANES) * i - 1.f;
		DSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		DSlot->SetOffsets(FMargin(X, 0.f, 2.f, 0.f));
		DSlot->SetAutoSize(false);
		LaneDividers.Add(Divider);
	}

	// ---- Player car ----
	PlayerCarWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PlayerCar"));
	PlayerCarWidget->SetBrushColor(PlayerCarColor);
	UCanvasPanelSlot* CarSlot = RoadCanvas->AddChildToCanvas(PlayerCarWidget);
	CarSlot->SetAnchors(FAnchors(0.f, 0.85f, 0.f, 0.85f));
	CarSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CarSlot->SetOffsets(FMargin(GetLaneX(1), 0.f, CAR_WIDTH, CAR_HEIGHT));
	CarSlot->SetAutoSize(false);

	// ---- Stats overlay (top-left of road) ----
	UVerticalBox* StatsBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("DrivingStatsBox"));
	UCanvasPanelSlot* StatsSlot = InnerCanvas->AddChildToCanvas(StatsBox);
	StatsSlot->SetAnchors(FAnchors(0.f, 0.f));
	StatsSlot->SetOffsets(FMargin(10.f, 10.f, 200.f, 80.f));
	StatsSlot->SetAutoSize(true);

	SpeedText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SpeedText"));
	SpeedText->SetText(FText::FromString(TEXT("Speed: 200")));
	SpeedText->SetFont(StatsFont);
	SpeedText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* SpdSlot = StatsBox->AddChildToVerticalBox(SpeedText);
	SpdSlot->SetPadding(FMargin(0.f, 2.f));

	DistanceText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DistanceText"));
	DistanceText->SetText(FText::FromString(TEXT("Distance: 0m")));
	DistanceText->SetFont(StatsFont);
	DistanceText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* DstSlot = StatsBox->AddChildToVerticalBox(DistanceText);
	DstSlot->SetPadding(FMargin(0.f, 2.f));

	CoinsText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CoinsText"));
	CoinsText->SetText(FText::FromString(TEXT("Coins: 0")));
	CoinsText->SetFont(StatsFont);
	CoinsText->SetColorAndOpacity(FSlateColor(CoinColor));
	UVerticalBoxSlot* CnSlot = StatsBox->AddChildToVerticalBox(CoinsText);
	CnSlot->SetPadding(FMargin(0.f, 2.f));

	// ---- Control buttons (bottom) ----
	UHorizontalBox* ControlRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ControlRow"));
	UCanvasPanelSlot* CtrlSlot = InnerCanvas->AddChildToCanvas(ControlRow);
	CtrlSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
	CtrlSlot->SetAlignment(FVector2D(0.5f, 1.f));
	CtrlSlot->SetOffsets(FMargin(0.f, 0.f, 300.f, 60.f));
	CtrlSlot->SetAutoSize(false);

	auto MakeControlBtn = [&](const FName& Name, const FString& Label) -> UButton*
	{
		USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("Size"))));
		BtnSize->SetMinDesiredWidth(120.f);
		BtnSize->SetMinDesiredHeight(50.f);

		UButton* Btn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnNormal);
		Style.Hovered.TintColor = FSlateColor(BtnHover);
		Style.Pressed.TintColor = FSlateColor(BtnPress);
		Btn->SetStyle(Style);

		UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Lbl"))));
		BtnLabel->SetText(FText::FromString(Label));
		BtnLabel->SetFont(BtnFont);
		BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLabel->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLabel);

		BtnSize->AddChild(Btn);
		UHorizontalBoxSlot* Slot = ControlRow->AddChildToHorizontalBox(BtnSize);
		Slot->SetPadding(FMargin(8.f, 0.f));
		Slot->SetHorizontalAlignment(HAlign_Center);

		return Btn;
	};

	LeftButton = MakeControlBtn(TEXT("LeftBtn"), TEXT("<  LEFT"));

	// Spacer between buttons
	USpacer* BtnSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("BtnSpacer"));
	UHorizontalBoxSlot* SpSlot = ControlRow->AddChildToHorizontalBox(BtnSpacer);
	SpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	RightButton = MakeControlBtn(TEXT("RightBtn"), TEXT("RIGHT  >"));

	UE_LOG(LogSimulatorHUD, Log, TEXT("Driving HUD built: road canvas, player car, controls."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct
// ---------------------------------------------------------------------------

void USimulatorHUD::NativeConstruct()
{
	Super::NativeConstruct();

	DrivingMode = Cast<ASimulatorGameMode>(GameModeRef);

	if (LeftButton) LeftButton->OnClicked.AddDynamic(this, &USimulatorHUD::OnLeftClicked);
	if (RightButton) RightButton->OnClicked.AddDynamic(this, &USimulatorHUD::OnRightClicked);

	if (DrivingMode)
	{
		DrivingMode->OnCrash.AddDynamic(this, &USimulatorHUD::HandleCrash);
		DrivingMode->OnCoinCollected.AddDynamic(this, &USimulatorHUD::HandleCoinCollected);
	}

	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------
// NativeTick - update visuals every frame
// ---------------------------------------------------------------------------

void USimulatorHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!DrivingMode || !DrivingMode->bGameActive)
	{
		return;
	}

	// Update stats text.
	if (SpeedText)
	{
		SpeedText->SetText(FText::FromString(
			FString::Printf(TEXT("Speed: %d km/h"), FMath::FloorToInt32(DrivingMode->CurrentSpeed))));
	}
	if (DistanceText)
	{
		DistanceText->SetText(FText::FromString(
			FString::Printf(TEXT("Distance: %dm"), FMath::FloorToInt32(DrivingMode->DistanceTraveled / 10.f))));
	}
	if (CoinsText)
	{
		CoinsText->SetText(FText::FromString(
			FString::Printf(TEXT("Coins: %d"), DrivingMode->CoinsCollected)));
	}

	// Update player car position.
	UpdatePlayerPosition();

	// Update obstacle widgets.
	UpdateObstacleWidgets();
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void USimulatorHUD::OnLeftClicked()
{
	if (DrivingMode) DrivingMode->ChangeLane(-1);
}

void USimulatorHUD::OnRightClicked()
{
	if (DrivingMode) DrivingMode->ChangeLane(1);
}

void USimulatorHUD::HandleCrash()
{
	UE_LOG(LogSimulatorHUD, Log, TEXT("CRASH! Game over."));
}

void USimulatorHUD::HandleCoinCollected(int32 TotalCoins)
{
	// Visual feedback could be added here (flash, particle, etc.)
}

// ---------------------------------------------------------------------------
// Visual updates
// ---------------------------------------------------------------------------

void USimulatorHUD::UpdatePlayerPosition()
{
	if (!PlayerCarWidget || !DrivingMode)
	{
		return;
	}

	UCanvasPanelSlot* CarSlot = Cast<UCanvasPanelSlot>(PlayerCarWidget->Slot);
	if (CarSlot)
	{
		const float X = GetLaneX(DrivingMode->PlayerLane);
		CarSlot->SetOffsets(FMargin(X, 0.f, CAR_WIDTH, CAR_HEIGHT));
	}
}

void USimulatorHUD::UpdateObstacleWidgets()
{
	if (!DrivingMode || !RoadCanvas)
	{
		return;
	}

	const TArray<FDrivingObstacle>& Obs = DrivingMode->Obstacles;

	// Get the road canvas height for Y positioning.
	const FGeometry& RoadGeo = RoadCanvas->GetCachedGeometry();
	const float RoadHeight = RoadGeo.GetLocalSize().Y;

	// Hide excess widgets.
	for (int32 i = Obs.Num(); i < ObstacleWidgets.Num(); ++i)
	{
		if (ObstacleWidgets[i])
		{
			ObstacleWidgets[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update/create widgets for active obstacles.
	for (int32 i = 0; i < Obs.Num(); ++i)
	{
		const FDrivingObstacle& O = Obs[i];
		if (!O.bActive) continue;

		UBorder* Widget = GetOrCreateObstacleWidget(i);
		if (!Widget) continue;

		Widget->SetVisibility(ESlateVisibility::Visible);

		// Color by type.
		FLinearColor Color;
		switch (O.Type)
		{
		case 0: Color = ObstacleCarColor; break;
		case 1: Color = BarrierColor; break;
		case 2: Color = CoinColor; break;
		default: Color = ObstacleCarColor; break;
		}
		Widget->SetBrushColor(Color);

		// Position on road canvas.
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (Slot)
		{
			const float X = GetLaneX(O.Lane);
			const float Y = O.PositionY * RoadHeight;
			const float W = O.Type == 2 ? 30.f : OBS_WIDTH;
			const float H = O.Type == 2 ? 30.f : OBS_HEIGHT;
			Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetOffsets(FMargin(X, Y, W, H));
			Slot->SetAutoSize(false);
		}
	}
}

UBorder* USimulatorHUD::GetOrCreateObstacleWidget(int32 Index)
{
	// Grow pool if needed.
	while (ObstacleWidgets.Num() <= Index)
	{
		UBorder* NewObs = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("Obstacle_%d"), ObstacleWidgets.Num()));
		NewObs->SetBrushColor(ObstacleCarColor);
		NewObs->SetVisibility(ESlateVisibility::Collapsed);
		RoadCanvas->AddChildToCanvas(NewObs);
		ObstacleWidgets.Add(NewObs);
	}
	return ObstacleWidgets[Index];
}

float USimulatorHUD::GetLaneX(int32 Lane) const
{
	// Center of each lane within ROAD_WIDTH.
	const float LaneWidth = ROAD_WIDTH / NUM_LANES;
	return LaneWidth * Lane + LaneWidth * 0.5f;
}
