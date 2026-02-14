// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/TempleRun/TempleRunHUD.h"
#include "Games/TempleRun/TempleRunGameMode.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogTempleRunHUD, Log, All);

// -- Color palette (anonymous namespace to avoid unity build conflicts) ------
namespace
{
const FLinearColor RunnerColor(0.15f, 0.75f, 0.20f, 1.f);       // Green runner
const FLinearColor ObsLowColor(0.85f, 0.30f, 0.10f, 1.f);       // Orange-red (jump over)
const FLinearColor ObsHighColor(0.70f, 0.10f, 0.70f, 1.f);      // Purple (slide under)
const FLinearColor ObsBlockColor(0.50f, 0.50f, 0.50f, 1.f);     // Gray (lane block)
const FLinearColor CoinColor(1.f, 0.85f, 0.f, 1.f);             // Gold coins
}

// ---------------------------------------------------------------------------
// Initialize - build the widget tree programmatically
// ---------------------------------------------------------------------------

bool UTempleRunHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// -- Color palette --------------------------------------------------------
	const FLinearColor TemplePathColor(0.35f, 0.22f, 0.10f, 1.f);   // Stone/brown
	const FLinearColor TempleEdgeColor(0.25f, 0.15f, 0.05f, 1.f);   // Darker brown edges
	const FLinearColor JungleColor(0.06f, 0.20f, 0.06f, 1.f);       // Dark green jungle
	const FLinearColor LaneLineColor(0.55f, 0.40f, 0.20f, 0.5f);    // Faded stone lines
	const FLinearColor DashColor(0.50f, 0.35f, 0.15f, 0.4f);        // Road dash marks
	const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
	const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
	const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);
	const FLinearColor JumpBtnNormal(0.1f, 0.55f, 0.3f, 1.f);
	const FLinearColor JumpBtnHover(0.15f, 0.7f, 0.4f, 1.f);
	const FLinearColor SlideBtnNormal(0.55f, 0.35f, 0.1f, 1.f);
	const FLinearColor SlideBtnHover(0.7f, 0.45f, 0.15f, 1.f);
	const FLinearColor StatsOverlayBg(0.04f, 0.04f, 0.08f, 0.7f);

	// Guard: if already built, skip.
	if (GameCanvas)
	{
		return true;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo StatsFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo StatsLabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

	// ========================================================================
	// GAME AREA (below the top bar at Y=50)
	// ========================================================================

	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TempleGameArea"));
	GameArea->SetBrushColor(JungleColor);
	GameArea->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* GameAreaSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameAreaSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameAreaSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	// Inner canvas for absolute positioning of all game elements.
	UCanvasPanel* InnerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("TempleInnerCanvas"));
	GameArea->SetContent(InnerCanvas);

	// ========================================================================
	// TEMPLE PATH BACKGROUND (centered vertical strip - stone/brown)
	// ========================================================================

	UBorder* PathBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TemplePathBg"));
	PathBg->SetBrushColor(TemplePathColor);
	UCanvasPanelSlot* PathBgSlot = InnerCanvas->AddChildToCanvas(PathBg);
	PathBgSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 1.f));
	PathBgSlot->SetAlignment(FVector2D(0.5f, 0.f));
	PathBgSlot->SetOffsets(FMargin(-ROAD_WIDTH * 0.5f, 0.f, ROAD_WIDTH, 0.f));
	PathBgSlot->SetAutoSize(false);

	// Left edge border (darker brown)
	UBorder* LeftEdge = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("LeftEdge"));
	LeftEdge->SetBrushColor(TempleEdgeColor);
	UCanvasPanelSlot* LESlot = InnerCanvas->AddChildToCanvas(LeftEdge);
	LESlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 1.f));
	LESlot->SetAlignment(FVector2D(0.5f, 0.f));
	LESlot->SetOffsets(FMargin(-ROAD_WIDTH * 0.5f - 6.f, 0.f, 6.f, 0.f));
	LESlot->SetAutoSize(false);

	// Right edge border (darker brown)
	UBorder* RightEdge = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("RightEdge"));
	RightEdge->SetBrushColor(TempleEdgeColor);
	UCanvasPanelSlot* RESlot = InnerCanvas->AddChildToCanvas(RightEdge);
	RESlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 1.f));
	RESlot->SetAlignment(FVector2D(0.5f, 0.f));
	RESlot->SetOffsets(FMargin(ROAD_WIDTH * 0.5f, 0.f, 6.f, 0.f));
	RESlot->SetAutoSize(false);

	// ========================================================================
	// GAME CANVAS (for runner, obstacles, coins - overlays the path)
	// ========================================================================

	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));
	UCanvasPanelSlot* GCSlot = InnerCanvas->AddChildToCanvas(GameCanvas);
	GCSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 1.f));
	GCSlot->SetAlignment(FVector2D(0.5f, 0.f));
	GCSlot->SetOffsets(FMargin(-ROAD_WIDTH * 0.5f, 0.f, ROAD_WIDTH, 0.f));
	GCSlot->SetAutoSize(false);

	// ========================================================================
	// LANE DIVIDER LINES (stone-colored, between lanes)
	// ========================================================================

	for (int32 i = 1; i < NUM_LANES; ++i)
	{
		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("LaneDivider_%d"), i));
		Divider->SetBrushColor(LaneLineColor);
		UCanvasPanelSlot* DSlot = GameCanvas->AddChildToCanvas(Divider);
		const float X = (ROAD_WIDTH / NUM_LANES) * i - 1.f;
		DSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		DSlot->SetOffsets(FMargin(X, 0.f, 2.f, 0.f));
		DSlot->SetAutoSize(false);
		LaneDividers.Add(Divider);
	}

	// ========================================================================
	// SCROLLING ROAD DASHES (center lane marks that scroll downward)
	// ========================================================================

	for (int32 i = 0; i < NUM_ROAD_DASHES; ++i)
	{
		UBorder* Dash = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("RoadDash_%d"), i));
		Dash->SetBrushColor(DashColor);
		UCanvasPanelSlot* DashSlot = GameCanvas->AddChildToCanvas(Dash);
		const float CenterX = ROAD_WIDTH * 0.5f - 2.f;
		const float Y = i * ROAD_DASH_SPACING;
		DashSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		DashSlot->SetOffsets(FMargin(CenterX, Y, 4.f, ROAD_DASH_HEIGHT));
		DashSlot->SetAutoSize(false);
		RoadDashes.Add(Dash);
	}

	// ========================================================================
	// RUNNER CHARACTER (green block near the bottom of the road)
	// ========================================================================

	RunnerWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("RunnerWidget"));
	RunnerWidget->SetBrushColor(RunnerColor);
	UCanvasPanelSlot* RunnerSlot = GameCanvas->AddChildToCanvas(RunnerWidget);
	RunnerSlot->SetAnchors(FAnchors(0.f, 0.85f, 0.f, 0.85f));
	RunnerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	RunnerSlot->SetOffsets(FMargin(LaneToCanvasX(1), 0.f, RUNNER_WIDTH, RUNNER_HEIGHT));
	RunnerSlot->SetAutoSize(false);

	// ========================================================================
	// STATS OVERLAY (top-left of game area: distance + coins)
	// ========================================================================

	UBorder* StatsBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TempleStatsBg"));
	StatsBg->SetBrushColor(StatsOverlayBg);
	StatsBg->SetPadding(FMargin(8.f, 4.f));

	UCanvasPanelSlot* StatsBgSlot = InnerCanvas->AddChildToCanvas(StatsBg);
	StatsBgSlot->SetAnchors(FAnchors(0.f, 0.f));
	StatsBgSlot->SetOffsets(FMargin(10.f, 10.f, 180.f, 70.f));
	StatsBgSlot->SetAutoSize(true);

	UVerticalBox* StatsBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("TempleStatsBox"));
	StatsBg->SetContent(StatsBox);

	// Distance text
	DistanceText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DistanceText"));
	DistanceText->SetText(FText::FromString(TEXT("0 m")));
	DistanceText->SetFont(StatsFont);
	DistanceText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* DstSlot = StatsBox->AddChildToVerticalBox(DistanceText);
	DstSlot->SetPadding(FMargin(0.f, 2.f));

	// Coins text
	UHorizontalBox* CoinRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CoinRow"));
	UVerticalBoxSlot* CRSlot = StatsBox->AddChildToVerticalBox(CoinRow);
	CRSlot->SetPadding(FMargin(0.f, 2.f));

	UTextBlock* CoinLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CoinLabel"));
	CoinLabel->SetText(FText::FromString(TEXT("Coins: ")));
	CoinLabel->SetFont(StatsLabelFont);
	CoinLabel->SetColorAndOpacity(FSlateColor(CoinColor));
	UHorizontalBoxSlot* CLSlot = CoinRow->AddChildToHorizontalBox(CoinLabel);
	CLSlot->SetVerticalAlignment(VAlign_Center);

	CoinCountText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CoinCountText"));
	CoinCountText->SetText(FText::FromString(TEXT("0")));
	CoinCountText->SetFont(StatsFont);
	CoinCountText->SetColorAndOpacity(FSlateColor(CoinColor));
	UHorizontalBoxSlot* CCSlot = CoinRow->AddChildToHorizontalBox(CoinCountText);
	CCSlot->SetVerticalAlignment(VAlign_Center);

	// ========================================================================
	// CONTROL BUTTONS (bottom of screen: LEFT | JUMP | SLIDE | RIGHT)
	// ========================================================================

	UHorizontalBox* ControlRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("TempleControlRow"));
	UCanvasPanelSlot* CtrlSlot = InnerCanvas->AddChildToCanvas(ControlRow);
	CtrlSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
	CtrlSlot->SetAlignment(FVector2D(0.5f, 1.f));
	CtrlSlot->SetOffsets(FMargin(0.f, 0.f, 460.f, 60.f));
	CtrlSlot->SetAutoSize(false);

	// Helper lambda to create styled control buttons.
	auto MakeControlBtn = [&](const FName& Name, const FString& Label,
		FLinearColor NormalCol, FLinearColor HoverCol, FLinearColor PressCol,
		float MinWidth) -> UButton*
	{
		USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), FName(*(Name.ToString() + TEXT("Size"))));
		BtnSize->SetMinDesiredWidth(MinWidth);
		BtnSize->SetMinDesiredHeight(50.f);

		UButton* Btn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(NormalCol);
		Style.Hovered.TintColor = FSlateColor(HoverCol);
		Style.Pressed.TintColor = FSlateColor(PressCol);
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
		Slot->SetPadding(FMargin(4.f, 0.f));
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetVerticalAlignment(VAlign_Center);

		return Btn;
	};

	// Left button
	LeftButton = MakeControlBtn(TEXT("LeftBtn"), TEXT("<  LEFT"),
		BtnNormal, BtnHover, BtnPress, 100.f);

	// Jump button
	JumpButton = MakeControlBtn(TEXT("JumpBtn"), TEXT("JUMP"),
		JumpBtnNormal, JumpBtnHover, BtnPress, 90.f);

	// Slide button
	SlideButton = MakeControlBtn(TEXT("SlideBtn"), TEXT("SLIDE"),
		SlideBtnNormal, SlideBtnHover, BtnPress, 90.f);

	// Right button
	RightButton = MakeControlBtn(TEXT("RightBtn"), TEXT("RIGHT  >"),
		BtnNormal, BtnHover, BtnPress, 100.f);

	// ========================================================================
	// SWIPE DETECTOR (transparent overlay on the road area for touch input)
	// ========================================================================

	SwipeDetector = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SwipeDetector"));
	SwipeDetector->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	UCanvasPanelSlot* SwipeSlot = InnerCanvas->AddChildToCanvas(SwipeDetector);
	SwipeSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.85f));
	SwipeSlot->SetAlignment(FVector2D(0.5f, 0.f));
	SwipeSlot->SetOffsets(FMargin(-ROAD_WIDTH * 0.5f, 0.f, ROAD_WIDTH, 0.f));
	SwipeSlot->SetAutoSize(false);

	UE_LOG(LogTempleRunHUD, Log,
		TEXT("Temple Run HUD built: path canvas, runner, lane dividers, "
		     "road dashes, stats overlay, 4 control buttons."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct
// ---------------------------------------------------------------------------

void UTempleRunHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Acquire the runner game mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	RunnerMode = Cast<ATempleRunGameMode>(GameModeRef);

	// Initialize lane position to center.
	CurrentLaneX = LaneToCanvasX(1);
	TargetLaneX = CurrentLaneX;
	RunnerBaseY = 0.f;
	RunnerVerticalOffset = 0.f;
	RoadDashOffset = 0.f;
	LastDistanceMilestone = 0.f;

	// Wire up control button delegates.
	if (LeftButton)
	{
		LeftButton->OnClicked.AddDynamic(this, &UTempleRunHUD::OnLeftClicked);
	}
	if (RightButton)
	{
		RightButton->OnClicked.AddDynamic(this, &UTempleRunHUD::OnRightClicked);
	}
	if (JumpButton)
	{
		JumpButton->OnClicked.AddDynamic(this, &UTempleRunHUD::OnJumpClicked);
	}
	if (SlideButton)
	{
		SlideButton->OnClicked.AddDynamic(this, &UTempleRunHUD::OnSlideClicked);
	}

	// Show the runner at start.
	if (RunnerWidget)
	{
		RunnerWidget->SetVisibility(ESlateVisibility::Visible);
	}
	if (RunnerImage)
	{
		RunnerImage->SetVisibility(ESlateVisibility::Visible);
	}

	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------
// NativeTick
// ---------------------------------------------------------------------------

void UTempleRunHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!RunnerMode || !RunnerMode->bGameActive)
	{
		return;
	}

	// -- Update HUD text ------------------------------------------------------

	if (DistanceText)
	{
		const int32 DisplayDistance = FMath::FloorToInt32(RunnerMode->DistanceTraveled);
		DistanceText->SetText(FText::FromString(
			FString::Printf(TEXT("%d m"), DisplayDistance)));
	}

	if (CoinCountText)
	{
		CoinCountText->SetText(FText::FromString(
			FString::Printf(TEXT("%d"), RunnerMode->CoinsCollected)));
	}

	// -- Update runner position -----------------------------------------------

	UpdateRunnerLane(RunnerMode->CurrentLane);

	// Smoothly interpolate horizontal position.
	CurrentLaneX = FMath::FInterpTo(CurrentLaneX, TargetLaneX, InDeltaTime, LaneLerpSpeed);

	// Handle jump/slide visual offset.
	if (RunnerMode->bIsJumping)
	{
		// Parabolic jump arc: peak at midpoint of jump duration.
		const float JumpProgress = 1.f - (RunnerMode->JumpTimer / RunnerMode->JumpDuration);
		RunnerVerticalOffset = -80.f * FMath::Sin(JumpProgress * PI);
	}
	else if (RunnerMode->bIsSliding)
	{
		RunnerVerticalOffset = 20.f; // Slight downward shift for slide.
	}
	else
	{
		RunnerVerticalOffset = FMath::FInterpTo(RunnerVerticalOffset, 0.f, InDeltaTime, 10.f);
	}

	// Position the runner widget on the canvas.
	if (RunnerWidget)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(RunnerWidget->Slot);
		if (Slot)
		{
			Slot->SetOffsets(FMargin(CurrentLaneX, RunnerVerticalOffset, RUNNER_WIDTH, RUNNER_HEIGHT));

			// Scale the runner during slide (flattened).
			if (RunnerMode->bIsSliding)
			{
				RunnerWidget->SetRenderTransform(FWidgetTransform(
					FVector2D::ZeroVector, FVector2D(1.2f, 0.5f), FVector2D::ZeroVector, 0.f));
			}
			else
			{
				RunnerWidget->SetRenderTransform(FWidgetTransform());
			}
		}

		// Color feedback for actions.
		if (RunnerMode->bIsJumping)
		{
			RunnerWidget->SetBrushColor(FLinearColor(0.4f, 1.f, 0.4f, 1.f));
		}
		else if (RunnerMode->bIsSliding)
		{
			RunnerWidget->SetBrushColor(FLinearColor(0.1f, 0.5f, 0.15f, 1.f));
		}
		else
		{
			RunnerWidget->SetBrushColor(RunnerColor);
		}
	}

	// Position the legacy runner image if present.
	if (RunnerImage)
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(RunnerImage->Slot))
		{
			Slot->SetPosition(FVector2D(CurrentLaneX, RunnerBaseY + RunnerVerticalOffset));
		}
	}

	// -- Update lane indicators -----------------------------------------------

	auto SetLaneHighlight = [this](UImage* Indicator, bool bActive)
	{
		if (Indicator)
		{
			Indicator->SetColorAndOpacity(bActive
				? FLinearColor(1.f, 0.9f, 0.2f, 0.8f)
				: FLinearColor(1.f, 1.f, 1.f, 0.3f));
		}
	};

	SetLaneHighlight(Lane0Indicator, RunnerMode->CurrentLane == 0);
	SetLaneHighlight(Lane1Indicator, RunnerMode->CurrentLane == 1);
	SetLaneHighlight(Lane2Indicator, RunnerMode->CurrentLane == 2);

	// -- Scrolling road dashes ------------------------------------------------

	UpdateRoadDashes(InDeltaTime);

	// -- Update obstacle widgets from game state ------------------------------

	UpdateObstacleWidgets();

	// -- Distance milestones --------------------------------------------------

	const float MilestoneInterval = 500.f;
	const float CurrentDistance = RunnerMode->DistanceTraveled;
	if (CurrentDistance - LastDistanceMilestone >= MilestoneInterval)
	{
		LastDistanceMilestone = FMath::FloorToFloat(CurrentDistance / MilestoneInterval) * MilestoneInterval;
		ShowDistanceMarker(LastDistanceMilestone);
	}
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UTempleRunHUD::OnLeftClicked()
{
	if (RunnerMode && RunnerMode->bGameActive)
	{
		RunnerMode->SwipeLane(-1);
	}
}

