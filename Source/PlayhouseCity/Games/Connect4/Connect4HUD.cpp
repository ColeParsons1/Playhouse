// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Connect4/Connect4HUD.h"
#include "Games/Connect4/Connect4GameMode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogConnect4HUD, Log, All);

static const FLinearColor EmptyCellColor(0.12f, 0.12f, 0.2f, 1.f);
static const FLinearColor PlayerColor(0.9f, 0.15f, 0.15f, 1.f);    // Red
static const FLinearColor CPUColor(0.95f, 0.85f, 0.1f, 1.f);       // Yellow
static const FLinearColor BoardBgColor(0.06f, 0.06f, 0.15f, 1.f);
static const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
static const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
static const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Connect4 board UI
// ---------------------------------------------------------------------------

bool UConnect4HUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	if (BoardGrid)
	{
		return true; // Already built.
	}

	// We need to add game-specific widgets to the existing root canvas.
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo StatusFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);

	// ---- Game content area (below top bar, above bottom) ----
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameArea"));
	GameArea->SetBrushColor(BoardBgColor);
	GameArea->SetPadding(FMargin(10.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ---- Turn status text ----
	TurnStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TurnStatus"));
	TurnStatusText->SetText(FText::FromString(TEXT("Your Turn - Drop a piece!")));
	TurnStatusText->SetFont(StatusFont);
	TurnStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TurnStatusText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* StatusSlot = ContentVBox->AddChildToVerticalBox(TurnStatusText);
	StatusSlot->SetHorizontalAlignment(HAlign_Center);
	StatusSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));

	// ---- Column drop buttons (row of 7) ----
	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
	UVerticalBoxSlot* BtnRowSlot = ContentVBox->AddChildToVerticalBox(ButtonRow);
	BtnRowSlot->SetHorizontalAlignment(HAlign_Center);
	BtnRowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	ColumnButtons.SetNum(NUM_COLS);
	for (int32 c = 0; c < NUM_COLS; ++c)
	{
		USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("ColBtnSize%d"), c));
		BtnSize->SetMinDesiredWidth(50.f);
		BtnSize->SetMinDesiredHeight(36.f);

		UButton* Btn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("ColBtn%d"), c));
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnNormal);
		Style.Hovered.TintColor = FSlateColor(BtnHover);
		Style.Pressed.TintColor = FSlateColor(BtnPress);
		Btn->SetStyle(Style);

		UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("ColLbl%d"), c));
		BtnLabel->SetText(FText::FromString(FString::Printf(TEXT("%d"), c + 1)));
		BtnLabel->SetFont(BtnFont);
		BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLabel->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLabel);

		BtnSize->AddChild(Btn);

		UHorizontalBoxSlot* BtnSlot = ButtonRow->AddChildToHorizontalBox(BtnSize);
		BtnSlot->SetPadding(FMargin(2.f, 0.f));
		BtnSlot->SetHorizontalAlignment(HAlign_Center);

		ColumnButtons[c] = Btn;
	}

	// ---- Board grid (7 cols x 6 rows) ----
	// Use a centered container with the grid inside.
	BoardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass(), TEXT("BoardGrid"));
	BoardGrid->SetSlotPadding(FMargin(2.f));

	UVerticalBoxSlot* GridSlot = ContentVBox->AddChildToVerticalBox(BoardGrid);
	GridSlot->SetHorizontalAlignment(HAlign_Center);
	GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	GridSlot->SetPadding(FMargin(0.f, 4.f));

	// Create 7x6 cells. UniformGridPanel layout: we place them row by row.
	// Row 0 in game = bottom, but in the grid widget row 0 = top.
	// So we invert: visual row 0 = game row 5 (top).
	CellWidgets.SetNum(NUM_COLS * NUM_ROWS);

	for (int32 visualRow = 0; visualRow < NUM_ROWS; ++visualRow)
	{
		const int32 gameRow = (NUM_ROWS - 1) - visualRow; // Invert

		for (int32 col = 0; col < NUM_COLS; ++col)
		{
			USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(),
				*FString::Printf(TEXT("CellSize_%d_%d"), col, gameRow));
			CellSize->SetMinDesiredWidth(48.f);
			CellSize->SetMinDesiredHeight(48.f);

			UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				*FString::Printf(TEXT("Cell_%d_%d"), col, gameRow));
			Cell->SetBrushColor(EmptyCellColor);
			Cell->SetPadding(FMargin(0.f));

			CellSize->AddChild(Cell);
			BoardGrid->AddChildToUniformGrid(CellSize, visualRow, col);

			const int32 Idx = col * NUM_ROWS + gameRow;
			CellWidgets[Idx] = Cell;
		}
	}

	UE_LOG(LogConnect4HUD, Log, TEXT("Connect4 board built: %d cells, %d buttons"),
		CellWidgets.Num(), ColumnButtons.Num());

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates
// ---------------------------------------------------------------------------

