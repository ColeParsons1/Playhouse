// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Jenga/JengaHUD.h"
#include "Games/Jenga/JengaGameMode.h"
#include "Games/Jenga/JengaBlock.h"
#include "Games/Jenga/JengaTower.h"
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

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "CollisionQueryParams.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogJengaHUD, Log, All);

// -- Colors used outside Initialize() (wrapped to avoid unity build conflicts) --
namespace
{
const FLinearColor WoodBrown(0.55f, 0.35f, 0.17f, 1.f);
const FLinearColor WoodLight(0.65f, 0.42f, 0.22f, 1.f);
const FLinearColor WoodDark(0.45f, 0.28f, 0.12f, 1.f);
const FLinearColor StabilityGreen(0.2f, 0.8f, 0.2f, 1.f);
const FLinearColor StabilityRed(0.85f, 0.12f, 0.12f, 1.f);
const FLinearColor TextWhite(1.f, 1.f, 1.f, 1.f);
}

// -- Row color variation (alternates between light/medium/dark) ---------------

static FLinearColor GetRowColor(int32 Row)
{
	switch (Row % 3)
	{
	case 0:  return WoodBrown;
	case 1:  return WoodLight;
	default: return WoodDark;
	}
}

// ---------------------------------------------------------------------------
// Initialize - build Jenga tower UI
// ---------------------------------------------------------------------------