void UTempleRunHUD::OnRightClicked()
{
	if (RunnerMode && RunnerMode->bGameActive)
	{
		RunnerMode->SwipeLane(1);
	}
}

void UTempleRunHUD::OnJumpClicked()
{
	if (RunnerMode && RunnerMode->bGameActive)
	{
		RunnerMode->Jump();
		ShowJumpAnimation();
	}
}

void UTempleRunHUD::OnSlideClicked()
{
	if (RunnerMode && RunnerMode->bGameActive)
	{
		RunnerMode->Slide();
		ShowSlideAnimation();
	}
}

// ---------------------------------------------------------------------------
// Touch / swipe input
// ---------------------------------------------------------------------------

FReply UTempleRunHUD::NativeOnTouchStarted(const FGeometry& InGeometry,
                                            const FPointerEvent& InTouchEvent)
{
	TouchStartPosition = InTouchEvent.GetScreenSpacePosition();
	bTouchActive = true;
	return FReply::Handled();
}

FReply UTempleRunHUD::NativeOnTouchMoved(const FGeometry& InGeometry,
                                          const FPointerEvent& InTouchEvent)
{
	// We process swipe on touch end for cleaner gesture detection.
	return FReply::Handled();
}

FReply UTempleRunHUD::NativeOnTouchEnded(const FGeometry& InGeometry,
                                          const FPointerEvent& InTouchEvent)
{
	if (!bTouchActive)
	{
		return FReply::Handled();
	}

	bTouchActive = false;
	const FVector2D TouchEndPosition = InTouchEvent.GetScreenSpacePosition();
	const FVector2D SwipeDir = ParseSwipeDirection(TouchStartPosition, TouchEndPosition);

	if (!SwipeDir.IsNearlyZero())
	{
		OnSwipeDetected(SwipeDir);
	}

	return FReply::Handled();
}

