// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "BingoCard.generated.h"

class UTextBlock;
class UUniformGridPanel;
class UHorizontalBox;
class ABingoGameMode;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Fired when the player taps a cell on the bingo card. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBingoCellClicked, int32, Row, int32, Col);

// ---------------------------------------------------------------------------
// UBingoCellButton -- tiny helper so each cell knows its own index
// ---------------------------------------------------------------------------

/**
 * Wrapper around UButton that carries a cell index so the parent widget
 * can identify which cell was clicked from a shared delegate handler.
 */
UCLASS()
class PLAYHOUSECITY_API UBingoCellButton : public UButton
{
	GENERATED_BODY()

public:
	/** Flat index into the 5x5 grid (row * 5 + col). */
	UPROPERTY()
	int32 CellIndex = 0;
};

// ---------------------------------------------------------------------------
// UBingoCard
// ---------------------------------------------------------------------------

/**
 * UMG widget that renders a 5x5 bingo card with clickable cells,
 * a last-called-number display, and a "Bingo!" button.
 *
 * The widget dynamically creates 25 UBingoCellButton instances inside
 * a UUniformGridPanel and highlights cells that match called numbers.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UBingoCard : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// -- Public API -------------------------------------------------------

	/** Populate the grid from a flat array of 25 numbers (row-major). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void SetCard(const TArray<int32>& CardDataFlat);

	/** Visually mark a cell (green if correct, red if not). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void MarkCell(int32 Row, int32 Col, bool bCorrect);

	/** Highlight every cell that contains the given number. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void HighlightCalledNumber(int32 Number);

	/** Animate the winning line through the given cell positions. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void SetBingoLine(const TArray<FIntPoint>& WinningCells);

	/** Inject the game-mode reference for mark validation. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void SetGameMode(ABingoGameMode* InGameMode);

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Bingo")
	FOnBingoCellClicked OnCellClicked;

protected:
	// -- Bind widgets -----------------------------------------------------

	/** 5x5 grid panel that hosts the cell buttons. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> GridPanel;

	/** Shows the most recently called number prominently. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CalledNumberText;

	/** Horizontal box displaying the last 5 called numbers. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> LastCalledNumbers;

	/** The "Bingo!" declaration button. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BingoButton;

	// -- Internal state ---------------------------------------------------

	/** 25 dynamically-created cell buttons (row-major index). */
	UPROPERTY(Transient)
	TArray<UBingoCellButton*> CellButtons;

	/** 25 text blocks placed inside each button. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CellTexts;

	/** Flat card data for reverse-lookup. */
	TArray<int32> CardData;

	/** Cached game-mode pointer. */
	UPROPERTY(Transient)
	TObjectPtr<ABingoGameMode> GameModeRef;

	// -- Callbacks --------------------------------------------------------

	/** Shared handler bound to every cell button's OnClicked. Identifies the
	 *  source by scanning CellButtons for the one whose OnClicked just fired. */
	UFUNCTION()
	void HandleAnyCellClicked();

	UFUNCTION()
	void HandleBingoClicked();

private:
	/** Create the 25 cell buttons inside GridPanel. */
	void BuildGrid();

	/** Convenience: convert (row, col) to flat index. */
	static int32 ToIndex(int32 Row, int32 Col) { return Row * 5 + Col; }

	/** Track which cell was most recently clicked (set inside the delegate). */
	int32 LastClickedCellIndex = -1;
};