bool UJengaHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard against double-build.
	if (TowerCanvas)
	{
		return true;
	}

	// -- Colors ---------------------------------------------------------------
	const FLinearColor DarkBrownBg(0.15f, 0.09f, 0.04f, 0.95f);
	const FLinearColor HighlightYellow(1.f, 0.85f, 0.1f, 0.7f);
	const FLinearColor JengaBtnNorm(0.45f, 0.3f, 0.12f, 1.f);
	const FLinearColor JengaBtnHov(0.6f, 0.4f, 0.18f, 1.f);
	const FLinearColor JengaBtnPrs(0.35f, 0.22f, 0.08f, 1.f);

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo LabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo TitleFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
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
	// GAME AREA (dark brown background, below top bar)
	// ================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("JengaGameArea"));
	GameArea->SetBrushColor(DarkBrownBg);
	GameArea->SetPadding(FMargin(8.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	UHorizontalBox* MainHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("JengaMainHBox"));
	GameArea->SetContent(MainHBox);

	// ================================================================
	// LEFT COLUMN: Stability meter + labels
	// ================================================================
	UVerticalBox* LeftCol = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("JengaLeftCol"));
	UHorizontalBoxSlot* LeftSlot = MainHBox->AddChildToHorizontalBox(LeftCol);
	LeftSlot->SetPadding(FMargin(8.f));
	LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// Stability label
	StabilityLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StabilityLabel"));
	StabilityLabel->SetText(FText::FromString(TEXT("STABILITY")));
	StabilityLabel->SetFont(LabelFont);
	StabilityLabel->SetColorAndOpacity(FSlateColor(StabilityGreen));
	StabilityLabel->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* StabLblSlot = LeftCol->AddChildToVerticalBox(StabilityLabel);
	StabLblSlot->SetHorizontalAlignment(HAlign_Center);
	StabLblSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));

	// Stability meter (progress bar)
	USizeBox* MeterSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("MeterSizeBox"));
	MeterSizeBox->SetMinDesiredWidth(30.f);
	MeterSizeBox->SetMinDesiredHeight(200.f);

	StabilityMeter = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("StabilityMeter"));
	StabilityMeter->SetPercent(1.f);
	StabilityMeter->SetFillColorAndOpacity(StabilityGreen);
	MeterSizeBox->AddChild(StabilityMeter);

	UVerticalBoxSlot* MeterSlot = LeftCol->AddChildToVerticalBox(MeterSizeBox);
	MeterSlot->SetHorizontalAlignment(HAlign_Center);
	MeterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MeterSlot->SetPadding(FMargin(4.f));

	// Danger zone label (red)
	UTextBlock* DangerLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DangerLabel"));
	DangerLabel->SetText(FText::FromString(TEXT("DANGER")));
	DangerLabel->SetFont(LabelFont);
	DangerLabel->SetColorAndOpacity(FSlateColor(StabilityRed));
	DangerLabel->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* DangerSlot = LeftCol->AddChildToVerticalBox(DangerLabel);
	DangerSlot->SetHorizontalAlignment(HAlign_Center);
	DangerSlot->SetPadding(FMargin(0.f, 4.f));

	// ================================================================
	// CENTER COLUMN: Tower display
	// ================================================================
	UVerticalBox* CenterCol = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("JengaCenterCol"));
	UHorizontalBoxSlot* CenterSlot = MainHBox->AddChildToHorizontalBox(CenterCol);
	CenterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CenterSlot->SetHorizontalAlignment(HAlign_Center);
	CenterSlot->SetPadding(FMargin(8.f));

	// Turn indicator
	TurnText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TurnText"));
	TurnText->SetText(FText::FromString(TEXT("Your Turn")));
	TurnText->SetFont(TitleFont);
	TurnText->SetColorAndOpacity(FSlateColor(TextWhite));
	TurnText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* TurnSlot = CenterCol->AddChildToVerticalBox(TurnText);
	TurnSlot->SetHorizontalAlignment(HAlign_Center);
	TurnSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));

	// Tower canvas (holds all block widgets)
	TowerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("TowerCanvas"));

	USizeBox* TowerSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("TowerSizeBox"));
	TowerSizeBox->SetMinDesiredWidth(BlockWidth * BlocksPerRow + BlockGap * (BlocksPerRow + 1));
	TowerSizeBox->SetMinDesiredHeight((BlockHeight + BlockGap) * NumRows + BlockGap);
	TowerSizeBox->AddChild(TowerCanvas);

	UVerticalBoxSlot* TowerSlot = CenterCol->AddChildToVerticalBox(TowerSizeBox);
	TowerSlot->SetHorizontalAlignment(HAlign_Center);
	TowerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TowerSlot->SetPadding(FMargin(0.f, 4.f));

	// Build 18 blocks (6 rows x 3 blocks each).
	// Rows alternate orientation: even rows = horizontal (side by side),
	// odd rows = rotated 90 degrees (visually stacked differently).
	// In the 2D HUD, we represent orientation difference via block
	// width/height swap and offset pattern.
	BlockWidgets.SetNum(NumRows * BlocksPerRow);

	const float TowerWidth = BlockWidth * BlocksPerRow + BlockGap * (BlocksPerRow + 1);

	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		// Visual row 0 = bottom of tower. We draw bottom-up, so visual Y
		// is inverted: row 0 at the bottom, row 5 at the top.
		const int32 VisualRow = (NumRows - 1) - Row;
		const float RowY = VisualRow * (BlockHeight + BlockGap) + BlockGap;
		const FLinearColor RowColor = GetRowColor(Row);

		// Alternate block layout per row.
		const bool bRotated = (Row % 2) != 0;
		const float BW = bRotated ? (BlockWidth * 0.9f) : BlockWidth;
		const float RowBlocksWidth = BW * BlocksPerRow + BlockGap * (BlocksPerRow - 1);
		const float RowStartX = (TowerWidth - RowBlocksWidth) * 0.5f;

		for (int32 Col = 0; Col < BlocksPerRow; ++Col)
		{
			const int32 Idx = Row * BlocksPerRow + Col;
			const float BlockX = RowStartX + Col * (BW + BlockGap);

			// Slight color variation per block within the row.
			FLinearColor BlockColor = RowColor;
			BlockColor.R += (Col - 1) * 0.03f;
			BlockColor.G += (Col - 1) * 0.02f;

			UBorder* Block = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				*FString::Printf(TEXT("JBlock_%d_%d"), Row, Col));
			Block->SetBrushColor(BlockColor);
			Block->SetPadding(FMargin(0.f));

			UCanvasPanelSlot* BlockSlot = TowerCanvas->AddChildToCanvas(Block);
			BlockSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			BlockSlot->SetOffsets(FMargin(BlockX, RowY, BW, BlockHeight));
			BlockSlot->SetAutoSize(false);

			BlockWidgets[Idx] = Block;
		}
	}

	// Selected block highlight overlay (hidden by default, repositioned on selection).
	SelectedBlockHighlight = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SelectedBlockHighlight"));
	SelectedBlockHighlight->SetBrushColor(HighlightYellow);
	SelectedBlockHighlight->SetPadding(FMargin(0.f));
	SelectedBlockHighlight->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* HighlightSlot = TowerCanvas->AddChildToCanvas(SelectedBlockHighlight);
	HighlightSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	HighlightSlot->SetOffsets(FMargin(0.f, 0.f, BlockWidth, BlockHeight));
	HighlightSlot->SetAutoSize(false);

	// Touch instructions below tower
	TouchInstructions = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TouchInstructions"));
	TouchInstructions->SetText(FText::FromString(TEXT("Tap a block to select")));
	TouchInstructions->SetFont(LabelFont);
	TouchInstructions->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.7f, 0.5f)));
	TouchInstructions->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* InstrSlot = CenterCol->AddChildToVerticalBox(TouchInstructions);
	InstrSlot->SetHorizontalAlignment(HAlign_Center);
	InstrSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));

	// Button row: PULL LEFT | PULL RIGHT | PLACE ON TOP
	UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("JengaBtnRow"));

	UVerticalBoxSlot* BtnRowSlot = CenterCol->AddChildToVerticalBox(BtnRow);
	BtnRowSlot->SetHorizontalAlignment(HAlign_Center);
	BtnRowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));

	PullLeftButton = MakeButton(TEXT("PullLeftBtn"), TEXT("PULL LEFT"), JengaBtnNorm, JengaBtnHov, JengaBtnPrs);
	UHorizontalBoxSlot* PLSlot = BtnRow->AddChildToHorizontalBox(PullLeftButton);
	PLSlot->SetPadding(FMargin(4.f, 0.f));

	PullRightButton = MakeButton(TEXT("PullRightBtn"), TEXT("PULL RIGHT"), JengaBtnNorm, JengaBtnHov, JengaBtnPrs);
	UHorizontalBoxSlot* PRSlot = BtnRow->AddChildToHorizontalBox(PullRightButton);
	PRSlot->SetPadding(FMargin(4.f, 0.f));

	PlaceOnTopButton = MakeButton(TEXT("PlaceOnTopBtn"), TEXT("PLACE ON TOP"),
		FLinearColor(0.2f, 0.5f, 0.2f, 1.f),
		FLinearColor(0.3f, 0.65f, 0.3f, 1.f),
		FLinearColor(0.15f, 0.35f, 0.15f, 1.f));
	UHorizontalBoxSlot* POTSlot = BtnRow->AddChildToHorizontalBox(PlaceOnTopButton);
	POTSlot->SetPadding(FMargin(4.f, 0.f));

	// ================================================================
	// RIGHT COLUMN: Stats
	// ================================================================
	UVerticalBox* RightCol = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("JengaRightCol"));
	UHorizontalBoxSlot* RightSlot = MainHBox->AddChildToHorizontalBox(RightCol);
	RightSlot->SetPadding(FMargin(8.f));
	RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// Blocks removed text
	BlocksRemovedText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("BlocksRemovedText"));
	BlocksRemovedText->SetText(FText::FromString(TEXT("Removed: 0")));
	BlocksRemovedText->SetFont(LabelFont);
	BlocksRemovedText->SetColorAndOpacity(FSlateColor(TextWhite));
	BlocksRemovedText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* RemovedSlot = RightCol->AddChildToVerticalBox(BlocksRemovedText);
	RemovedSlot->SetHorizontalAlignment(HAlign_Center);
	RemovedSlot->SetPadding(FMargin(0.f, 8.f));

	// Block count text (remaining)
	BlockCountText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("BlockCountText"));
	BlockCountText->SetText(FText::FromString(TEXT("Blocks: 18")));
	BlockCountText->SetFont(LabelFont);
	BlockCountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.7f, 0.5f)));
	BlockCountText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* CountSlot = RightCol->AddChildToVerticalBox(BlockCountText);
	CountSlot->SetHorizontalAlignment(HAlign_Center);
	CountSlot->SetPadding(FMargin(0.f, 4.f));

	// Fill spacer to push content up
	USpacer* RightSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("JengaRightSpacer"));
	UVerticalBoxSlot* RSSlot = RightCol->AddChildToVerticalBox(RightSpacer);
	RSSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UE_LOG(LogJengaHUD, Log,
		TEXT("Jenga HUD built: %d block widgets, tower %dx%d"),
		BlockWidgets.Num(), NumRows, BlocksPerRow);

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up button delegates
// ---------------------------------------------------------------------------

void UJengaHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cast from the base-class reference (set by the PlayerController
	// before NativeConstruct runs).
	JengaMode = Cast<AJengaGameMode>(GameModeRef);

	// Wire button delegates.
	if (PullLeftButton)
	{
		PullLeftButton->OnClicked.AddDynamic(this, &UJengaHUD::OnPullLeftClicked);
	}
	if (PullRightButton)
	{
		PullRightButton->OnClicked.AddDynamic(this, &UJengaHUD::OnPullRightClicked);
	}
	if (PlaceOnTopButton)
	{
		PlaceOnTopButton->OnClicked.AddDynamic(this, &UJengaHUD::OnPlaceOnTopClicked);
	}

	RefreshDisplay();
}

// ---------------------------------------------------------------------------
// NativeTick - wobble animation + display refresh
// ---------------------------------------------------------------------------

void UJengaHUD::NativeTick(
	const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	WobbleTime += InDeltaTime;

	// Wobble animation: slightly shift block positions based on stability.
	if (JengaMode && JengaMode->Tower && TowerCanvas)
	{
		const float LeanAngle = JengaMode->Tower->GetCurrentLeanAngle();
		const float Instability = FMath::Clamp(
			LeanAngle / JengaMode->TowerStabilityThreshold, 0.f, 1.f);

		// Higher instability = more wobble amplitude.
		const float WobbleAmplitude = Instability * 3.f;

		if (WobbleAmplitude > 0.1f)
		{
			for (int32 i = 0; i < BlockWidgets.Num(); ++i)
			{
				if (!BlockWidgets[i])
				{
					continue;
				}

				UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(
					BlockWidgets[i]->Slot);
				if (!Slot)
				{
					continue;
				}

				// Each block wobbles at a slightly different phase.
				const float Phase = WobbleTime * 8.f + i * 0.7f;
				const float OffsetX = FMath::Sin(Phase) * WobbleAmplitude;

				FMargin Current = Slot->GetOffsets();
				const int32 Row = i / BlocksPerRow;
				const int32 Col = i % BlocksPerRow;
				const int32 VisualRow = (NumRows - 1) - Row;
				const bool bRotated = (Row % 2) != 0;
				const float BW = bRotated ? (BlockWidth * 0.9f) : BlockWidth;
				const float TowerWidth = BlockWidth * BlocksPerRow + BlockGap * (BlocksPerRow + 1);
				const float RowBlocksWidth = BW * BlocksPerRow + BlockGap * (BlocksPerRow - 1);
				const float BaseX = (TowerWidth - RowBlocksWidth) * 0.5f + Col * (BW + BlockGap);

				Current.Left = BaseX + OffsetX;
				Slot->SetOffsets(Current);
			}
		}

		// Update stability meter color: green -> red based on instability.
		if (StabilityMeter)
		{
			const FLinearColor MeterColor = FMath::Lerp(StabilityGreen, StabilityRed, Instability);
			StabilityMeter->SetFillColorAndOpacity(MeterColor);
		}

		if (StabilityLabel)
		{
			const FLinearColor LabelColor = FMath::Lerp(StabilityGreen, StabilityRed, Instability);
			StabilityLabel->SetColorAndOpacity(FSlateColor(LabelColor));
		}
	}

	RefreshDisplay();
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------

FReply UJengaHUD::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	InteractionStartPos = InMouseEvent.GetScreenSpacePosition();
	PreviousPointerPos = InteractionStartPos;

	// Try to select a block under the cursor
	AJengaBlock* HitBlock = TraceBlockAtScreenPos(InteractionStartPos);
	if (HitBlock && JengaMode)
	{
		JengaMode->SelectBlock(HitBlock);
		if (JengaMode->SelectedBlock == HitBlock)
		{
			bDraggingBlock = true;
			bOrbitingCamera = false;
		}
		else
		{
			bDraggingBlock = false;
			bOrbitingCamera = true;
		}
	}
	else
	{
		bDraggingBlock = false;
		bOrbitingCamera = true;
	}

	return FReply::Handled().CaptureMouse(TakeWidget());
}