FVector2D UTempleRunHUD::ParseSwipeDirection(FVector2D Start, FVector2D End) const
{
	const FVector2D Delta = End - Start;
	const float Distance = Delta.Size();

	if (Distance < MinSwipeDistance)
	{
		return FVector2D::ZeroVector;
	}

	// Determine primary axis (horizontal vs vertical).
	const float AbsX = FMath::Abs(Delta.X);
	const float AbsY = FMath::Abs(Delta.Y);

	if (AbsX > AbsY)
	{
		// Horizontal swipe: left or right.
		return FVector2D(Delta.X > 0.f ? 1.f : -1.f, 0.f);
	}
	else
	{
		// Vertical swipe: up (negative Y in screen space) or down.
		return FVector2D(0.f, Delta.Y > 0.f ? 1.f : -1.f);
	}
}

void UTempleRunHUD::OnSwipeDetected(FVector2D Direction)
{
	if (!RunnerMode || !RunnerMode->bGameActive)
	{
		return;
	}

	if (FMath::Abs(Direction.X) > FMath::Abs(Direction.Y))
	{
		// Horizontal swipe: lane change.
		const int32 LaneDir = (Direction.X > 0.f) ? 1 : -1;
		RunnerMode->SwipeLane(LaneDir);
	}
	else
	{
		if (Direction.Y < 0.f)
		{
			// Swipe up: jump.
			RunnerMode->Jump();
			ShowJumpAnimation();
		}
		else
		{
			// Swipe down: slide.
			RunnerMode->Slide();
			ShowSlideAnimation();
		}
	}
}

