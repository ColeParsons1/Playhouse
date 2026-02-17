// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Pong/PongHUD.h"
#include "Engine/World.h"
#include "Games/Pong/PongGameMode.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#include "TimerManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPongHUD, Log, All);

// Pong colour palette
static const FLinearColor PongFieldBg(0.04f, 0.04f, 0.10f, 1.f);
static const FLinearColor PongCenterLine(0.25f, 0.25f, 0.35f, 1.f);
static const FLinearColor PongPaddleColor(0.9f, 0.9f, 0.95f, 1.f);
static const FLinearColor PongBallColor(1.f, 0.85f, 0.1f, 1.f);
static const FLinearColor PongScoreColor(0.9f, 0.9f, 0.95f, 1.f);
static const FLinearColor PongServingColor(0.6f, 0.8f, 1.f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Pong widget tree programmatically
// ---------------------------------------------------------------------------

bool UPongHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game-over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// If widgets already exist (e.g. Blueprint-provided) skip construction.
	if (GameCanvas)
	{
		return true;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo ScoreFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 48);
	const FSlateFontInfo ServingFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 24);

	// ================================================================
	// Game area (fills below the top bar)
	// ================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PongGameArea"));
	GameArea->SetBrushColor(PongFieldBg);
	GameArea->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* GameAreaSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameAreaSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameAreaSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("PongContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ================================================================
	// Score row: "Player  0  -  0  CPU"
	// ================================================================
	UHorizontalBox* ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PongScoreRow"));
	UVerticalBoxSlot* ScoreRowSlot = ContentVBox->AddChildToVerticalBox(ScoreRow);
	ScoreRowSlot->SetHorizontalAlignment(HAlign_Center);
	ScoreRowSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 8.f));

	// "Player" label
	UTextBlock* PlayerLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerLabel"));
	PlayerLabel->SetText(FText::FromString(TEXT("Player")));
	PlayerLabel->SetFont(FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20));
	PlayerLabel->SetColorAndOpacity(FSlateColor(PongScoreColor));
	UHorizontalBoxSlot* PLSlot = ScoreRow->AddChildToHorizontalBox(PlayerLabel);
	PLSlot->SetVerticalAlignment(VAlign_Center);
	PLSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));

	// Player score number
	PlayerScoreText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerScoreText"));
	PlayerScoreText->SetText(FText::FromString(TEXT("0")));
	PlayerScoreText->SetFont(ScoreFont);
	PlayerScoreText->SetColorAndOpacity(FSlateColor(PongScoreColor));
	UHorizontalBoxSlot* PSSlot = ScoreRow->AddChildToHorizontalBox(PlayerScoreText);
	PSSlot->SetVerticalAlignment(VAlign_Center);
	PSSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	// Dash separator
	UTextBlock* DashLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DashLabel"));
	DashLabel->SetText(FText::FromString(TEXT("-")));
	DashLabel->SetFont(ScoreFont);
	DashLabel->SetColorAndOpacity(FSlateColor(PongCenterLine));
	UHorizontalBoxSlot* DashSlot = ScoreRow->AddChildToHorizontalBox(DashLabel);
	DashSlot->SetVerticalAlignment(VAlign_Center);
	DashSlot->SetPadding(FMargin(8.f, 0.f));

	// CPU score number
	CPUScoreText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPUScoreText"));
	CPUScoreText->SetText(FText::FromString(TEXT("0")));
	CPUScoreText->SetFont(ScoreFont);
	CPUScoreText->SetColorAndOpacity(FSlateColor(PongScoreColor));
	UHorizontalBoxSlot* CSSlot = ScoreRow->AddChildToHorizontalBox(CPUScoreText);
	CSSlot->SetVerticalAlignment(VAlign_Center);
	CSSlot->SetPadding(FMargin(8.f, 0.f, 12.f, 0.f));

	// "CPU" label
	UTextBlock* CPULabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPULabel"));
	CPULabel->SetText(FText::FromString(TEXT("CPU")));
	CPULabel->SetFont(FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20));
	CPULabel->SetColorAndOpacity(FSlateColor(PongScoreColor));
	UHorizontalBoxSlot* CLSlot = ScoreRow->AddChildToHorizontalBox(CPULabel);
	CLSlot->SetVerticalAlignment(VAlign_Center);

	// ================================================================
	// Game field canvas (paddles, ball, center line)
	// ================================================================
	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));

	USizeBox* FieldSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("FieldSizeBox"));
	FieldSizeBox->SetMinDesiredWidth(800.f);
	FieldSizeBox->SetMinDesiredHeight(600.f);
	FieldSizeBox->AddChild(GameCanvas);

	UVerticalBoxSlot* FieldSlot = ContentVBox->AddChildToVerticalBox(FieldSizeBox);
	FieldSlot->SetHorizontalAlignment(HAlign_Center);
	FieldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FieldSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 0.f));

	// -- Field background border --
	UBorder* FieldBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("FieldBg"));
	FieldBg->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.14f, 1.f));

	UCanvasPanelSlot* FieldBgSlot = GameCanvas->AddChildToCanvas(FieldBg);
	FieldBgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FieldBgSlot->SetOffsets(FMargin(0.f));

	// -- Center line (vertical divider) --
	UBorder* CenterLine = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CenterLine"));
	CenterLine->SetBrushColor(PongCenterLine);

	UCanvasPanelSlot* CenterSlot = GameCanvas->AddChildToCanvas(CenterLine);
	CenterSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 1.f));
	CenterSlot->SetOffsets(FMargin(-1.f, 0.f, 2.f, 0.f)); // 2px wide centered
	CenterSlot->SetAlignment(FVector2D(0.f, 0.f));

	// -- Player paddle (left side) --
	PlayerPaddleImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("PlayerPaddleImage"));
	PlayerPaddleImage->SetColorAndOpacity(PongPaddleColor);

	UCanvasPanelSlot* PlayerPadSlot = GameCanvas->AddChildToCanvas(PlayerPaddleImage);
	PlayerPadSlot->SetPosition(FVector2D(0.f, 250.f));
	PlayerPadSlot->SetSize(FVector2D(15.f, 100.f));
	PlayerPadSlot->SetAutoSize(false);

	// -- CPU paddle (right side) --
	CPUPaddleImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("CPUPaddleImage"));
	CPUPaddleImage->SetColorAndOpacity(PongPaddleColor);

	UCanvasPanelSlot* CPUPadSlot = GameCanvas->AddChildToCanvas(CPUPaddleImage);
	CPUPadSlot->SetPosition(FVector2D(785.f, 250.f));
	CPUPadSlot->SetSize(FVector2D(15.f, 100.f));
	CPUPadSlot->SetAutoSize(false);

	// -- Ball --
	BallImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("BallImage"));
	BallImage->SetColorAndOpacity(PongBallColor);

	UCanvasPanelSlot* BallSlot = GameCanvas->AddChildToCanvas(BallImage);
	BallSlot->SetPosition(FVector2D(395.f, 295.f));
	BallSlot->SetSize(FVector2D(10.f, 10.f));
	BallSlot->SetAutoSize(false);

	// ================================================================
	// Serving / status text (centered over the field)
	// ================================================================
	ServingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ServingText"));
	ServingText->SetText(FText::FromString(TEXT("Tap to Serve")));
	ServingText->SetFont(ServingFont);
	ServingText->SetColorAndOpacity(FSlateColor(PongServingColor));
	ServingText->SetJustification(ETextJustify::Center);
	ServingText->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* ServingSlot = GameCanvas->AddChildToCanvas(ServingText);
	ServingSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	ServingSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ServingSlot->SetAutoSize(true);

	// ================================================================
	// Control hint area
	// ================================================================
	UTextBlock* ControlHint = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ControlHint"));
	ControlHint->SetText(FText::FromString(TEXT("Move mouse or touch to control your paddle")));
	ControlHint->SetFont(FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14));
	ControlHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.6f)));
	ControlHint->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* HintSlot = ContentVBox->AddChildToVerticalBox(ControlHint);
	HintSlot->SetHorizontalAlignment(HAlign_Center);
	HintSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));

	UE_LOG(LogPongHUD, Log, TEXT("Pong widget tree built programmatically."));

	return true;
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void UPongHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache the Pong game mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	PongMode = Cast<APongGameMode>(GameModeRef);

	// Hide serving text initially.
	if (ServingText)
	{
		ServingText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Ensure the widget receives mouse/touch events.
	SetIsFocusable(true);
}

void UPongHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!PongMode)
	{
		return;
	}

	// Update visual positions from game-mode state.
	UpdatePositions(PongMode->BallPosition, PongMode->PlayerPaddleY, PongMode->CPUPaddleY);

	// Update score texts.
	if (PlayerScoreText)
	{
		PlayerScoreText->SetText(FText::AsNumber(PongMode->PlayerScore));
	}
	if (CPUScoreText)
	{
		CPUScoreText->SetText(FText::AsNumber(PongMode->CPUScore));
	}

	// Show/hide serving text.
	if (ServingText)
	{
		const ESlateVisibility Vis = PongMode->bBallActive
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible;
		ServingText->SetVisibility(Vis);
	}
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

FReply UPongHUD::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (PongMode && PongMode->bGameActive)
	{
		const float FieldY = ScreenYToFieldY(InGeometry, InMouseEvent.GetScreenSpacePosition().Y);
		PongMode->SetPlayerPaddleY(FieldY);
	}

	return FReply::Handled();
}

FReply UPongHUD::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (PongMode && PongMode->bGameActive)
	{
		const float FieldY = ScreenYToFieldY(InGeometry, InTouchEvent.GetScreenSpacePosition().Y);
		PongMode->SetPlayerPaddleY(FieldY);
	}

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Visual updates
// ---------------------------------------------------------------------------

