// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/PegGame/PegGameHUD.h"
#include "Games/PegGame/PegBoard.h"
#include "Games/PegGame/PegGameMode.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPegGameHUD, Log, All);

// ---------------------------------------------------------------------------
// Static color constants
// ---------------------------------------------------------------------------

const FLinearColor UPegGameHUD::BoardBrownColor     = FLinearColor(0.35f, 0.22f, 0.10f, 1.f);
const FLinearColor UPegGameHUD::PegYellowColor      = FLinearColor(0.95f, 0.82f, 0.20f, 1.f);
const FLinearColor UPegGameHUD::HoleDarkColor       = FLinearColor(0.12f, 0.08f, 0.04f, 0.9f);
const FLinearColor UPegGameHUD::SelectedGreenColor  = FLinearColor(0.2f, 0.9f, 0.3f, 1.f);
const FLinearColor UPegGameHUD::ValidJumpGreenColor = FLinearColor(0.3f, 0.75f, 0.4f, 0.85f);
const FLinearColor UPegGameHUD::GameAreaBgColor     = FLinearColor(0.06f, 0.06f, 0.10f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Peg Game board UI programmatically
// ---------------------------------------------------------------------------

bool UPegGameHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// If the board is already built (from a previous Initialize call or Blueprint),
	// skip re-creation.
	if (BoardCanvas || PegBoardWidget)
	{
		return true;
	}

	// We need the root canvas created by the base class.
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	// ---- Fonts ----
	const FSlateFontInfo StatusFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
	const FSlateFontInfo InfoFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

	// ---- Button color scheme ----
	const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
	const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
	const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);

	// Helper: create a styled button with a text label.
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
		BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLabel->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLabel);

		return Btn;
	};

	// ============================================================
	// GAME CONTENT AREA (below top bar at Y=50)
	// ============================================================

	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PegGameArea"));
	GameArea->SetBrushColor(GameAreaBgColor);
	GameArea->SetPadding(FMargin(10.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("PegContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ============================================================
	// STATUS TEXT (instruction / game over message)
	// ============================================================

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("Select a peg to jump")));
	StatusText->SetFont(StatusFont);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StatusText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* StatusSlot = ContentVBox->AddChildToVerticalBox(StatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));

	// ============================================================
	// INFO ROW (Pegs Remaining + Moves Count)
	// ============================================================

	UHorizontalBox* InfoRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PegInfoRow"));
	UVerticalBoxSlot* InfoRowSlot = ContentVBox->AddChildToVerticalBox(InfoRow);
	InfoRowSlot->SetHorizontalAlignment(HAlign_Center);
	InfoRowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));

	PegsRemainingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PegsRemainingText"));
	PegsRemainingText->SetText(FText::FromString(TEXT("Pegs: 14")));
	PegsRemainingText->SetFont(InfoFont);
	PegsRemainingText->SetColorAndOpacity(FSlateColor(PegYellowColor));
	UHorizontalBoxSlot* PegsSlot = InfoRow->AddChildToHorizontalBox(PegsRemainingText);
	PegsSlot->SetVerticalAlignment(VAlign_Center);
	PegsSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));

	MovesCountText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("MovesCountText"));
	MovesCountText->SetText(FText::FromString(TEXT("Moves: 0")));
	MovesCountText->SetFont(InfoFont);
	MovesCountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.8f, 1.f)));
	UHorizontalBoxSlot* MovesSlot = InfoRow->AddChildToHorizontalBox(MovesCountText);
	MovesSlot->SetVerticalAlignment(VAlign_Center);
	MovesSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));

	// ============================================================
	// BOARD AREA (brown background + triangle of peg buttons)
	// ============================================================

	// Size box to constrain the board area.
	USizeBox* BoardSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("BoardSizeBox"));
	BoardSizeBox->SetMinDesiredWidth(BOARD_WIDTH);
	BoardSizeBox->SetMinDesiredHeight(BOARD_HEIGHT);

	UVerticalBoxSlot* BoardSizeSlot = ContentVBox->AddChildToVerticalBox(BoardSizeBox);
	BoardSizeSlot->SetHorizontalAlignment(HAlign_Center);
	BoardSizeSlot->SetPadding(FMargin(0.f, 4.f));

	// Brown background border for the board.
	BoardBackground = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("BoardBackground"));
	BoardBackground->SetBrushColor(BoardBrownColor);
	BoardBackground->SetPadding(FMargin(0.f));
	BoardSizeBox->AddChild(BoardBackground);

	// Canvas for absolute positioning of pegs.
	BoardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BoardCanvas"));
	BoardBackground->SetContent(BoardCanvas);

	// ---- Compute triangle positions and create 15 peg buttons ----
	ComputePegPositions();

	PegButtons.SetNum(NUM_PEGS);
	PegBorders.SetNum(NUM_PEGS);

	// Function pointer table for per-peg click handlers.
	typedef void (UPegGameHUD::*FPegClickHandler)();
	FPegClickHandler PegHandlers[NUM_PEGS] = {
		&UPegGameHUD::OnPeg0,  &UPegGameHUD::OnPeg1,
		&UPegGameHUD::OnPeg2,  &UPegGameHUD::OnPeg3,
		&UPegGameHUD::OnPeg4,  &UPegGameHUD::OnPeg5,
		&UPegGameHUD::OnPeg6,  &UPegGameHUD::OnPeg7,
		&UPegGameHUD::OnPeg8,  &UPegGameHUD::OnPeg9,
		&UPegGameHUD::OnPeg10, &UPegGameHUD::OnPeg11,
		&UPegGameHUD::OnPeg12, &UPegGameHUD::OnPeg13,
		&UPegGameHUD::OnPeg14
	};

	for (int32 i = 0; i < NUM_PEGS; ++i)
	{
		// Create button for the peg position.
		UButton* PegBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("PegBtn%d"), i));

		// Style the button to be transparent (the border child provides the visual).
		FButtonStyle PegBtnStyle = PegBtn->GetStyle();
		PegBtnStyle.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
		PegBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f));
		PegBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.3f));
		PegBtn->SetStyle(PegBtnStyle);

		// Create the visual circle (UBorder with rounded appearance via color).
		UBorder* PegVisual = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("PegVis%d"), i));
		PegVisual->SetBrushColor(PegYellowColor);
		PegVisual->SetPadding(FMargin(0.f));
		PegVisual->SetDesiredSizeScale(FVector2D(1.f, 1.f));

		PegBtn->AddChild(PegVisual);

		// Place on the board canvas.
		UCanvasPanelSlot* PegSlot = BoardCanvas->AddChildToCanvas(PegBtn);
		if (PegSlot)
		{
			// Position so the center of the button is at PegPositions[i].
			PegSlot->SetPosition(PegPositions[i] - FVector2D(PEG_SIZE * 0.5f, PEG_SIZE * 0.5f));
			PegSlot->SetSize(FVector2D(PEG_SIZE, PEG_SIZE));
		}

		PegButtons[i] = PegBtn;
		PegBorders[i] = PegVisual;
	}

	bBoardBuiltProgrammatically = true;

	// ============================================================
	// BUTTON ROW (Undo + New Game)
	// ============================================================

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PegButtonRow"));
	UVerticalBoxSlot* BtnRowSlot = ContentVBox->AddChildToVerticalBox(ButtonRow);
	BtnRowSlot->SetHorizontalAlignment(HAlign_Center);
	BtnRowSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 4.f));

	// Undo button.
	UndoButton = MakeButton(TEXT("UndoButton"), TEXT("Undo"), BtnNormal, BtnHover, BtnPress);
	UHorizontalBoxSlot* UndoBtnSlot = ButtonRow->AddChildToHorizontalBox(UndoButton);
	UndoBtnSlot->SetHorizontalAlignment(HAlign_Center);
	UndoBtnSlot->SetVerticalAlignment(VAlign_Center);
	UndoBtnSlot->SetPadding(FMargin(8.f, 0.f));
	UndoButton->SetIsEnabled(false);

	// Reset / New Game button.
	ResetButton = MakeButton(TEXT("ResetButton"), TEXT("Reset"),
		FLinearColor(0.6f, 0.3f, 0.1f, 1.f),
		FLinearColor(0.75f, 0.4f, 0.15f, 1.f),
		FLinearColor(0.45f, 0.2f, 0.08f, 1.f));
	UHorizontalBoxSlot* ResetBtnSlot = ButtonRow->AddChildToHorizontalBox(ResetButton);
	ResetBtnSlot->SetHorizontalAlignment(HAlign_Center);
	ResetBtnSlot->SetVerticalAlignment(VAlign_Center);
	ResetBtnSlot->SetPadding(FMargin(8.f, 0.f));

	// New Game button (distinct from Reset: starts fresh with a possibly different empty peg).
	NewGameButton = MakeButton(TEXT("NewGameButton"), TEXT("New Game"),
		FLinearColor(0.15f, 0.5f, 0.2f, 1.f),
		FLinearColor(0.2f, 0.65f, 0.3f, 1.f),
		FLinearColor(0.1f, 0.4f, 0.15f, 1.f));
	UHorizontalBoxSlot* NewGameBtnSlot = ButtonRow->AddChildToHorizontalBox(NewGameButton);
	NewGameBtnSlot->SetHorizontalAlignment(HAlign_Center);
	NewGameBtnSlot->SetVerticalAlignment(VAlign_Center);
	NewGameBtnSlot->SetPadding(FMargin(8.f, 0.f));

	UE_LOG(LogPegGameHUD, Log,
		TEXT("PegGame board built programmatically: %d peg buttons on triangle layout."),
		PegButtons.Num());

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates
// ---------------------------------------------------------------------------

void UPegGameHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// If we have the legacy Blueprint-provided PegBoard widget, wire it up.
	if (PegBoardWidget)
	{
		PegBoardWidget->OnPegClicked.AddDynamic(this, &UPegGameHUD::HandlePegClicked);
	}

	// Wire programmatically-built peg buttons.
	if (bBoardBuiltProgrammatically && PegButtons.Num() == NUM_PEGS)
	{
		typedef void (UPegGameHUD::*FPegClickHandler)();
		FPegClickHandler PegHandlers[NUM_PEGS] = {
			&UPegGameHUD::OnPeg0,  &UPegGameHUD::OnPeg1,
			&UPegGameHUD::OnPeg2,  &UPegGameHUD::OnPeg3,
			&UPegGameHUD::OnPeg4,  &UPegGameHUD::OnPeg5,
			&UPegGameHUD::OnPeg6,  &UPegGameHUD::OnPeg7,
			&UPegGameHUD::OnPeg8,  &UPegGameHUD::OnPeg9,
			&UPegGameHUD::OnPeg10, &UPegGameHUD::OnPeg11,
			&UPegGameHUD::OnPeg12, &UPegGameHUD::OnPeg13,
			&UPegGameHUD::OnPeg14
		};

		for (int32 i = 0; i < NUM_PEGS; ++i)
		{
			if (PegButtons[i])
			{
				PegButtons[i]->OnClicked.AddDynamic(this, PegHandlers[i]);
			}
		}
	}

	// Wire action buttons.
	if (UndoButton)
	{
		UndoButton->OnClicked.AddDynamic(this, &UPegGameHUD::HandleUndoClicked);
	}

	if (ResetButton)
	{
		ResetButton->OnClicked.AddDynamic(this, &UPegGameHUD::HandleResetClicked);
	}

	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddDynamic(this, &UPegGameHUD::HandleNewGameClicked);
	}
}