void UConnect4HUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Get the Connect4 game mode from the base class GameModeRef.
	Connect4Mode = Cast<AConnect4GameMode>(GameModeRef);

	if (Connect4Mode)
	{
		Connect4Mode->OnPiecePlaced.AddDynamic(this, &UConnect4HUD::HandlePiecePlaced);
		Connect4Mode->OnTurnChanged.AddDynamic(this, &UConnect4HUD::HandleTurnChanged);
		Connect4Mode->OnGameOver.AddDynamic(this, &UConnect4HUD::HandleGameOver);

		// Initialize the board display.
		UpdateAllCells();
		HandleTurnChanged(Connect4Mode->IsPlayerTurn());
	}

	// Wire column buttons.
	if (ColumnButtons.Num() >= NUM_COLS)
	{
		if (ColumnButtons[0]) ColumnButtons[0]->OnClicked.AddDynamic(this, &UConnect4HUD::OnCol0);
		if (ColumnButtons[1]) ColumnButtons[1]->OnClicked.AddDynamic(this, &UConnect4HUD::OnCol1);
		if (ColumnButtons[2]) ColumnButtons[2]->OnClicked.AddDynamic(this, &UConnect4HUD::OnCol2);
		if (ColumnButtons[3]) ColumnButtons[3]->OnClicked.AddDynamic(this, &UConnect4HUD::OnCol3);
		if (ColumnButtons[4]) ColumnButtons[4]->OnClicked.AddDynamic(this, &UConnect4HUD::OnCol4);
		if (ColumnButtons[5]) ColumnButtons[5]->OnClicked.AddDynamic(this, &UConnect4HUD::OnCol5);
		if (ColumnButtons[6]) ColumnButtons[6]->OnClicked.AddDynamic(this, &UConnect4HUD::OnCol6);
	}
}

// ---------------------------------------------------------------------------
// Cell updates
// ---------------------------------------------------------------------------

void UConnect4HUD::UpdateCellColor(int32 Col, int32 Row, int32 Player)
{
	const int32 Idx = Col * NUM_ROWS + Row;
	if (!CellWidgets.IsValidIndex(Idx) || !CellWidgets[Idx])
	{
		return;
	}

	FLinearColor Color = EmptyCellColor;
	if (Player == 1) Color = PlayerColor;
	else if (Player == 2) Color = CPUColor;

	CellWidgets[Idx]->SetBrushColor(Color);
}

void UConnect4HUD::UpdateAllCells()
{
	if (!Connect4Mode)
	{
		return;
	}

	for (int32 c = 0; c < NUM_COLS; ++c)
	{
		for (int32 r = 0; r < NUM_ROWS; ++r)
		{
			UpdateCellColor(c, r, Connect4Mode->GetCell(c, r));
		}
	}
}

void UConnect4HUD::SetColumnButtonsEnabled(bool bEnabled)
{
	for (int32 c = 0; c < ColumnButtons.Num(); ++c)
	{
		if (ColumnButtons[c])
		{
			ColumnButtons[c]->SetIsEnabled(bEnabled);
		}
	}
}

// ---------------------------------------------------------------------------
// Game mode callbacks
// ---------------------------------------------------------------------------

void UConnect4HUD::HandlePiecePlaced(int32 Column, int32 Row, int32 Player)
{
	UpdateCellColor(Column, Row, Player);
}

void UConnect4HUD::HandleTurnChanged(bool bIsPlayerTurn)
{
	SetColumnButtonsEnabled(bIsPlayerTurn);

	if (TurnStatusText)
	{
		TurnStatusText->SetText(FText::FromString(
			bIsPlayerTurn
				? TEXT("Your Turn - Drop a piece!")
				: TEXT("CPU is thinking...")));
		TurnStatusText->SetColorAndOpacity(FSlateColor(
			bIsPlayerTurn ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.7f)));
	}
}

void UConnect4HUD::HandleGameOver(int32 Winner)
{
	SetColumnButtonsEnabled(false);

	if (TurnStatusText)
	{
		FString ResultText;
		if (Winner == 1)
		{
			ResultText = TEXT("You Win!");
		}
		else if (Winner == 2)
		{
			ResultText = TEXT("CPU Wins!");
		}
		else
		{
			ResultText = TEXT("Draw!");
		}
		TurnStatusText->SetText(FText::FromString(ResultText));
		TurnStatusText->SetColorAndOpacity(FSlateColor(
			Winner == 1 ? FLinearColor(0.2f, 0.9f, 0.3f) : FLinearColor(0.9f, 0.3f, 0.2f)));
	}
}

// ---------------------------------------------------------------------------
// Player input
// ---------------------------------------------------------------------------

void UConnect4HUD::DropInColumn(int32 Col)
{
	if (Connect4Mode && Connect4Mode->IsPlayerTurn())
	{
		Connect4Mode->DropPiece(Col);
	}
}