// ---------------------------------------------------------------------------

FReply UJengaHUD::NativeOnMouseButtonUp(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingBlock && JengaMode)
	{
		JengaMode->ReleaseBlock();
	}

	bDraggingBlock = false;
	bOrbitingCamera = false;

	return FReply::Handled().ReleaseMouseCapture();
}

// ---------------------------------------------------------------------------

FReply UJengaHUD::NativeOnMouseMove(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D CurrentPos = InMouseEvent.GetScreenSpacePosition();
	const FVector2D Delta = CurrentPos - PreviousPointerPos;
	PreviousPointerPos = CurrentPos;

	if (bDraggingBlock && JengaMode && JengaMode->SelectedBlock)
	{
		// Pull the selected block
		const FVector PullDir = ComputePullDirection(Delta);
		const float Amount = Delta.Size() * PullSensitivity;
		JengaMode->PullBlock(PullDir, Amount);
	}
	else if (bOrbitingCamera)
	{
		// Orbit camera around tower
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				const FRotator DeltaRot(
					-Delta.Y * OrbitSensitivity,
					Delta.X * OrbitSensitivity,
					0.f);
				Pawn->AddActorWorldRotation(DeltaRot);
			}
		}
	}

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Touch input
// ---------------------------------------------------------------------------

FReply UJengaHUD::NativeOnTouchStarted(
	const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	InteractionStartPos = InTouchEvent.GetScreenSpacePosition();
	PreviousPointerPos = InteractionStartPos;

	AJengaBlock* HitBlock = TraceBlockAtScreenPos(InteractionStartPos);
	if (HitBlock && JengaMode)
	{
		JengaMode->SelectBlock(HitBlock);
		if (JengaMode->SelectedBlock == HitBlock)
		{
			bDraggingBlock = true;
			bOrbitingCamera = false;
		}
		else
		{
			bDraggingBlock = false;
			bOrbitingCamera = true;
		}
	}
	else
	{
		bDraggingBlock = false;
		bOrbitingCamera = true;
	}

	return FReply::Handled();
}

