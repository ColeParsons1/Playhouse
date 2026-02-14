// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Carrom/CarromHUD.h"
#include "Games/Carrom/CarromGameMode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogCarromHUD, Log, All);

// -- Colour palette ---------------------------------------------------------

static const FLinearColor BoardBrown(0.65f, 0.45f, 0.25f, 1.f);       // main board surface
static const FLinearColor BoardFrameColor(0.40f, 0.25f, 0.12f, 1.f);   // outer frame
static const FLinearColor BoardTan(0.78f, 0.65f, 0.42f, 1.f);         // lighter inner surface
static const FLinearColor PocketDark(0.10f, 0.08f, 0.06f, 1.f);       // dark corner pockets
static const FLinearColor WhitePiece(0.95f, 0.95f, 0.90f, 1.f);       // white coins
static const FLinearColor BlackPiece(0.10f, 0.10f, 0.12f, 1.f);       // black coins
static const FLinearColor QueenRed(0.85f, 0.15f, 0.15f, 1.f);         // red queen
static const FLinearColor StrikerRed(0.90f, 0.20f, 0.15f, 1.f);       // striker
static const FLinearColor GameAreaBg(0.08f, 0.08f, 0.12f, 0.9f);      // background behind board
static const FLinearColor ControlBg(0.05f, 0.05f, 0.1f, 0.85f);       // control bar background
static const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
static const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
static const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);
static const FLinearColor ShootBtnNormal(0.15f, 0.70f, 0.25f, 1.f);
static const FLinearColor ShootBtnHover(0.20f, 0.85f, 0.30f, 1.f);
static const FLinearColor ShootBtnPress(0.10f, 0.50f, 0.18f, 1.f);

// Board layout constants.
static constexpr float BoardSize      = 300.f; // square board dimension
static constexpr float PocketSize     = 22.f;  // corner pocket diameter
static constexpr float PieceSize      = 16.f;  // coin widget diameter
static constexpr float StrikerSize    = 20.f;  // striker widget diameter
static constexpr int32 NumWhitePieces = 9;
static constexpr int32 NumBlackPieces = 9;

// ---------------------------------------------------------------------------
// Initialize - build the Carrom game UI
// ---------------------------------------------------------------------------