// ---------------------------------------------------------------------------
// NativeTick
// ---------------------------------------------------------------------------

void UPegGameHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UPegGameHUD::SetPegGameMode(APegGameMode* InGameMode)
{
	PegGameModeRef = InGameMode;
	SetGameMode(InGameMode);

	if (PegGameModeRef)
	{
		PegGameModeRef->OnPegSelected.AddDynamic(this, &UPegGameHUD::HandlePegSelected);
		PegGameModeRef->OnPegJumped.AddDynamic(this, &UPegGameHUD::HandlePegJumped);
		PegGameModeRef->OnPegGameOver.AddDynamic(this, &UPegGameHUD::HandlePegGameOver);
	}

	// If Blueprint-provided PegBoard, initialize it.
	if (PegBoardWidget)
	{
		PegBoardWidget->InitializeBoard();
	}

	RefreshDisplay();
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UPegGameHUD::HandlePegClicked(int32 Index)
{
	if (PegGameModeRef)
	{
		PegGameModeRef->SelectPeg(Index);
	}
}

void UPegGameHUD::HandlePegSelected(int32 Position)
{
	if (!PegGameModeRef)
	{
		return;
	}

	// --- Blueprint PegBoard path ---
	if (PegBoardWidget)
	{
		PegBoardWidget->ClearHighlights();

		TArray<bool> PegStates = PegGameModeRef->GetPegPositions();
		PegBoardWidget->UpdatePegs(PegStates);

		if (Position >= 0)
		{
			PegBoardWidget->HighlightSelected(Position);

			TArray<int32> ValidJumps = PegGameModeRef->GetValidJumps(Position);
			PegBoardWidget->HighlightValidJumps(ValidJumps);
		}
		return;
	}

	// --- Programmatic board path ---
	if (!bBoardBuiltProgrammatically)
	{
		return;
	}

	ClearPegHighlights();
	UpdateAllPegVisuals();
	HighlightedPeg = Position;

	if (Position >= 0)
	{
		HighlightPeg(Position);

		TArray<int32> ValidJumps = PegGameModeRef->GetValidJumps(Position);
		HighlightValidDestinations(ValidJumps);

		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Select a destination to jump to")));
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
	else
	{
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Select a peg to jump")));
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}

void UPegGameHUD::HandlePegJumped(int32 From, int32 Over, int32 To)
{
	// Blueprint PegBoard path.
	if (PegBoardWidget)
	{
		PegBoardWidget->AnimateJump(From, Over, To);
	}

	// Programmatic path: flash the jumped-over peg red briefly.
	if (bBoardBuiltProgrammatically)
	{
		if (PegBorders.IsValidIndex(Over) && PegBorders[Over])
		{
			PegBorders[Over]->SetBrushColor(FLinearColor::Red);
			PegBorders[Over]->SetRenderOpacity(0.6f);
		}
	}

	HighlightedPeg = -1;
	RefreshDisplay();
}

void UPegGameHUD::HandlePegGameOver(int32 PegsRemaining)
{
	FString Msg;
	if (PegsRemaining == 1)
	{
		Msg = TEXT("Perfect! 1 peg remaining!");
	}
	else if (PegsRemaining == 2)
	{
		Msg = FString::Printf(TEXT("Great! %d pegs remaining."), PegsRemaining);
	}
	else if (PegsRemaining == 3)
	{
		Msg = FString::Printf(TEXT("Good! %d pegs remaining."), PegsRemaining);
	}
	else
	{
		Msg = FString::Printf(TEXT("Game Over! %d pegs remaining."), PegsRemaining);
	}

	if (PegsRemainingText)
	{
		PegsRemainingText->SetText(FText::FromString(Msg));
	}

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Msg));
		StatusText->SetColorAndOpacity(FSlateColor(
			PegsRemaining == 1
				? FLinearColor(0.2f, 0.9f, 0.3f)
				: FLinearColor(0.9f, 0.6f, 0.2f)));
	}
}

