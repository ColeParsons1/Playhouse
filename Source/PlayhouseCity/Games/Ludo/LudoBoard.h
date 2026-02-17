// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Games/Ludo/LudoGameMode.h"
#include "LudoBoard.generated.h"

class UCanvasPanel;
class UButton;
class UImage;
class UTextBlock;
class UCanvasPanelSlot;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLudoBoardDiceClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLudoBoardPieceClicked, int32, PieceIndex);

// ---------------------------------------------------------------------------
// ULudoBoard
// ---------------------------------------------------------------------------

/**
 * UMG widget that renders the Ludo board: cross-shaped track, four colored
 * home areas, 16 piece images, dice button, and turn indicator.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ULudoBoard : public UUserWidget
{
	GENERATED_BODY()

public:
	// -- Initialization ---------------------------------------------------

	/** Set up the board visuals, piece images, and position lookup table. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void InitializeBoard();

	// -- Updates ----------------------------------------------------------

	/** Reposition all 16 piece images based on current game state. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void UpdatePieces(const TArray<FPHCLudoPiece>& AllPieces);

	/** Show the dice result with a brief visual feedback. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void ShowDiceResult(int32 Value);

	/** Highlight pieces that can be moved this turn. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void HighlightValidMoves(const TArray<int32>& PieceIndices);

	/** Animate a piece moving between two board positions. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void AnimateMovement(int32 PieceIndex, int32 FromPos, int32 ToPos);

	/** Visual feedback when a piece is captured. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void ShowCapture(int32 CapturedPieceIndex);

	// -- Coordinate mapping -----------------------------------------------

	/** Map a track position (0-57) to pixel coordinates on the board canvas. */
	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	FVector2D GetBoardPosition(int32 TrackPosition, EPHCLudoColor Color) const;

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Ludo")
	FOnLudoBoardDiceClicked OnDiceClicked;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Ludo")
	FOnLudoBoardPieceClicked OnPieceClicked;

protected:
	virtual void NativeConstruct() override;

	// -- Bind widgets -----------------------------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> BoardCanvas;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DiceButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> DiceImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TurnText;

	// -- Piece images (16 total: 4 per player) ----------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Ludo")
	TArray<TObjectPtr<UImage>> PieceImages;

	// -- Board size for coordinate mapping --------------------------------

	/** Logical board size used for mapping track positions to pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Ludo")
	float BoardSize = 600.f;

	/** Offset from the canvas edge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Ludo")
	FVector2D BoardOffset = FVector2D(50.f, 50.f);

	/** Cell size for positioning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Ludo")
	float CellSize = 40.f;

private:
	/** Internal lookup: absolute track position -> pixel coordinate. */
	TArray<FVector2D> TrackPositionLUT;

	/** Home yard positions for each color (4 positions per color). */
	TArray<FVector2D> HomeYardPositions;

	/** Build the position lookup tables. */
	void BuildPositionLUT();

	/** Button callbacks. */
	UFUNCTION()
	void HandleDiceClicked();

	/** Get the home yard offset for stacking pieces of same color. */
	FVector2D GetHomeYardPosition(EPHCLudoColor Color, int32 PieceSubIndex) const;
};