bool UCarromHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard against double-build.
	if (BoardCanvas)
	{
		return true;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo LabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo ValueFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo TurnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
	const FSlateFontInfo ScoreFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 15);
	const FSlateFontInfo SmallFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12);

	// -- Helper: styled button with label -----------------------------------
	auto MakeButton = [&](const FName& Name, const FString& Label,
						   FLinearColor NormalCol, FLinearColor HoverCol,
						   FLinearColor PressCol, const FSlateFontInfo& Font) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor  = FSlateColor(NormalCol);
		Style.Hovered.TintColor = FSlateColor(HoverCol);
		Style.Pressed.TintColor = FSlateColor(PressCol);
		Btn->SetStyle(Style);

		UTextBlock* BtnLbl = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Lbl"))));
		BtnLbl->SetText(FText::FromString(Label));
		BtnLbl->SetFont(Font);
		BtnLbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLbl->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLbl);
		return Btn;
	};

	// ===================================================================
	// GAME AREA  (below top bar, fills remaining screen minus control bar)
	// ===================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CarromGameArea"));
	GameArea->SetBrushColor(GameAreaBg);
	GameArea->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* GameAreaSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameAreaSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameAreaSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	// Inner canvas for absolute positioning.
	UCanvasPanel* InnerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("CarromInnerCanvas"));
	GameArea->SetContent(InnerCanvas);

	// ===================================================================
	// SCORE / STATUS DISPLAY (left side of game area)
	// ===================================================================
	UVerticalBox* ScoreVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ScoreVBox"));

	UCanvasPanelSlot* ScoreSlot = InnerCanvas->AddChildToCanvas(ScoreVBox);
	ScoreSlot->SetAnchors(FAnchors(0.02f, 0.05f, 0.02f, 0.05f));
	ScoreSlot->SetOffsets(FMargin(0.f, 0.f, 140.f, 250.f));

	// Turn text.
	TurnText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TurnText"));
	TurnText->SetText(FText::FromString(TEXT("Your Turn (White)")));
	TurnText->SetFont(TurnFont);
	TurnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* TurnSlot = ScoreVBox->AddChildToVerticalBox(TurnText);
	TurnSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	// Player score.
	PlayerScoreText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerScoreText"));
	PlayerScoreText->SetText(FText::FromString(TEXT("You: 0")));
	PlayerScoreText->SetFont(ScoreFont);
	PlayerScoreText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UVerticalBoxSlot* PSSlot = ScoreVBox->AddChildToVerticalBox(PlayerScoreText);
	PSSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	// CPU score.
	CPUScoreText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPUScoreText"));
	CPUScoreText->SetText(FText::FromString(TEXT("CPU: 0")));
	CPUScoreText->SetFont(ScoreFont);
	CPUScoreText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.8f)));
	UVerticalBoxSlot* CSSlot = ScoreVBox->AddChildToVerticalBox(CPUScoreText);
	CSSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// Queen status.
	QueenStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("QueenStatusText"));
	QueenStatusText->SetText(FText::FromString(TEXT("Queen: Uncovered")));
	QueenStatusText->SetFont(SmallFont);
	QueenStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.4f, 0.4f)));
	UVerticalBoxSlot* QSSlot = ScoreVBox->AddChildToVerticalBox(QueenStatusText);
	QSSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	// Pieces remaining.
	PlayerPiecesText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerPiecesText"));
	PlayerPiecesText->SetText(FText::FromString(TEXT("White left: 9")));
	PlayerPiecesText->SetFont(SmallFont);
	PlayerPiecesText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f)));
	ScoreVBox->AddChildToVerticalBox(PlayerPiecesText);

	CPUPiecesText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPUPiecesText"));
	CPUPiecesText->SetText(FText::FromString(TEXT("Black left: 9")));
	CPUPiecesText->SetFont(SmallFont);
	CPUPiecesText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.7f)));
	ScoreVBox->AddChildToVerticalBox(CPUPiecesText);

	// ===================================================================
	// CARROM BOARD  (centered square)
	// ===================================================================

	// Board frame (outer border).
	UBorder* BoardFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("BoardFrame"));
	BoardFrame->SetBrushColor(BoardFrame->GetBrushColor()); // reset
	BoardFrame->SetBrushColor(BoardBrown);
	BoardFrame->SetPadding(FMargin(8.f));

	UCanvasPanelSlot* FrameSlot = InnerCanvas->AddChildToCanvas(BoardFrame);
	FrameSlot->SetAnchors(FAnchors(0.5f, 0.45f, 0.5f, 0.45f));
	const float FrameSize = BoardSize + 16.f;
	FrameSlot->SetOffsets(FMargin(-FrameSize * 0.5f, -FrameSize * 0.5f, FrameSize, FrameSize));
	FrameSlot->SetAlignment(FVector2D(0.f, 0.f));

	// Board surface (inner tan area).
	BoardSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("BoardSurface"));
	BoardSurface->SetBrushColor(BoardTan);
	BoardSurface->SetPadding(FMargin(0.f));

	// BoardCanvas sits inside the board surface for piece positioning.
	BoardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BoardCanvas"));
	BoardSurface->SetContent(BoardCanvas);
	BoardFrame->SetContent(BoardSurface);

	// ===================================================================
	// CORNER POCKETS (4 dark squares at board corners)
	// ===================================================================
	auto MakePocket = [&](const FName& Name, float AnchorX, float AnchorY) -> UBorder*
	{
		UBorder* Pocket = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), Name);
		Pocket->SetBrushColor(PocketDark);

		UCanvasPanelSlot* PSlot = BoardCanvas->AddChildToCanvas(Pocket);
		PSlot->SetAnchors(FAnchors(AnchorX, AnchorY, AnchorX, AnchorY));
		PSlot->SetOffsets(FMargin(-PocketSize * 0.5f, -PocketSize * 0.5f, PocketSize, PocketSize));
		return Pocket;
	};

	PocketTL = MakePocket(TEXT("PocketTL"), 0.03f, 0.03f);
	PocketTR = MakePocket(TEXT("PocketTR"), 0.97f, 0.03f);
	PocketBL = MakePocket(TEXT("PocketBL"), 0.03f, 0.97f);
	PocketBR = MakePocket(TEXT("PocketBR"), 0.97f, 0.97f);

	// ===================================================================
	// PIECES  (arranged in centre circle pattern)
	// ===================================================================

	// Helper: create a piece widget at a given normalized position.
	auto MakePiece = [&](const FName& Name, FLinearColor Color,
						  float NormX, float NormY, float Size) -> UBorder*
	{
		UBorder* Piece = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), Name);
		Piece->SetBrushColor(Color);

		UCanvasPanelSlot* PSlot = BoardCanvas->AddChildToCanvas(Piece);
		PSlot->SetAnchors(FAnchors(NormX, NormY, NormX, NormY));
		PSlot->SetOffsets(FMargin(-Size * 0.5f, -Size * 0.5f, Size, Size));
		return Piece;
	};

	// Centre arrangement: queen at dead centre, pieces in a ring.
	// Ring of 18 pieces around the centre at radius ~0.08 (normalised).
	const float CentreX = 0.5f;
	const float CentreY = 0.5f;
	const float RingRadius = 0.08f;

	WhitePieceWidgets.SetNum(NumWhitePieces);
	BlackPieceWidgets.SetNum(NumBlackPieces);

	for (int32 i = 0; i < NumWhitePieces + NumBlackPieces; ++i)
	{
		const float AngleRad = (static_cast<float>(i) / static_cast<float>(NumWhitePieces + NumBlackPieces)) * 2.f * PI;
		const float PX = CentreX + FMath::Cos(AngleRad) * RingRadius;
		const float PY = CentreY + FMath::Sin(AngleRad) * RingRadius;

		if (i < NumWhitePieces)
		{
			WhitePieceWidgets[i] = MakePiece(
				*FString::Printf(TEXT("WhitePiece_%d"), i),
				WhitePiece, PX, PY, PieceSize);
		}
		else
		{
			const int32 Idx = i - NumWhitePieces;
			BlackPieceWidgets[Idx] = MakePiece(
				*FString::Printf(TEXT("BlackPiece_%d"), Idx),
				BlackPiece, PX, PY, PieceSize);
		}
	}

	// Queen at dead centre.
	QueenWidget = MakePiece(TEXT("QueenWidget"), QueenRed, CentreX, CentreY, PieceSize + 2.f);

	// ===================================================================
	// STRIKER  (bottom baseline, player's starting position)
	// ===================================================================
	StrikerWidget = MakePiece(TEXT("StrikerWidget"), StrikerRed, CentreX, 0.88f, StrikerSize);

	// ===================================================================
	// AIM ANGLE TEXT  (above the board)
	// ===================================================================
	AimAngleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AimAngleText"));
	AimAngleText->SetText(FText::FromString(TEXT("Aim: 0")));
	AimAngleText->SetFont(SmallFont);
	AimAngleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.9f)));
	AimAngleText->SetJustification(ETextJustify::Center);

	UCanvasPanelSlot* AimTxtSlot = InnerCanvas->AddChildToCanvas(AimAngleText);
	AimTxtSlot->SetAnchors(FAnchors(0.5f, 0.78f, 0.5f, 0.78f));
	AimTxtSlot->SetOffsets(FMargin(-50.f, 0.f, 100.f, 18.f));

	// ===================================================================
	// CONTROL PANEL  (bottom bar: aim left/right, power, shoot)
	// ===================================================================
	UBorder* ControlBar = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CarromControlBar"));
	ControlBar->SetBrushColor(ControlBg);
	ControlBar->SetPadding(FMargin(12.f, 6.f));

	UCanvasPanelSlot* CtrlSlot = RootCanvas->AddChildToCanvas(ControlBar);
	CtrlSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
	CtrlSlot->SetOffsets(FMargin(0.f, -70.f, 0.f, 70.f));

	UHorizontalBox* CtrlHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CarromCtrlHBox"));
	ControlBar->SetContent(CtrlHBox);

	// -- AIM controls ----------------------------------------------------
	UTextBlock* AimLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AimLabel"));
	AimLabel->SetText(FText::FromString(TEXT("Aim:")));
	AimLabel->SetFont(LabelFont);
	AimLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* AimLblSlot = CtrlHBox->AddChildToHorizontalBox(AimLabel);
	AimLblSlot->SetVerticalAlignment(VAlign_Center);
	AimLblSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));

	AimLeftButton = MakeButton(TEXT("AimLeftButton"), TEXT("LEFT"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* ALBSlot = CtrlHBox->AddChildToHorizontalBox(AimLeftButton);
	ALBSlot->SetVerticalAlignment(VAlign_Center);
	ALBSlot->SetPadding(FMargin(2.f, 0.f));

	AimRightButton = MakeButton(TEXT("AimRightButton"), TEXT("RIGHT"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* ARBSlot = CtrlHBox->AddChildToHorizontalBox(AimRightButton);
	ARBSlot->SetVerticalAlignment(VAlign_Center);
	ARBSlot->SetPadding(FMargin(2.f, 0.f));

	// Spacer.
	USpacer* Sp1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("CarromSp1"));
	UHorizontalBoxSlot* Sp1Slot = CtrlHBox->AddChildToHorizontalBox(Sp1);
	Sp1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// -- Power controls ---------------------------------------------------
	UTextBlock* PowerLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CarromPowerLabel"));
	PowerLabel->SetText(FText::FromString(TEXT("Power:")));
	PowerLabel->SetFont(LabelFont);
	PowerLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* PLblSlot = CtrlHBox->AddChildToHorizontalBox(PowerLabel);
	PLblSlot->SetVerticalAlignment(VAlign_Center);
	PLblSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));

	PowerDownButton = MakeButton(TEXT("CarromPowerDown"), TEXT("-"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* PDBSlot = CtrlHBox->AddChildToHorizontalBox(PowerDownButton);
	PDBSlot->SetVerticalAlignment(VAlign_Center);
	PDBSlot->SetPadding(FMargin(2.f, 0.f));

	// Power bar.
	USizeBox* PowerSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("CarromPowerSizeBox"));
	PowerSizeBox->SetMinDesiredWidth(90.f);
	PowerSizeBox->SetMinDesiredHeight(18.f);

	FlickPowerBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("FlickPowerBar"));
	FlickPowerBar->SetPercent(0.5f);
	FlickPowerBar->SetFillColorAndOpacity(FLinearColor(1.f, 0.5f, 0.1f, 1.f));
	PowerSizeBox->AddChild(FlickPowerBar);

	UHorizontalBoxSlot* PBarSlot = CtrlHBox->AddChildToHorizontalBox(PowerSizeBox);
	PBarSlot->SetVerticalAlignment(VAlign_Center);
	PBarSlot->SetPadding(FMargin(4.f, 0.f));

	PowerValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CarromPowerValue"));
	PowerValueText->SetText(FText::FromString(TEXT("50%")));
	PowerValueText->SetFont(ValueFont);
	PowerValueText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.7f, 0.2f)));
	UHorizontalBoxSlot* PVSlot = CtrlHBox->AddChildToHorizontalBox(PowerValueText);
	PVSlot->SetVerticalAlignment(VAlign_Center);
	PVSlot->SetPadding(FMargin(4.f, 0.f));

	PowerUpButton = MakeButton(TEXT("CarromPowerUp"), TEXT("+"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* PUBSlot = CtrlHBox->AddChildToHorizontalBox(PowerUpButton);
	PUBSlot->SetVerticalAlignment(VAlign_Center);
	PUBSlot->SetPadding(FMargin(2.f, 0.f));

	// Spacer before shoot.
	USpacer* Sp2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("CarromSp2"));
	UHorizontalBoxSlot* Sp2Slot = CtrlHBox->AddChildToHorizontalBox(Sp2);
	Sp2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// -- SHOOT button -----------------------------------------------------
	ShootButton = MakeButton(TEXT("ShootButton"), TEXT("SHOOT"),
		ShootBtnNormal, ShootBtnHover, ShootBtnPress, TurnFont);

	USizeBox* ShootSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ShootSizeBox"));
	ShootSizeBox->SetMinDesiredWidth(90.f);
	ShootSizeBox->SetMinDesiredHeight(44.f);
	ShootSizeBox->AddChild(ShootButton);

	UHorizontalBoxSlot* ShootSlot = CtrlHBox->AddChildToHorizontalBox(ShootSizeBox);
	ShootSlot->SetVerticalAlignment(VAlign_Center);
	ShootSlot->SetPadding(FMargin(8.f, 0.f));

	UE_LOG(LogCarromHUD, Log,
		TEXT("Carrom HUD built: board (%dx%d), %d white + %d black pieces, queen, striker, aim/power/shoot controls."),
		static_cast<int32>(BoardSize), static_cast<int32>(BoardSize),
		NumWhitePieces, NumBlackPieces);

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates
// ---------------------------------------------------------------------------

void UCarromHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cast from the base-class reference (set by the PlayerController
	// before NativeConstruct runs).
	CarromMode = Cast<ACarromGameMode>(GameModeRef);

	if (FlickPowerBar)
	{
		FlickPowerBar->SetPercent(0.f);
	}

	// Wire button callbacks.
	if (AimLeftButton)
	{
		AimLeftButton->OnClicked.AddDynamic(this, &UCarromHUD::OnAimLeft);
	}
	if (AimRightButton)
	{
		AimRightButton->OnClicked.AddDynamic(this, &UCarromHUD::OnAimRight);
	}
	if (PowerUpButton)
	{
		PowerUpButton->OnClicked.AddDynamic(this, &UCarromHUD::OnPowerUp);
	}
	if (PowerDownButton)
	{
		PowerDownButton->OnClicked.AddDynamic(this, &UCarromHUD::OnPowerDown);
	}
	if (ShootButton)
	{
		ShootButton->OnClicked.AddDynamic(this, &UCarromHUD::OnShootClicked);
	}
}

// ---------------------------------------------------------------------------
// NativeTick - update dynamic elements
// ---------------------------------------------------------------------------

void UCarromHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update power display.
	if (FlickPowerBar)
	{
		FlickPowerBar->SetPercent(FlickPower);
	}
	if (PowerValueText)
	{
		PowerValueText->SetText(FText::FromString(
			FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(FlickPower * 100.f))));
	}
	if (AimAngleText)
	{
		AimAngleText->SetText(FText::FromString(
			FString::Printf(TEXT("Aim: %d"), FMath::RoundToInt32(AimAngleDeg))));
	}

	// Disable shoot while pieces are moving.
	if (ShootButton && CarromMode)
	{
		const bool bCanShoot = CarromMode->bPlayerTurn && !CarromMode->bPiecesMoving;
		ShootButton->SetIsEnabled(bCanShoot);
	}

	RefreshDisplay();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UCarromHUD::SetFlickPower(float Power)
{
	FlickPower = FMath::Clamp(Power, 0.f, 1.f);
	if (FlickPowerBar)
	{
		FlickPowerBar->SetPercent(FlickPower);
	}
}