void UPongHUD::UpdatePositions(FVector2D BallPos, float PlayerY, float CPUY)
{
	if (!GameCanvas)
	{
		return;
	}

	// -- Ball --
	if (BallImage)
	{
		const FVector2D CanvasPos = FieldToCanvas(BallPos);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(BallImage->Slot))
		{
			const float HalfBall = PongMode ? PongMode->BallSize * 0.5f : 5.f;
			Slot->SetPosition(CanvasPos - FVector2D(HalfBall, HalfBall));
		}
	}

	// -- Player paddle (left side) --
	if (PlayerPaddleImage && PongMode)
	{
		const float PaddleHalfH = PongMode->PaddleHeight * 0.5f;
		const FVector2D PaddleFieldPos(0.f, PlayerY);
		const FVector2D CanvasPos = FieldToCanvas(PaddleFieldPos);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PlayerPaddleImage->Slot))
		{
			Slot->SetPosition(FVector2D(CanvasPos.X, CanvasPos.Y - PaddleHalfH));
		}
	}

	// -- CPU paddle (right side) --
	if (CPUPaddleImage && PongMode)
	{
		const float PaddleHalfH = PongMode->PaddleHeight * 0.5f;
		const FVector2D PaddleFieldPos(PongMode->FieldWidth - PongMode->PaddleWidth, CPUY);
		const FVector2D CanvasPos = FieldToCanvas(PaddleFieldPos);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CPUPaddleImage->Slot))
		{
			Slot->SetPosition(FVector2D(CanvasPos.X, CanvasPos.Y - PaddleHalfH));
		}
	}
}

void UPongHUD::ShowGoalScored(bool bPlayerScored)
{
	if (ServingText)
	{
		ServingText->SetText(bPlayerScored
			? FText::FromString(TEXT("GOAL!"))
			: FText::FromString(TEXT("CPU Scored!")));
		ServingText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// Clear the flash text after a short delay.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GoalFlashHandle,
				[this]()
				{
					if (ServingText)
					{
						ServingText->SetVisibility(ESlateVisibility::Collapsed);
					}
				},
				0.8f,
				/*bLoop=*/ false
			);
		}
	}
}

void UPongHUD::ShowMatchResult(bool bPlayerWon)
{
	if (ServingText)
	{
		ServingText->SetText(bPlayerWon
			? FText::FromString(TEXT("YOU WIN!"))
			: FText::FromString(TEXT("YOU LOSE")));
		ServingText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

// ---------------------------------------------------------------------------
// Coordinate conversion helpers
// ---------------------------------------------------------------------------

FVector2D UPongHUD::FieldToCanvas(FVector2D FieldPos) const
{
	// The canvas panel is expected to match the field dimensions.
	// If the canvas has a different size, scale proportionally.
	if (!GameCanvas || !PongMode)
	{
		return FieldPos;
	}

	const FGeometry& CanvasGeometry = GameCanvas->GetCachedGeometry();
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

	if (CanvasSize.X <= 0.f || CanvasSize.Y <= 0.f)
	{
		return FieldPos;
	}

	const float ScaleX = CanvasSize.X / PongMode->FieldWidth;
	const float ScaleY = CanvasSize.Y / PongMode->FieldHeight;

	return FVector2D(FieldPos.X * ScaleX, FieldPos.Y * ScaleY);
}

float UPongHUD::ScreenYToFieldY(const FGeometry& Geometry, float ScreenY) const
{
	if (!PongMode || !GameCanvas)
	{
		return 0.f;
	}

	// Convert screen coordinates to local canvas coordinates.
	const FGeometry& CanvasGeometry = GameCanvas->GetCachedGeometry();
	const FVector2D LocalPos = CanvasGeometry.AbsoluteToLocal(FVector2D(0.f, ScreenY));
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

	if (CanvasSize.Y <= 0.f)
	{
		return PongMode->FieldHeight * 0.5f;
	}

	// Map local Y [0..CanvasSize.Y] -> field Y [0..FieldHeight].
	const float NormalizedY = FMath::Clamp(LocalPos.Y / CanvasSize.Y, 0.f, 1.f);
	return NormalizedY * PongMode->FieldHeight;
}