// ---------------------------------------------------------------------------
// Visual updates
// ---------------------------------------------------------------------------

void UTempleRunHUD::UpdateRunnerLane(int32 Lane)
{
	TargetLaneX = LaneToCanvasX(Lane);
}

void UTempleRunHUD::ShowJumpAnimation()
{
	// The actual jump offset is driven by NativeTick using the game mode's
	// JumpTimer. This function can trigger additional VFX if desired.
	if (RunnerWidget)
	{
		RunnerWidget->SetBrushColor(FLinearColor(0.4f, 1.f, 0.4f, 1.f));
	}
	if (RunnerImage)
	{
		RunnerImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 0.7f, 1.f));
	}
}

void UTempleRunHUD::ShowSlideAnimation()
{
	// Slide visual feedback. Scale compression is driven in NativeTick.
	if (RunnerWidget)
	{
		RunnerWidget->SetBrushColor(FLinearColor(0.1f, 0.5f, 0.15f, 1.f));
	}
	if (RunnerImage)
	{
		RunnerImage->SetColorAndOpacity(FLinearColor(0.7f, 0.9f, 1.f, 1.f));
	}
}

void UTempleRunHUD::SpawnObstacleVisual(int32 Lane, EPHCObstacleType Type, float ZPosition)
{
	if (!GameCanvas)
	{
		return;
	}

	UImage* ObsImage = NewObject<UImage>(this);
	if (!ObsImage)
	{
		return;
	}

	// Set color based on obstacle type.
	FLinearColor ObsColor;
	switch (Type)
	{
	case EPHCObstacleType::Low:
		ObsColor = ObsLowColor;
		break;
	case EPHCObstacleType::High:
		ObsColor = ObsHighColor;
		break;
	case EPHCObstacleType::BlockLeft:
	case EPHCObstacleType::BlockCenter:
	case EPHCObstacleType::BlockRight:
		ObsColor = ObsBlockColor;
		break;
	}
	ObsImage->SetColorAndOpacity(ObsColor);

	UCanvasPanelSlot* Slot = GameCanvas->AddChildToCanvas(ObsImage);
	if (Slot)
	{
		Slot->SetSize(FVector2D(50.f, 30.f));
		Slot->SetPosition(FVector2D(LaneToCanvasX(Lane), ZToCanvasY(ZPosition)));
	}

	ObstacleVisuals.Add(ObsImage);
}