// ---------------------------------------------------------------------------

FReply UJengaHUD::NativeOnTouchMoved(
	const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	const FVector2D CurrentPos = InTouchEvent.GetScreenSpacePosition();
	const FVector2D Delta = CurrentPos - PreviousPointerPos;
	PreviousPointerPos = CurrentPos;

	if (bDraggingBlock && JengaMode && JengaMode->SelectedBlock)
	{
		const FVector PullDir = ComputePullDirection(Delta);
		const float Amount = Delta.Size() * PullSensitivity;
		JengaMode->PullBlock(PullDir, Amount);
	}
	else if (bOrbitingCamera)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				const FRotator DeltaRot(
					-Delta.Y * OrbitSensitivity,
					Delta.X * OrbitSensitivity,
					0.f);
				Pawn->AddActorWorldRotation(DeltaRot);
			}
		}
	}

	return FReply::Handled();
}

// ---------------------------------------------------------------------------

FReply UJengaHUD::NativeOnTouchEnded(
	const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (bDraggingBlock && JengaMode)
	{
		JengaMode->ReleaseBlock();
	}

	bDraggingBlock = false;
	bOrbitingCamera = false;

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UJengaHUD::OnPullLeftClicked()
{
	if (JengaMode && JengaMode->SelectedBlock)
	{
		JengaMode->PullBlock(FVector::LeftVector, 20.f);
	}
}

void UJengaHUD::OnPullRightClicked()
{
	if (JengaMode && JengaMode->SelectedBlock)
	{
		JengaMode->PullBlock(FVector::RightVector, 20.f);
	}
}

void UJengaHUD::OnPlaceOnTopClicked()
{
	if (JengaMode && JengaMode->SelectedBlock)
	{
		JengaMode->PlaceOnTop(JengaMode->SelectedBlock);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UJengaHUD::RefreshDisplay()
{
	if (!JengaMode)
	{
		return;
	}

	// Blocks removed count
	if (BlocksRemovedText)
	{
		BlocksRemovedText->SetText(FText::FromString(
			FString::Printf(TEXT("Removed: %d"), JengaMode->BlocksRemoved)));
	}

	// Block count (remaining in tower)
	if (BlockCountText)
	{
		const int32 Total = NumRows * BlocksPerRow;
		const int32 Remaining = Total - JengaMode->BlocksRemoved;
		BlockCountText->SetText(FText::FromString(
			FString::Printf(TEXT("Blocks: %d"), FMath::Max(0, Remaining))));
	}

	// Stability meter: lean angle normalised to threshold
	if (StabilityMeter && JengaMode->Tower)
	{
		const float LeanAngle = JengaMode->Tower->GetCurrentLeanAngle();
		const float Ratio = FMath::Clamp(
			LeanAngle / JengaMode->TowerStabilityThreshold, 0.f, 1.f);
		StabilityMeter->SetPercent(1.f - Ratio); // full = stable, empty = about to fall
	}

	// Turn text
	if (TurnText)
	{
		if (JengaMode->bTowerFallen)
		{
			TurnText->SetText(FText::FromString(TEXT("TOWER FALLEN!")));
			TurnText->SetColorAndOpacity(FSlateColor(StabilityRed));
		}
		else if (JengaMode->bWaitingForSettle)
		{
			TurnText->SetText(FText::FromString(TEXT("Settling...")));
			TurnText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.8f, 0.3f)));
		}
		else
		{
			TurnText->SetText(FText::FromString(TEXT("Your Turn")));
			TurnText->SetColorAndOpacity(FSlateColor(TextWhite));
		}
	}

	// Touch instructions
	if (TouchInstructions)
	{
		FString Instructions;
		if (JengaMode->bTowerFallen)
		{
			Instructions = TEXT("Tower collapsed!");
		}
		else if (JengaMode->bWaitingForSettle)
		{
			Instructions = TEXT("Waiting for tower to settle...");
		}
		else if (JengaMode->bIsPulling)
		{
			Instructions = TEXT("Drag to pull block out");
		}
		else if (JengaMode->SelectedBlock)
		{
			Instructions = TEXT("Drag to slide block out");
		}
		else
		{
			Instructions = TEXT("Tap a block to select");
		}
		TouchInstructions->SetText(FText::FromString(Instructions));
	}

	// Enable/disable pull buttons based on selection state.
	const bool bHasSelection = JengaMode->SelectedBlock != nullptr
		&& !JengaMode->bTowerFallen
		&& !JengaMode->bWaitingForSettle;

	if (PullLeftButton)
	{
		PullLeftButton->SetIsEnabled(bHasSelection);
	}
	if (PullRightButton)
	{
		PullRightButton->SetIsEnabled(bHasSelection);
	}
	if (PlaceOnTopButton)
	{
		PlaceOnTopButton->SetIsEnabled(bHasSelection && JengaMode->bIsPulling);
	}

	// Update selected block highlight.
	if (SelectedBlockHighlight)
	{
		if (JengaMode->SelectedBlock && !JengaMode->bTowerFallen)
		{
			// Find the index of the selected block in the game mode's block list
			// and map it to our widget array. For the HUD overlay, we show a
			// highlight at the corresponding widget position.
			const int32 BlockIdx = JengaMode->Blocks.IndexOfByKey(JengaMode->SelectedBlock);
			if (BlockIdx != INDEX_NONE && BlockIdx < BlockWidgets.Num() && BlockWidgets[BlockIdx])
			{
				SelectedBlockHighlight->SetVisibility(ESlateVisibility::HitTestInvisible);

				// Copy position from the target block widget.
				UCanvasPanelSlot* SrcSlot = Cast<UCanvasPanelSlot>(BlockWidgets[BlockIdx]->Slot);
				UCanvasPanelSlot* DstSlot = Cast<UCanvasPanelSlot>(SelectedBlockHighlight->Slot);
				if (SrcSlot && DstSlot)
				{
					DstSlot->SetOffsets(SrcSlot->GetOffsets());
				}
				SelectedBlockIndex = BlockIdx;
			}
			else
			{
				SelectedBlockHighlight->SetVisibility(ESlateVisibility::Collapsed);
				SelectedBlockIndex = -1;
			}
		}
		else
		{
			SelectedBlockHighlight->SetVisibility(ESlateVisibility::Collapsed);
			SelectedBlockIndex = -1;
		}
	}
}

