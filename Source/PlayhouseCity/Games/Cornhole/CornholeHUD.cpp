// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Cornhole/CornholeHUD.h"
#include "Games/Cornhole/CornholeGameMode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogCornholeHUD, Log, All);

// -- Color palette ------------------------------------------------------------

static const FLinearColor GrassGreen(0.18f, 0.55f, 0.15f, 1.f);
static const FLinearColor GrassDark(0.12f, 0.4f, 0.1f, 1.f);
static const FLinearColor BoardTan(0.72f, 0.58f, 0.35f, 1.f);
static const FLinearColor BoardEdge(0.55f, 0.42f, 0.25f, 1.f);
static const FLinearColor HoleColor(0.05f, 0.05f, 0.05f, 1.f);
static const FLinearColor BagRed(0.82f, 0.15f, 0.12f, 1.f);
static const FLinearColor BagBlue(0.12f, 0.25f, 0.82f, 1.f);
static const FLinearColor BtnNormal(0.2f, 0.4f, 0.2f, 1.f);
static const FLinearColor BtnHover(0.3f, 0.55f, 0.3f, 1.f);
static const FLinearColor BtnPress(0.15f, 0.3f, 0.15f, 1.f);
static const FLinearColor ThrowBtnNormal(0.7f, 0.2f, 0.1f, 1.f);
static const FLinearColor ThrowBtnHover(0.85f, 0.3f, 0.15f, 1.f);
static const FLinearColor ThrowBtnPress(0.5f, 0.15f, 0.08f, 1.f);
static const FLinearColor TextWhite(1.f, 1.f, 1.f, 1.f);
static const FLinearColor ScoreGold(1.f, 0.85f, 0.f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build Cornhole game UI
// ---------------------------------------------------------------------------

bool UCornholeHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard against double-build.
	if (GameCanvas)
	{
		return true;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo LabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo TitleFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
	const FSlateFontInfo ScoreFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

	// Helper: create a styled button.
	auto MakeButton = [&](const FName& Name, const FString& Label,
		FLinearColor NormalCol, FLinearColor HoverCol, FLinearColor PressCol) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(NormalCol);
		Style.Hovered.TintColor = FSlateColor(HoverCol);
		Style.Pressed.TintColor = FSlateColor(PressCol);
		Btn->SetStyle(Style);

		UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Lbl"))));
		BtnLabel->SetText(FText::FromString(Label));
		BtnLabel->SetFont(BtnFont);
		BtnLabel->SetColorAndOpacity(FSlateColor(TextWhite));
		BtnLabel->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLabel);

		return Btn;
	};

	// ================================================================
	// GAME AREA (green grass background, below top bar)
	// ================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CornholeGameArea"));
	GameArea->SetBrushColor(GrassGreen);
	GameArea->SetPadding(FMargin(8.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("CornholeContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ================================================================
	// ROW 1: Score display + Round counter + Turn indicator
	// ================================================================
	UHorizontalBox* ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CornholeScoreRow"));

	UVerticalBoxSlot* ScoreRowSlot = ContentVBox->AddChildToVerticalBox(ScoreRow);
	ScoreRowSlot->SetHorizontalAlignment(HAlign_Fill);
	ScoreRowSlot->SetPadding(FMargin(8.f, 4.f));

	// Player score
	PlayerScoreText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerScoreText"));
	PlayerScoreText->SetText(FText::FromString(TEXT("Player: 0")));
	PlayerScoreText->SetFont(ScoreFont);
	PlayerScoreText->SetColorAndOpacity(FSlateColor(BagRed));
	PlayerScoreText->SetJustification(ETextJustify::Left);

	UHorizontalBoxSlot* PScoreSlot = ScoreRow->AddChildToHorizontalBox(PlayerScoreText);
	PScoreSlot->SetVerticalAlignment(VAlign_Center);
	PScoreSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// Round counter
	RoundText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("RoundText"));
	RoundText->SetText(FText::FromString(TEXT("Round 1")));
	RoundText->SetFont(TitleFont);
	RoundText->SetColorAndOpacity(FSlateColor(ScoreGold));
	RoundText->SetJustification(ETextJustify::Center);

	UHorizontalBoxSlot* RoundSlot = ScoreRow->AddChildToHorizontalBox(RoundText);
	RoundSlot->SetVerticalAlignment(VAlign_Center);
	RoundSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	RoundSlot->SetHorizontalAlignment(HAlign_Center);

	// CPU score
	CPUScoreText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPUScoreText"));
	CPUScoreText->SetText(FText::FromString(TEXT("CPU: 0")));
	CPUScoreText->SetFont(ScoreFont);
	CPUScoreText->SetColorAndOpacity(FSlateColor(BagBlue));
	CPUScoreText->SetJustification(ETextJustify::Right);

	UHorizontalBoxSlot* CScoreSlot = ScoreRow->AddChildToHorizontalBox(CPUScoreText);
	CScoreSlot->SetVerticalAlignment(VAlign_Center);
	CScoreSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));

	// Turn indicator row
	UHorizontalBox* TurnRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CornholeTurnRow"));

	UVerticalBoxSlot* TurnRowSlot = ContentVBox->AddChildToVerticalBox(TurnRow);
	TurnRowSlot->SetHorizontalAlignment(HAlign_Center);
	TurnRowSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 4.f));

	TurnText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TurnText"));
	TurnText->SetText(FText::FromString(TEXT("Your Turn")));
	TurnText->SetFont(LabelFont);
	TurnText->SetColorAndOpacity(FSlateColor(TextWhite));
	TurnText->SetJustification(ETextJustify::Center);

	UHorizontalBoxSlot* TurnTxtSlot = TurnRow->AddChildToHorizontalBox(TurnText);
	TurnTxtSlot->SetHorizontalAlignment(HAlign_Center);
	TurnTxtSlot->SetPadding(FMargin(8.f, 0.f));

	BagsRemainingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("BagsRemainingText"));
	BagsRemainingText->SetText(FText::FromString(TEXT("Bags  You: 4  CPU: 4")));
	BagsRemainingText->SetFont(LabelFont);
	BagsRemainingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.7f)));
	BagsRemainingText->SetJustification(ETextJustify::Center);

	UHorizontalBoxSlot* BagsSlot = TurnRow->AddChildToHorizontalBox(BagsRemainingText);
	BagsSlot->SetHorizontalAlignment(HAlign_Center);
	BagsSlot->SetPadding(FMargin(8.f, 0.f));

	// ================================================================
	// ROW 2: Game field canvas (board + hole + bag)
	// ================================================================
	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));

	UBorder* FieldBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("FieldBorder"));
	FieldBorder->SetBrushColor(GrassDark);
	FieldBorder->SetPadding(FMargin(4.f));
	FieldBorder->SetContent(GameCanvas);

	UVerticalBoxSlot* FieldSlot = ContentVBox->AddChildToVerticalBox(FieldBorder);
	FieldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FieldSlot->SetHorizontalAlignment(HAlign_Fill);
	FieldSlot->SetPadding(FMargin(8.f, 4.f));

	// -- Cornhole board (angled rectangle in upper area) --
	// The board is represented as a tan rectangle. The "angle" is suggested
	// by a trapezoidal visual: a slightly narrower top portion atop a wider base.
	// We approximate this with two overlapping rectangles.

	// Board base (wider bottom part)
	UBorder* BoardBase = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("BoardBase"));
	BoardBase->SetBrushColor(BoardEdge);
	BoardBase->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* BoardBaseSlot = GameCanvas->AddChildToCanvas(BoardBase);
	BoardBaseSlot->SetAnchors(FAnchors(0.2f, 0.05f, 0.8f, 0.55f));
	BoardBaseSlot->SetOffsets(FMargin(0.f));

	// Board top surface (slightly inset, lighter)
	BoardWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("BoardWidget"));
	BoardWidget->SetBrushColor(BoardTan);
	BoardWidget->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* BoardSlot = GameCanvas->AddChildToCanvas(BoardWidget);
	BoardSlot->SetAnchors(FAnchors(0.22f, 0.07f, 0.78f, 0.53f));
	BoardSlot->SetOffsets(FMargin(0.f));

	// Hole on the board (dark circle - approximated as a small rectangle)
	HoleWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("HoleWidget"));
	HoleWidget->SetBrushColor(HoleColor);
	HoleWidget->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* HoleSlot = GameCanvas->AddChildToCanvas(HoleWidget);
	HoleSlot->SetAnchors(FAnchors(0.43f, 0.15f, 0.57f, 0.28f));
	HoleSlot->SetOffsets(FMargin(0.f));

	// -- Bean bag (at bottom of field, player's throw position) --
	BagWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("BagWidget"));
	BagWidget->SetBrushColor(BagRed);
	BagWidget->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* BagSlot = GameCanvas->AddChildToCanvas(BagWidget);
	BagSlot->SetAnchors(FAnchors(0.45f, 0.78f, 0.55f, 0.88f));
	BagSlot->SetOffsets(FMargin(0.f));

	// ================================================================
	// ROW 3: Controls row (Aim + Power + Throw)
	// ================================================================
	UHorizontalBox* ControlsRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CornholeControlsRow"));

	UVerticalBoxSlot* CtrlSlot = ContentVBox->AddChildToVerticalBox(ControlsRow);
	CtrlSlot->SetHorizontalAlignment(HAlign_Center);
	CtrlSlot->SetPadding(FMargin(8.f, 4.f, 8.f, 4.f));

	// -- Aim controls --
	UVerticalBox* AimVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("AimVBox"));
	UHorizontalBoxSlot* AimSlot = ControlsRow->AddChildToHorizontalBox(AimVBox);
	AimSlot->SetVerticalAlignment(VAlign_Center);
	AimSlot->SetPadding(FMargin(4.f, 0.f));

	UTextBlock* AimLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AimLabel"));
	AimLabel->SetText(FText::FromString(TEXT("AIM")));
	AimLabel->SetFont(LabelFont);
	AimLabel->SetColorAndOpacity(FSlateColor(TextWhite));
	AimLabel->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* AimLblSlot = AimVBox->AddChildToVerticalBox(AimLabel);
	AimLblSlot->SetHorizontalAlignment(HAlign_Center);
	AimLblSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	UHorizontalBox* AimBtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("AimBtnRow"));
	UVerticalBoxSlot* AimBtnSlot = AimVBox->AddChildToVerticalBox(AimBtnRow);
	AimBtnSlot->SetHorizontalAlignment(HAlign_Center);

	AimLeftButton = MakeButton(TEXT("AimLeftBtn"), TEXT("< LEFT"), BtnNormal, BtnHover, BtnPress);
	UHorizontalBoxSlot* ALSlot = AimBtnRow->AddChildToHorizontalBox(AimLeftButton);
	ALSlot->SetPadding(FMargin(2.f, 0.f));

	AimRightButton = MakeButton(TEXT("AimRightBtn"), TEXT("RIGHT >"), BtnNormal, BtnHover, BtnPress);
	UHorizontalBoxSlot* ARSlot = AimBtnRow->AddChildToHorizontalBox(AimRightButton);
	ARSlot->SetPadding(FMargin(2.f, 0.f));

	// -- Spacer --
	USpacer* CtrlSpacer1 = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("CtrlSpacer1"));
	CtrlSpacer1->SetSize(FVector2D(16.f, 1.f));
	ControlsRow->AddChildToHorizontalBox(CtrlSpacer1);

	// -- Power controls --
	UVerticalBox* PowerVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("PowerVBox"));
	UHorizontalBoxSlot* PowerSlot = ControlsRow->AddChildToHorizontalBox(PowerVBox);
	PowerSlot->SetVerticalAlignment(VAlign_Center);
	PowerSlot->SetPadding(FMargin(4.f, 0.f));

	PowerLabelText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PowerLabelText"));
	PowerLabelText->SetText(FText::FromString(TEXT("POWER")));
	PowerLabelText->SetFont(LabelFont);
	PowerLabelText->SetColorAndOpacity(FSlateColor(TextWhite));
	PowerLabelText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* PwrLblSlot = PowerVBox->AddChildToVerticalBox(PowerLabelText);
	PwrLblSlot->SetHorizontalAlignment(HAlign_Center);
	PwrLblSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	// Power bar
	USizeBox* PowerBarSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("PowerBarSize"));
	PowerBarSize->SetMinDesiredWidth(120.f);
	PowerBarSize->SetMinDesiredHeight(20.f);

	PowerBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("PowerBar"));
	PowerBar->SetPercent(0.5f);
	PowerBar->SetFillColorAndOpacity(FLinearColor(0.9f, 0.5f, 0.1f, 1.f));
	PowerBarSize->AddChild(PowerBar);

	UVerticalBoxSlot* PBarSlot = PowerVBox->AddChildToVerticalBox(PowerBarSize);
	PBarSlot->SetHorizontalAlignment(HAlign_Center);
	PBarSlot->SetPadding(FMargin(0.f, 2.f));

	// Power up/down buttons
	UHorizontalBox* PowerBtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PowerBtnRow"));
	UVerticalBoxSlot* PwrBtnSlot = PowerVBox->AddChildToVerticalBox(PowerBtnRow);
	PwrBtnSlot->SetHorizontalAlignment(HAlign_Center);

	PowerDownButton = MakeButton(TEXT("PowerDownBtn"), TEXT("- DOWN"), BtnNormal, BtnHover, BtnPress);
	UHorizontalBoxSlot* PDSlot = PowerBtnRow->AddChildToHorizontalBox(PowerDownButton);
	PDSlot->SetPadding(FMargin(2.f, 0.f));

	PowerUpButton = MakeButton(TEXT("PowerUpBtn"), TEXT("UP +"), BtnNormal, BtnHover, BtnPress);
	UHorizontalBoxSlot* PUSlot = PowerBtnRow->AddChildToHorizontalBox(PowerUpButton);
	PUSlot->SetPadding(FMargin(2.f, 0.f));

	// -- Spacer --
	USpacer* CtrlSpacer2 = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("CtrlSpacer2"));
	CtrlSpacer2->SetSize(FVector2D(16.f, 1.f));
	ControlsRow->AddChildToHorizontalBox(CtrlSpacer2);

	// -- Throw button (large, prominent) --
	USizeBox* ThrowSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ThrowSizeBox"));
	ThrowSizeBox->SetMinDesiredWidth(100.f);
	ThrowSizeBox->SetMinDesiredHeight(60.f);

	ThrowButton = MakeButton(TEXT("ThrowBtn"), TEXT("THROW!"), ThrowBtnNormal, ThrowBtnHover, ThrowBtnPress);
	ThrowSizeBox->AddChild(ThrowButton);

	UHorizontalBoxSlot* ThrowSlot = ControlsRow->AddChildToHorizontalBox(ThrowSizeBox);
	ThrowSlot->SetVerticalAlignment(VAlign_Center);
	ThrowSlot->SetPadding(FMargin(8.f, 0.f));

	UE_LOG(LogCornholeHUD, Log,
		TEXT("Cornhole HUD built: board, bag, aim/power/throw controls, score display"));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates
// ---------------------------------------------------------------------------

void UCornholeHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache a typed reference to the cornhole game mode from the base-class
	// reference (set by the PlayerController before NativeConstruct runs).
	CornholeMode = Cast<ACornholeGameMode>(GameModeRef);

	// Allow this widget to receive keyboard / mouse focus for drag input.
	SetIsFocusable(true);

	// Initialise power bar to default level.
	if (PowerBar)
	{
		PowerBar->SetPercent(PowerLevel);
	}

	// Wire button delegates.
	if (AimLeftButton)
	{
		AimLeftButton->OnClicked.AddDynamic(this, &UCornholeHUD::OnAimLeftClicked);
	}
	if (AimRightButton)
	{
		AimRightButton->OnClicked.AddDynamic(this, &UCornholeHUD::OnAimRightClicked);
	}
	if (PowerUpButton)
	{
		PowerUpButton->OnClicked.AddDynamic(this, &UCornholeHUD::OnPowerUpClicked);
	}
	if (PowerDownButton)
	{
		PowerDownButton->OnClicked.AddDynamic(this, &UCornholeHUD::OnPowerDownClicked);
	}
	if (ThrowButton)
	{
		ThrowButton->OnClicked.AddDynamic(this, &UCornholeHUD::OnThrowClicked);
	}
}