void UTempleRunHUD::SpawnCoinVisual(int32 Lane, float ZPosition)
{
	if (!GameCanvas)
	{
		return;
	}

	UImage* CoinImage = NewObject<UImage>(this);
	if (!CoinImage)
	{
		return;
	}

	CoinImage->SetColorAndOpacity(CoinColor);

	UCanvasPanelSlot* Slot = GameCanvas->AddChildToCanvas(CoinImage);
	if (Slot)
	{
		Slot->SetSize(FVector2D(20.f, 20.f));
		Slot->SetPosition(FVector2D(LaneToCanvasX(Lane), ZToCanvasY(ZPosition)));
	}

	CoinVisuals.Add(CoinImage);
}

void UTempleRunHUD::RemoveVisual(int32 VisualIndex)
{
	// Remove from obstacles first, then coins.
	if (VisualIndex < ObstacleVisuals.Num())
	{
		if (ObstacleVisuals.IsValidIndex(VisualIndex) && ObstacleVisuals[VisualIndex])
		{
			ObstacleVisuals[VisualIndex]->RemoveFromParent();
			ObstacleVisuals.RemoveAt(VisualIndex);
		}
	}
	else
	{
		const int32 CoinIdx = VisualIndex - ObstacleVisuals.Num();
		if (CoinVisuals.IsValidIndex(CoinIdx) && CoinVisuals[CoinIdx])
		{
			CoinVisuals[CoinIdx]->RemoveFromParent();
			CoinVisuals.RemoveAt(CoinIdx);
		}
	}
}

