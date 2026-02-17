// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Connect4/Connect4Board.h"
#include "Games/Connect4/Connect4GameMode.h"
#include "Components/UniformGridPanel.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

// Colors
static const FLinearColor EmptyColor(0.2f, 0.2f, 0.25f, 1.f);
static const FLinearColor Player1Color(0.9f, 0.2f, 0.2f, 1.f); // Red
static const FLinearColor Player2Color(0.9f, 0.9f, 0.1f, 1.f); // Yellow
static const FLinearColor WinHighlightColor(0.2f, 1.f, 0.2f, 1.f); // Green

void UConnect4Board::NativeConstruct()
{
	Super::NativeConstruct();

	// Populate column buttons array
	ColumnButtons.Empty();
	ColumnButtons.Add(ColButton0);
	ColumnButtons.Add(ColButton1);
	ColumnButtons.Add(ColButton2);
	ColumnButtons.Add(ColButton3);
	ColumnButtons.Add(ColButton4);
	ColumnButtons.Add(ColButton5);
	ColumnButtons.Add(ColButton6);

	// Bind column button clicks
	if (ColButton0) ColButton0->OnClicked.AddDynamic(this, &UConnect4Board::OnCol0Clicked);
	if (ColButton1) ColButton1->OnClicked.AddDynamic(this, &UConnect4Board::OnCol1Clicked);
	if (ColButton2) ColButton2->OnClicked.AddDynamic(this, &UConnect4Board::OnCol2Clicked);
	if (ColButton3) ColButton3->OnClicked.AddDynamic(this, &UConnect4Board::OnCol3Clicked);
	if (ColButton4) ColButton4->OnClicked.AddDynamic(this, &UConnect4Board::OnCol4Clicked);
	if (ColButton5) ColButton5->OnClicked.AddDynamic(this, &UConnect4Board::OnCol5Clicked);
	if (ColButton6) ColButton6->OnClicked.AddDynamic(this, &UConnect4Board::OnCol6Clicked);

	// Set player/CPU color indicators
	if (PlayerColorIndicator)
	{
		PlayerColorIndicator->SetColorAndOpacity(Player1Color);
	}
	if (CPUColorIndicator)
	{
		CPUColorIndicator->SetColorAndOpacity(Player2Color);
	}
}

void UConnect4Board::SetGameMode(AConnect4GameMode* InGameMode)
{
	GameModeRef = InGameMode;
	if (GameModeRef)
	{
		GameModeRef->OnPiecePlaced.AddDynamic(this, &UConnect4Board::HandlePiecePlaced);
		GameModeRef->OnTurnChanged.AddDynamic(this, &UConnect4Board::HandleTurnChanged);
		GameModeRef->OnGameOver.AddDynamic(this, &UConnect4Board::HandleGameOver);
	}
}

void UConnect4Board::InitializeBoard()
{
	BuildGrid();
	ClearBoard();
	SetInteractable(true);
	if (TurnIndicatorText)
	{
		TurnIndicatorText->SetText(FText::FromString(TEXT("Your Turn")));
	}
}

void UConnect4Board::BuildGrid()
{
	if (!GridPanel)
	{
		return;
	}

	GridPanel->ClearChildren();
	CellImages.Empty();
	CellImages.SetNum(NUM_COLS * NUM_ROWS);

	// Build grid: Row 0 is bottom, so we iterate top-to-bottom for visual layout
	for (int32 VisRow = 0; VisRow < NUM_ROWS; ++VisRow)
	{
		const int32 DataRow = NUM_ROWS - 1 - VisRow; // Flip: top visual row = highest data row
		for (int32 Col = 0; Col < NUM_COLS; ++Col)
		{
			UImage* CellImage = NewObject<UImage>(this);
			if (CellImage)
			{
				CellImage->SetColorAndOpacity(EmptyColor);
				GridPanel->AddChildToUniformGrid(CellImage, VisRow, Col);
				CellImages[CellIndex(Col, DataRow)] = CellImage;
			}
		}
	}
}

void UConnect4Board::AnimateDrop(int32 Column, int32 Row, int32 Player)
{
	SetCellColor(Column, Row, GetPlayerColor(Player));
}

void UConnect4Board::SetCellColor(int32 Col, int32 Row, FLinearColor Color)
{
	const int32 Idx = CellIndex(Col, Row);
	if (CellImages.IsValidIndex(Idx) && CellImages[Idx])
	{
		CellImages[Idx]->SetColorAndOpacity(Color);
	}
}

void UConnect4Board::HighlightWinningCells(const TArray<FIntPoint>& Cells)
{
	for (const FIntPoint& Cell : Cells)
	{
		SetCellColor(Cell.X, Cell.Y, WinHighlightColor);
	}
}

void UConnect4Board::ClearBoard()
{
	for (UImage* Img : CellImages)
	{
		if (Img)
		{
			Img->SetColorAndOpacity(EmptyColor);
		}
	}
}

void UConnect4Board::SetInteractable(bool bEnabled)
{
	for (UButton* Btn : ColumnButtons)
	{
		if (Btn)
		{
			Btn->SetIsEnabled(bEnabled);
		}
	}
}

// -- Column click callbacks --
void UConnect4Board::OnCol0Clicked() { HandleColumnClicked(0); }
void UConnect4Board::OnCol1Clicked() { HandleColumnClicked(1); }
void UConnect4Board::OnCol2Clicked() { HandleColumnClicked(2); }
void UConnect4Board::OnCol3Clicked() { HandleColumnClicked(3); }
void UConnect4Board::OnCol4Clicked() { HandleColumnClicked(4); }
void UConnect4Board::OnCol5Clicked() { HandleColumnClicked(5); }
void UConnect4Board::OnCol6Clicked() { HandleColumnClicked(6); }

void UConnect4Board::HandleColumnClicked(int32 Column)
{
	if (GameModeRef && GameModeRef->IsPlayerTurn())
	{
		GameModeRef->DropPiece(Column);
	}
}

// -- Delegate handlers --
void UConnect4Board::HandlePiecePlaced(int32 Column, int32 Row, int32 Player)
{
	AnimateDrop(Column, Row, Player);
}

void UConnect4Board::HandleTurnChanged(bool bIsPlayerTurn)
{
	SetInteractable(bIsPlayerTurn);
	if (TurnIndicatorText)
	{
		TurnIndicatorText->SetText(FText::FromString(
			bIsPlayerTurn ? TEXT("Your Turn") : TEXT("CPU Thinking...")));
	}
}

void UConnect4Board::HandleGameOver(int32 Winner)
{
	SetInteractable(false);
	if (TurnIndicatorText)
	{
		if (Winner == 1)
		{
			TurnIndicatorText->SetText(FText::FromString(TEXT("You Win!")));
		}
		else if (Winner == 2)
		{
			TurnIndicatorText->SetText(FText::FromString(TEXT("CPU Wins!")));
		}
		else
		{
			TurnIndicatorText->SetText(FText::FromString(TEXT("Draw!")));
		}
	}
}

// -- Helpers --
int32 UConnect4Board::CellIndex(int32 Col, int32 Row) const
{
	return Col * NUM_ROWS + Row;
}

FLinearColor UConnect4Board::GetPlayerColor(int32 Player)
{
	switch (Player)
	{
	case 1: return Player1Color;
	case 2: return Player2Color;
	default: return EmptyColor;
	}
}