// ---------------------------------------------------------------------------

AJengaBlock* UJengaHUD::TraceBlockAtScreenPos(const FVector2D& ScreenPos) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	FVector WorldOrigin, WorldDir;
	if (!PC->DeprojectScreenPositionToWorld(
			ScreenPos.X, ScreenPos.Y, WorldOrigin, WorldDir))
	{
		return nullptr;
	}

	const FVector TraceEnd = WorldOrigin + WorldDir * 5000.f;

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;

	if (World->LineTraceSingleByChannel(
			Hit, WorldOrigin, TraceEnd, ECC_PhysicsBody, QueryParams))
	{
		return Cast<AJengaBlock>(Hit.GetActor());
	}

	return nullptr;
}

// ---------------------------------------------------------------------------

FVector UJengaHUD::ComputePullDirection(const FVector2D& DragDelta) const
{
	// Map screen-space drag direction to a world-space pull direction.
	// Horizontal screen movement maps to the block's length axis.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FVector::ForwardVector;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return FVector::ForwardVector;
	}

	// Use the camera's right vector for horizontal drags
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector CamRight = FRotationMatrix(CamRot).GetUnitAxis(EAxis::Y);
	const FVector CamUp = FRotationMatrix(CamRot).GetUnitAxis(EAxis::Z);

	// Combine horizontal and vertical screen movement into world direction
	FVector WorldDir = CamRight * DragDelta.X + CamUp * -DragDelta.Y;
	WorldDir.Z = 0.f; // Keep movement horizontal
	WorldDir.Normalize();

	return WorldDir.IsNearlyZero() ? FVector::ForwardVector : WorldDir;
}