void UTempleRunHUD::ShowDistanceMarker(float Distance)
{
	// Brief text flash showing the milestone. In production this would
	// use a pooled widget with an animation, but for now we update the
	// distance text with emphasis.
	if (DistanceText)
	{
		DistanceText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.9f, 0.f, 1.f)));

		// Schedule color reset after 0.5 seconds.
		if (UWorld* World = GetWorld())
		{
			FTimerHandle ResetHandle;
			World->GetTimerManager().SetTimer(ResetHandle, [this]()
			{
				if (DistanceText)
				{
					DistanceText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				}
			}, 0.5f, false);
		}
	}
}

// ---------------------------------------------------------------------------
// Road dash scrolling
// ---------------------------------------------------------------------------

void UTempleRunHUD::UpdateRoadDashes(float InDeltaTime)
{
	if (!RunnerMode || RoadDashes.Num() == 0)
	{
		return;
	}

	// Scroll speed proportional to the game's run speed.
	const float ScrollSpeed = RunnerMode->RunSpeed * 0.3f;
	RoadDashOffset += ScrollSpeed * InDeltaTime;

	// Total cycle length: when dashes wrap around.
	const float CycleLength = NUM_ROAD_DASHES * ROAD_DASH_SPACING;
	if (RoadDashOffset >= CycleLength)
	{
		RoadDashOffset -= CycleLength;
	}

	// Get the road canvas height for wrapping.
	const FGeometry& CanvasGeo = GameCanvas->GetCachedGeometry();
	const float CanvasHeight = FMath::Max(CanvasGeo.GetLocalSize().Y, 600.f);

	const float CenterX = ROAD_WIDTH * 0.5f - 2.f;

	for (int32 i = 0; i < RoadDashes.Num(); ++i)
	{
		UBorder* Dash = RoadDashes[i];
		if (!Dash) continue;

		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Dash->Slot);
		if (!Slot) continue;

		float Y = FMath::Fmod(i * ROAD_DASH_SPACING + RoadDashOffset, CycleLength);
		// Wrap dashes that go below the canvas back to the top.
		if (Y > CanvasHeight + ROAD_DASH_HEIGHT)
		{
			Y -= CycleLength;
		}

		Slot->SetOffsets(FMargin(CenterX, Y, 4.f, ROAD_DASH_HEIGHT));
	}
}