// ---------------------------------------------------------------------------

void UCarromHUD::SetFlickDirection(FVector2D Direction)
{
	if (DirectionArrow)
	{
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
		DirectionArrow->SetRenderTransformAngle(AngleDeg);
	}
}

// ---------------------------------------------------------------------------
// RefreshDisplay - update score/status from game mode
// ---------------------------------------------------------------------------

void UCarromHUD::RefreshDisplay()
{
	if (!CarromMode)
	{
		return;
	}

	// Turn
	if (TurnText)
	{
		const FText Turn = CarromMode->bPlayerTurn
			? FText::FromString(TEXT("Your Turn (White)"))
			: FText::FromString(TEXT("CPU Turn (Black)"));
		TurnText->SetText(Turn);
	}

	// Scores
	if (PlayerScoreText)
	{
		PlayerScoreText->SetText(FText::FromString(
			FString::Printf(TEXT("You: %d"), CarromMode->PlayerScore)));
	}
	if (CPUScoreText)
	{
		CPUScoreText->SetText(FText::FromString(
			FString::Printf(TEXT("CPU: %d"), CarromMode->CPUScore)));
	}

	// Queen status
	if (QueenStatusText)
	{
		const FText Status = CarromMode->bQueenCovered
			? FText::FromString(TEXT("Queen: Covered"))
			: FText::FromString(TEXT("Queen: Uncovered"));
		QueenStatusText->SetText(Status);
	}

	// Score display
	UpdateScore(CarromMode->GetScore());
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UCarromHUD::OnAimLeft()
{
	AimAngleDeg = FMath::Clamp(AimAngleDeg - 5.f, -80.f, 80.f);
}

void UCarromHUD::OnAimRight()
{
	AimAngleDeg = FMath::Clamp(AimAngleDeg + 5.f, -80.f, 80.f);
}

void UCarromHUD::OnPowerUp()
{
	FlickPower = FMath::Clamp(FlickPower + 0.1f, 0.f, 1.f);
}

void UCarromHUD::OnPowerDown()
{
	FlickPower = FMath::Clamp(FlickPower - 0.1f, 0.f, 1.f);
}

void UCarromHUD::OnShootClicked()
{
	if (!CarromMode || !CarromMode->bPlayerTurn || CarromMode->bPiecesMoving)
	{
		return;
	}

	// Convert aim angle (degrees, 0 = straight up) to a 2D direction vector.
	const float AngleRad = FMath::DegreesToRadians(AimAngleDeg);
	const FVector2D Direction(FMath::Sin(AngleRad), -FMath::Cos(AngleRad));

	CarromMode->FlickStriker(Direction, FlickPower);
}