void UPegGameHUD::HandleUndoClicked()
{
	if (PegGameModeRef)
	{
		PegGameModeRef->UndoLastMove();
		HighlightedPeg = -1;
		RefreshDisplay();

		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Move undone. Select a peg to jump")));
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}

void UPegGameHUD::HandleResetClicked()
{
	if (PegGameModeRef)
	{
		PegGameModeRef->StartGame();
		HighlightedPeg = -1;
		RefreshDisplay();

		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("Board reset. Select a peg to jump")));
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}

void UPegGameHUD::HandleNewGameClicked()
{
	if (PegGameModeRef)
	{
		PegGameModeRef->StartGame();
		HighlightedPeg = -1;
		RefreshDisplay();

		if (StatusText)
		{
			StatusText->SetText(FText::FromString(TEXT("New game! Select a peg to jump")));
			StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
	}
}

// ---------------------------------------------------------------------------
// Peg visual helpers
// ---------------------------------------------------------------------------

void UPegGameHUD::ComputePegPositions()
{
	PegPositions.Empty();
	PegPositions.SetNum(NUM_PEGS);

	/*
	 * Triangle layout (centered within BOARD_WIDTH x BOARD_HEIGHT):
	 *     Row 0: 1 peg  (index 0)
	 *     Row 1: 2 pegs (indices 1-2)
	 *     Row 2: 3 pegs (indices 3-5)
	 *     Row 3: 4 pegs (indices 6-9)
	 *     Row 4: 5 pegs (indices 10-14)
	 */

	const float CenterX = BOARD_WIDTH * 0.5f;
	const float TopPadding = 30.f;

	int32 Idx = 0;
	for (int32 Row = 0; Row < 5; ++Row)
	{
		const int32 PegsInRow = Row + 1;
		const float RowWidth = (PegsInRow - 1) * H_SPACING;
		const float StartX = CenterX - RowWidth * 0.5f;
		const float Y = TopPadding + Row * V_SPACING;

		for (int32 Col = 0; Col < PegsInRow; ++Col)
		{
			PegPositions[Idx] = FVector2D(StartX + Col * H_SPACING, Y);
			Idx++;
		}
	}
}

void UPegGameHUD::UpdateAllPegVisuals()
{
	if (!PegGameModeRef || !bBoardBuiltProgrammatically)
	{
		return;
	}

	TArray<bool> PegStates = PegGameModeRef->GetPegPositions();
	for (int32 i = 0; i < NUM_PEGS && i < PegStates.Num(); ++i)
	{
		SetPegVisual(i, PegStates[i]);
	}
}

void UPegGameHUD::SetPegVisual(int32 Index, bool bHasPeg)
{
	if (!PegBorders.IsValidIndex(Index) || !PegBorders[Index])
	{
		return;
	}

	if (bHasPeg)
	{
		PegBorders[Index]->SetBrushColor(PegYellowColor);
		PegBorders[Index]->SetRenderOpacity(1.0f);
	}
	else
	{
		PegBorders[Index]->SetBrushColor(HoleDarkColor);
		PegBorders[Index]->SetRenderOpacity(0.7f);
	}
}

void UPegGameHUD::HighlightPeg(int32 Index)
{
	if (PegBorders.IsValidIndex(Index) && PegBorders[Index])
	{
		PegBorders[Index]->SetBrushColor(SelectedGreenColor);
		PegBorders[Index]->SetRenderOpacity(1.0f);
	}
}

void UPegGameHUD::HighlightValidDestinations(const TArray<int32>& Destinations)
{
	for (int32 Dest : Destinations)
	{
		if (PegBorders.IsValidIndex(Dest) && PegBorders[Dest])
		{
			PegBorders[Dest]->SetBrushColor(ValidJumpGreenColor);
			PegBorders[Dest]->SetRenderOpacity(1.0f);
		}
	}
}

void UPegGameHUD::ClearPegHighlights()
{
	// Reset all pegs to their state-based colors.
	// Caller should follow with UpdateAllPegVisuals() for proper state.
	for (int32 i = 0; i < NUM_PEGS; ++i)
	{
		if (PegBorders.IsValidIndex(i) && PegBorders[i])
		{
			PegBorders[i]->SetBrushColor(PegYellowColor);
			PegBorders[i]->SetRenderOpacity(1.0f);
		}
	}
}

// ---------------------------------------------------------------------------
// RefreshDisplay - sync all visuals with game state
// ---------------------------------------------------------------------------

void UPegGameHUD::RefreshDisplay()
{
	if (!PegGameModeRef)
	{
		return;
	}

	// Update the programmatic board.
	if (bBoardBuiltProgrammatically)
	{
		ClearPegHighlights();
		UpdateAllPegVisuals();
	}

	// Update the Blueprint PegBoard if present.
	if (PegBoardWidget)
	{
		PegBoardWidget->ClearHighlights();
		TArray<bool> PegStates = PegGameModeRef->GetPegPositions();
		PegBoardWidget->UpdatePegs(PegStates);
	}

	// Update remaining counter.
	if (PegsRemainingText)
	{
		PegsRemainingText->SetText(FText::FromString(
			FString::Printf(TEXT("Pegs: %d"), PegGameModeRef->GetPegsRemaining())));
	}

	// Update moves counter.
	if (MovesCountText)
	{
		MovesCountText->SetText(FText::FromString(
			FString::Printf(TEXT("Moves: %d"), PegGameModeRef->GetMoveCount())));
	}

	// Enable/disable undo button.
	if (UndoButton)
	{
		UndoButton->SetIsEnabled(PegGameModeRef->GetMoveCount() > 0);
	}
}