// ---------------------------------------------------------------------------
// Obstacle widget pool
// ---------------------------------------------------------------------------

UBorder* UTempleRunHUD::GetOrCreateObstacleWidget(int32 Index)
{
	// Grow the pool if needed.
	while (ObstacleWidgets.Num() <= Index)
	{
		UBorder* NewObs = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("TempleObs_%d"), ObstacleWidgets.Num()));
		NewObs->SetBrushColor(ObsBlockColor);
		NewObs->SetVisibility(ESlateVisibility::Collapsed);
		GameCanvas->AddChildToCanvas(NewObs);
		ObstacleWidgets.Add(NewObs);
	}
	return ObstacleWidgets[Index];
}

void UTempleRunHUD::UpdateObstacleWidgets()
{
	if (!RunnerMode || !GameCanvas)
	{
		return;
	}

	const TArray<FPHCRunnerTile>& Tiles = RunnerMode->ActiveTiles;

	// Get the road canvas height for Y positioning.
	const FGeometry& CanvasGeo = GameCanvas->GetCachedGeometry();
	const float CanvasHeight = FMath::Max(CanvasGeo.GetLocalSize().Y, 600.f);
	const float VisibleRange = RunnerMode->TileLength * RunnerMode->InitialTileCount;

	// Flatten all visible obstacles and coins into a single draw list.
	int32 WidgetIndex = 0;

	for (const FPHCRunnerTile& Tile : Tiles)
	{
		// Only draw tiles that are within visible range.
		if (Tile.ZPosition + RunnerMode->TileLength < 0.f || Tile.ZPosition > VisibleRange)
		{
			continue;
		}

		// Normalize tile Z to canvas Y (top = far away, bottom = close).
		const float NormalizedZ = FMath::Clamp(Tile.ZPosition / VisibleRange, 0.f, 1.f);
		const float TileY = CanvasHeight * (1.f - NormalizedZ);

		// -- Draw obstacles --
		for (const FPHCRunnerObstacle& Obs : Tile.Obstacles)
		{
			UBorder* Widget = GetOrCreateObstacleWidget(WidgetIndex);
			if (!Widget) continue;

			Widget->SetVisibility(ESlateVisibility::Visible);

			// Color by obstacle type.
			FLinearColor Color;
			switch (Obs.Type)
			{
			case EPHCObstacleType::Low:     Color = ObsLowColor; break;
			case EPHCObstacleType::High:    Color = ObsHighColor; break;
			default:                        Color = ObsBlockColor; break;
			}
			Widget->SetBrushColor(Color);

			UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
			if (Slot)
			{
				const float X = LaneToCanvasX(Obs.Lane);
				Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
				Slot->SetAlignment(FVector2D(0.5f, 0.5f));
				Slot->SetOffsets(FMargin(X, TileY, OBS_WIDTH, OBS_HEIGHT));
				Slot->SetAutoSize(false);
			}

			++WidgetIndex;
		}

		// -- Draw coins --
		for (int32 CoinIdx = 0; CoinIdx < Tile.CoinLanes.Num(); ++CoinIdx)
		{
			// Skip collected coins.
			if (Tile.CoinCollected.IsValidIndex(CoinIdx) && Tile.CoinCollected[CoinIdx])
			{
				continue;
			}

			UBorder* Widget = GetOrCreateObstacleWidget(WidgetIndex);
			if (!Widget) continue;

			Widget->SetVisibility(ESlateVisibility::Visible);
			Widget->SetBrushColor(CoinColor);

			UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
			if (Slot)
			{
				const float X = LaneToCanvasX(Tile.CoinLanes[CoinIdx]);
				// Offset coins slightly from obstacle position.
				const float CoinY = TileY - 15.f;
				Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
				Slot->SetAlignment(FVector2D(0.5f, 0.5f));
				Slot->SetOffsets(FMargin(X, CoinY, 20.f, 20.f));
				Slot->SetAutoSize(false);
			}

			++WidgetIndex;
		}
	}

	// Hide any excess pooled widgets.
	for (int32 i = WidgetIndex; i < ObstacleWidgets.Num(); ++i)
	{
		if (ObstacleWidgets[i])
		{
			ObstacleWidgets[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float UTempleRunHUD::ZToCanvasY(float ZPosition) const
{
	// Map game Z-distance to canvas Y.
	// Higher Z (further away) maps to lower canvas Y (toward top).
	// Canvas origin is top-left.
	const float CanvasHeight = 600.f; // Design reference height.
	const float VisibleRange = 5000.f;
	const float NormalizedZ = FMath::Clamp(ZPosition / VisibleRange, 0.f, 1.f);
	return CanvasHeight * (1.f - NormalizedZ);
}

float UTempleRunHUD::LaneToCanvasX(int32 Lane) const
{
	// Three lanes evenly spaced across the road width.
	const float LaneWidth = ROAD_WIDTH / NUM_LANES;
	return LaneWidth * 0.5f + LaneWidth * static_cast<float>(FMath::Clamp(Lane, 0, 2));
}
