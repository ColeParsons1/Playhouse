// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Bingo/BingoCard.h"
#include "Games/Bingo/BingoGameMode.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

static constexpr int32 BINGO_GRID = 5;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UBingoCard::NativeConstruct()
{
	Super::NativeConstruct();

	BuildGrid();

	if (BingoButton)
	{
		BingoButton->OnClicked.AddDynamic(this, &UBingoCard::HandleBingoClicked);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UBingoCard::SetCard(const TArray<int32>& CardDataFlat)
{
	CardData = CardDataFlat;

	for (int32 i = 0; i < FMath::Min(CardDataFlat.Num(), CellTexts.Num()); ++i)
	{
		if (CellTexts[i])
		{
			// Free space (centre) is stored as 0.
			if (CardDataFlat[i] == 0)
			{
				CellTexts[i]->SetText(FText::FromString(TEXT("FREE")));
			}
			else
			{
				CellTexts[i]->SetText(FText::AsNumber(CardDataFlat[i]));
			}
		}
	}
}

void UBingoCard::MarkCell(int32 Row, int32 Col, bool bCorrect)
{
	const int32 Idx = ToIndex(Row, Col);
	if (!CellButtons.IsValidIndex(Idx) || !CellButtons[Idx])
	{
		return;
	}

	FButtonStyle Style = CellButtons[Idx]->GetStyle();
	const FLinearColor Color = bCorrect
		? FLinearColor(0.1f, 0.7f, 0.2f, 1.f) // green
		: FLinearColor(0.8f, 0.1f, 0.1f, 1.f); // red

	Style.Normal.TintColor = FSlateColor(Color);
	Style.Hovered.TintColor = FSlateColor(Color);
	Style.Pressed.TintColor = FSlateColor(Color);
	CellButtons[Idx]->SetStyle(Style);
}

void UBingoCard::HighlightCalledNumber(int32 Number)
{
	// Update the big called-number display.
	if (CalledNumberText)
	{
		CalledNumberText->SetText(FText::AsNumber(Number));
	}

	// Highlight the matching cell on the card (if any).
	for (int32 i = 0; i < CardData.Num(); ++i)
	{
		if (CardData[i] == Number && CellButtons.IsValidIndex(i) && CellButtons[i])
		{
			FButtonStyle Style = CellButtons[i]->GetStyle();
			const FLinearColor Highlight(1.f, 0.85f, 0.f, 1.f); // gold
			Style.Normal.TintColor = FSlateColor(Highlight);
			Style.Hovered.TintColor = FSlateColor(Highlight);
			CellButtons[i]->SetStyle(Style);
		}
	}

	// Add to the rolling "last 5" display.
	if (LastCalledNumbers)
	{
		UTextBlock* NumText = NewObject<UTextBlock>(this);
		if (NumText)
		{
			NumText->SetText(FText::AsNumber(Number));

			FSlateFontInfo Font = NumText->GetFont();
			Font.Size = 18;
			NumText->SetFont(Font);

			UHorizontalBoxSlot* Slot = LastCalledNumbers->AddChildToHorizontalBox(NumText);
			if (Slot)
			{
				Slot->SetPadding(FMargin(4.f, 0.f));
			}
		}

		// Keep at most 5 entries.
		while (LastCalledNumbers->GetChildrenCount() > 5)
		{
			LastCalledNumbers->RemoveChildAt(0);
		}
	}
}

void UBingoCard::SetBingoLine(const TArray<FIntPoint>& WinningCells)
{
	const FLinearColor WinColor(0.f, 1.f, 0.f, 1.f);
	for (const FIntPoint& Cell : WinningCells)
	{
		const int32 Idx = ToIndex(Cell.X, Cell.Y);
		if (CellButtons.IsValidIndex(Idx) && CellButtons[Idx])
		{
			FButtonStyle Style = CellButtons[Idx]->GetStyle();
			Style.Normal.TintColor = FSlateColor(WinColor);
			Style.Hovered.TintColor = FSlateColor(WinColor);
			Style.Pressed.TintColor = FSlateColor(WinColor);
			CellButtons[Idx]->SetStyle(Style);
		}
	}
}

void UBingoCard::SetGameMode(ABingoGameMode* InGameMode)
{
	GameModeRef = InGameMode;
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UBingoCard::HandleAnyCellClicked()
{
	// Identify which button fired by checking CellIndex on each UBingoCellButton.
	// Since OnClicked does not pass the source, we check IsHovered() as a
	// reliable indicator -- the button that was just clicked is still hovered
	// at the moment the delegate fires.
	int32 FoundIndex = -1;
	for (int32 i = 0; i < CellButtons.Num(); ++i)
	{
		if (CellButtons[i] && CellButtons[i]->IsHovered())
		{
			FoundIndex = CellButtons[i]->CellIndex;
			break;
		}
	}

	if (FoundIndex < 0)
	{
		return;
	}

	const int32 Row = FoundIndex / BINGO_GRID;
	const int32 Col = FoundIndex % BINGO_GRID;

	// Attempt to mark via game mode.
	if (GameModeRef)
	{
		const bool bSuccess = GameModeRef->MarkCell(Row, Col);
		MarkCell(Row, Col, bSuccess);
	}

	OnCellClicked.Broadcast(Row, Col);
}

void UBingoCard::HandleBingoClicked()
{
	if (GameModeRef)
	{
		GameModeRef->OnBingoCalled();
	}
}

// ---------------------------------------------------------------------------
// Grid construction
// ---------------------------------------------------------------------------

void UBingoCard::BuildGrid()
{
	if (!GridPanel)
	{
		return;
	}

	CellButtons.Reset();
	CellTexts.Reset();
	CellButtons.SetNum(BINGO_GRID * BINGO_GRID);
	CellTexts.SetNum(BINGO_GRID * BINGO_GRID);

	GridPanel->ClearChildren();

	for (int32 Row = 0; Row < BINGO_GRID; ++Row)
	{
		for (int32 Col = 0; Col < BINGO_GRID; ++Col)
		{
			const int32 Idx = ToIndex(Row, Col);

			UBingoCellButton* Btn = NewObject<UBingoCellButton>(this);
			UTextBlock* Txt = NewObject<UTextBlock>(this);

			if (Btn && Txt)
			{
				Btn->CellIndex = Idx;

				// Put the text inside the button.
				Btn->AddChild(Txt);

				FSlateFontInfo Font = Txt->GetFont();
				Font.Size = 20;
				Txt->SetFont(Font);
				Txt->SetJustification(ETextJustify::Center);

				// Place into the grid panel.
				GridPanel->AddChildToUniformGrid(Btn, Row, Col);

				// All cell buttons share the same handler; identification
				// happens via IsHovered() + CellIndex inside the handler.
				Btn->OnClicked.AddDynamic(this, &UBingoCard::HandleAnyCellClicked);

				CellButtons[Idx] = Btn;
				CellTexts[Idx] = Txt;
			}
		}
	}
}
