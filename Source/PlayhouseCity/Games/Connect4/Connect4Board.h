// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Connect4Board.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UUniformGridPanel;
class AConnect4GameMode;

// ---------------------------------------------------------------------------
// UConnect4Board
// ---------------------------------------------------------------------------

/**
 * UMG widget displaying the Connect 4 board (7 columns x 6 rows),
 * column-selection buttons, and turn/colour indicators.
 *
 * Cell images are created dynamically inside a UUniformGridPanel.
 * The widget communicates with AConnect4GameMode to drop pieces
 * and animate the results.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UConnect4Board : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// -- Public API -------------------------------------------------------

	/** Inject the game-mode reference. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	void SetGameMode(AConnect4GameMode* InGameMode);

	/** Rebuild the entire board visual from game-mode state. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	void InitializeBoard();

	/** Animate a piece dropping into the given column/row for the given player. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	void AnimateDrop(int32 Column, int32 Row, int32 Player);

	/** Set a specific cell to a colour directly (no animation). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	void SetCellColor(int32 Col, int32 Row, FLinearColor Color);

	/** Highlight the four cells forming the winning line. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	void HighlightWinningCells(const TArray<FIntPoint>& Cells);

	/** Reset all cells to the empty colour. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	void ClearBoard();

	/** Enable / disable the column buttons (e.g. during CPU turn). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	void SetInteractable(bool bEnabled);

protected:
	// -- Bind widgets -----------------------------------------------------

	/** Grid panel hosting the 42 cell images. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> GridPanel;

	/** 7 column-selection buttons laid out above the grid. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ColButton0;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ColButton1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ColButton2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ColButton3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ColButton4;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ColButton5;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ColButton6;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TurnIndicatorText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PlayerColorIndicator;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CPUColorIndicator;

	// -- Internal state ---------------------------------------------------

	/** 42 cell images (index = col * NUM_ROWS + row). Row 0 = bottom. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CellImages;

	UPROPERTY(Transient)
	TObjectPtr<AConnect4GameMode> GameModeRef;

	TArray<TObjectPtr<UButton>> ColumnButtons;

	static constexpr int32 NUM_COLS = 7;
	static constexpr int32 NUM_ROWS = 6;

	// -- Callbacks --------------------------------------------------------

	UFUNCTION() void OnCol0Clicked();
	UFUNCTION() void OnCol1Clicked();
	UFUNCTION() void OnCol2Clicked();
	UFUNCTION() void OnCol3Clicked();
	UFUNCTION() void OnCol4Clicked();
	UFUNCTION() void OnCol5Clicked();
	UFUNCTION() void OnCol6Clicked();

	void HandleColumnClicked(int32 Column);

	/** Delegate handlers from game-mode. */
	UFUNCTION()
	void HandlePiecePlaced(int32 Column, int32 Row, int32 Player);

	UFUNCTION()
	void HandleTurnChanged(bool bIsPlayerTurn);

	UFUNCTION()
	void HandleGameOver(int32 Winner);

	// -- Helpers ----------------------------------------------------------

	void BuildGrid();
	int32 CellIndex(int32 Col, int32 Row) const;
	static FLinearColor GetPlayerColor(int32 Player);
};