// ---------------------------------------------------------------------------
// NativeTick - bag flight animation + display refresh
// ---------------------------------------------------------------------------

void UCornholeHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CornholeMode)
	{
		return;
	}

	// -- Bag flight animation ---------------------------------------------

	if (bBagInFlight && BagWidget)
	{
		FlightTime += InDeltaTime;
		const float Alpha = FMath::Clamp(FlightTime / FlightDuration, 0.f, 1.f);

		if (Alpha >= 1.f)
		{
			// Flight complete - reset bag to start position.
			bBagInFlight = false;
			FlightTime = 0.f;

			UCanvasPanelSlot* BagSlot = Cast<UCanvasPanelSlot>(BagWidget->Slot);
			if (BagSlot)
			{
				BagSlot->SetAnchors(FAnchors(0.45f, 0.78f, 0.55f, 0.88f));
				BagSlot->SetOffsets(FMargin(0.f));
			}

			// Reset bag color for current player.
			BagWidget->SetBrushColor(
				CornholeMode->IsPlayerTurn() ? BagRed : BagBlue);
		}
		else
		{
			// Interpolate bag position along a parabolic arc.
			const float LerpX = FMath::Lerp(BagStartPos.X, BagTargetPos.X, Alpha);
			const float LerpY = FMath::Lerp(BagStartPos.Y, BagTargetPos.Y, Alpha);

			// Parabolic arc: bag rises then falls.
			const float ArcHeight = -0.15f * FMath::Sin(Alpha * PI);

			UCanvasPanelSlot* BagSlot = Cast<UCanvasPanelSlot>(BagWidget->Slot);
			if (BagSlot)
			{
				const float BagW = 0.10f;
				const float BagH = 0.10f;
				BagSlot->SetAnchors(FAnchors(
					LerpX, LerpY + ArcHeight,
					LerpX + BagW, LerpY + ArcHeight + BagH));
				BagSlot->SetOffsets(FMargin(0.f));
			}

			// Scale down the bag as it gets further (perspective).
			// We approximate by reducing the anchor size slightly.
			const float ScaleFactor = FMath::Lerp(1.f, 0.6f, Alpha);
			if (BagSlot)
			{
				const float BagW = 0.10f * ScaleFactor;
				const float BagH = 0.10f * ScaleFactor;
				const float CenterX = LerpX + 0.05f;
				const float CenterY = LerpY + ArcHeight + 0.05f;
				BagSlot->SetAnchors(FAnchors(
					CenterX - BagW * 0.5f, CenterY - BagH * 0.5f,
					CenterX + BagW * 0.5f, CenterY + BagH * 0.5f));
			}
		}
	}

	// -- Score displays ---------------------------------------------------

	if (PlayerScoreText)
	{
		PlayerScoreText->SetText(FText::FromString(
			FString::Printf(TEXT("Player: %d"), CornholeMode->PlayerTotalScore)));
	}

	if (CPUScoreText)
	{
		CPUScoreText->SetText(FText::FromString(
			FString::Printf(TEXT("CPU: %d"), CornholeMode->CPUTotalScore)));
	}

	// -- Round ------------------------------------------------------------

	if (RoundText)
	{
		RoundText->SetText(FText::FromString(
			FString::Printf(TEXT("Round %d"), CornholeMode->CurrentRound)));
	}

	// -- Turn indicator ---------------------------------------------------

	if (TurnText)
	{
		TurnText->SetText(CornholeMode->IsPlayerTurn()
			? FText::FromString(TEXT("Your Turn"))
			: FText::FromString(TEXT("CPU Turn")));
	}

	// -- Bags remaining ---------------------------------------------------

	if (BagsRemainingText)
	{
		const int32 PlayerRemaining =
			CornholeMode->BagsPerRound - CornholeMode->PlayerBagsThrown;
		const int32 CPURemaining =
			CornholeMode->BagsPerRound - CornholeMode->CPUBagsThrown;

		BagsRemainingText->SetText(FText::FromString(
			FString::Printf(TEXT("Bags  You: %d  CPU: %d"),
				FMath::Max(0, PlayerRemaining),
				FMath::Max(0, CPURemaining))));
	}

	// -- Power bar (live during aiming or button-controlled) ---------------

	if (PowerBar)
	{
		if (bAiming)
		{
			PowerBar->SetPercent(GetThrowPower());
		}
		else
		{
			PowerBar->SetPercent(PowerLevel);
		}
	}

	// -- Bag color (red for player, blue for CPU) -------------------------

	if (BagWidget && !bBagInFlight)
	{
		BagWidget->SetBrushColor(
			CornholeMode->IsPlayerTurn() ? BagRed : BagBlue);
	}

	// -- Enable/disable controls based on turn ----------------------------

	const bool bCanThrow = CornholeMode->IsPlayerTurn() && !bBagInFlight;
	if (ThrowButton) ThrowButton->SetIsEnabled(bCanThrow);
	if (AimLeftButton) AimLeftButton->SetIsEnabled(bCanThrow);
	if (AimRightButton) AimRightButton->SetIsEnabled(bCanThrow);
	if (PowerUpButton) PowerUpButton->SetIsEnabled(bCanThrow);
	if (PowerDownButton) PowerDownButton->SetIsEnabled(bCanThrow);
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------

FReply UCornholeHUD::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!CornholeMode || !CornholeMode->IsPlayerTurn())
	{
		return FReply::Unhandled();
	}

	bAiming = true;
	AimStartPos = InMouseEvent.GetScreenSpacePosition();
	AimCurrentPos = AimStartPos;

	return FReply::Handled().CaptureMouse(GetCachedWidget().ToSharedRef());
}

FReply UCornholeHUD::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bAiming)
	{
		AimCurrentPos = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UCornholeHUD::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bAiming)
	{
		AimCurrentPos = InMouseEvent.GetScreenSpacePosition();
		ProcessAimRelease();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

// ---------------------------------------------------------------------------
// Touch input
// ---------------------------------------------------------------------------

FReply UCornholeHUD::NativeOnTouchStarted(
	const FGeometry& InGeometry,
	const FPointerEvent& InTouchEvent)
{
	if (!CornholeMode || !CornholeMode->IsPlayerTurn())
	{
		return FReply::Unhandled();
	}

	bAiming = true;
	AimStartPos = InTouchEvent.GetScreenSpacePosition();
	AimCurrentPos = AimStartPos;

	return FReply::Handled();
}

FReply UCornholeHUD::NativeOnTouchMoved(
	const FGeometry& InGeometry,
	const FPointerEvent& InTouchEvent)
{
	if (bAiming)
	{
		AimCurrentPos = InTouchEvent.GetScreenSpacePosition();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UCornholeHUD::NativeOnTouchEnded(
	const FGeometry& InGeometry,
	const FPointerEvent& InTouchEvent)
{
	if (bAiming)
	{
		AimCurrentPos = InTouchEvent.GetScreenSpacePosition();
		ProcessAimRelease();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UCornholeHUD::OnAimLeftClicked()
{
	AimOffset = FMath::Clamp(AimOffset - 0.15f, -1.f, 1.f);
}

void UCornholeHUD::OnAimRightClicked()
{
	AimOffset = FMath::Clamp(AimOffset + 0.15f, -1.f, 1.f);
}

void UCornholeHUD::OnPowerUpClicked()
{
	PowerLevel = FMath::Clamp(PowerLevel + 0.1f, 0.f, 1.f);
	if (PowerBar)
	{
		PowerBar->SetPercent(PowerLevel);
	}
}

void UCornholeHUD::OnPowerDownClicked()
{
	PowerLevel = FMath::Clamp(PowerLevel - 0.1f, 0.f, 1.f);
	if (PowerBar)
	{
		PowerBar->SetPercent(PowerLevel);
	}
}

void UCornholeHUD::OnThrowClicked()
{
	if (!CornholeMode || !CornholeMode->IsPlayerTurn() || bBagInFlight)
	{
		return;
	}

	if (PowerLevel < 0.05f)
	{
		return; // Too weak, ignore.
	}

	// Convert button-based aim/power to a throw direction.
	const FVector Direction(
		1.f,          // forward (towards the board)
		-AimOffset,   // lateral aim
		0.5f);        // fixed upward arc component

	CornholeMode->ThrowBag(Direction, PowerLevel);

	// Start bag flight animation.
	bBagInFlight = true;
	FlightTime = 0.f;
	BagStartPos = FVector2D(0.45f, 0.78f);
	// Target is near the hole, adjusted by aim offset.
	BagTargetPos = FVector2D(
		0.45f + AimOffset * 0.15f,
		0.15f + (1.f - PowerLevel) * 0.15f);

	// Reset aim for next throw.
	AimOffset = 0.f;
	PowerLevel = 0.5f;
}

// ---------------------------------------------------------------------------
// Aiming helpers
// ---------------------------------------------------------------------------

void UCornholeHUD::ProcessAimRelease()
{
	bAiming = false;

	if (!CornholeMode || !CornholeMode->IsPlayerTurn())
	{
		return;
	}

	const float Power = GetThrowPower();

	// Ignore negligible drags (accidental taps).
	if (Power < 0.05f)
	{
		return;
	}

	const FVector2D Dir2D = GetThrowDirection();

	// Convert 2D screen-space direction to a 3D world-space throw vector.
	// X = forward (towards the board), Y = lateral, Z = upward arc.
	const FVector Direction(
		FMath::Abs(Dir2D.Y),   // drag-up   -> forward throw
		-Dir2D.X,              // drag-left / right -> lateral aim
		0.5f);                 // fixed upward arc component

	CornholeMode->ThrowBag(Direction, Power);

	// Start bag flight animation.
	bBagInFlight = true;
	FlightTime = 0.f;
	BagStartPos = FVector2D(0.45f, 0.78f);
	BagTargetPos = FVector2D(
		0.45f + Dir2D.X * 0.15f,
		0.15f + (1.f - Power) * 0.15f);
}

FVector2D UCornholeHUD::GetThrowDirection() const
{
	// Slingshot: pull back to throw forward.  Direction is Start -> Current
	// reversed so pulling down launches up.
	const FVector2D Delta = AimStartPos - AimCurrentPos;
	const float Len = Delta.Size();
	if (Len < KINDA_SMALL_NUMBER)
	{
		return FVector2D(0.f, 1.f); // default forward
	}
	return Delta / Len;
}

float UCornholeHUD::GetThrowPower() const
{
	const float Distance = FVector2D::Distance(AimStartPos, AimCurrentPos);
	return FMath::Clamp(Distance / MaxDragDistance, 0.f, 1.f);
}
