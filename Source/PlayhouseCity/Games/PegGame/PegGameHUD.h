// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "PegGameHUD.generated.h"

class UPegBoard;
class UTextBlock;
class UButton;
class UBorder;
class UImage;
class UCanvasPanel;
class APegGameMode;

// ---------------------------------------------------------------------------
// UPegGameHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the Peg Game (triangle peg solitaire).
 *
 * Builds the complete board UI programmatically in Initialize():
 *  - A triangular arrangement of 15 peg buttons (rows 1-5)
 *  - Pegs-remaining counter and moves counter
 *  - Undo and New Game buttons
 *  - Status text for game-over messages
 *
 * If a Blueprint supplies a UPegBoard via BindWidgetOptional, that
 * embedded widget is used instead of the programmatically-built board.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UPegGameHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Bind to the PegGame-specific game mode. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void SetPegGameMode(APegGameMode* InGameMode);

protected:
	// -- Bind widgets (Blueprint-provided, optional) -------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPegBoard> PegBoardWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PegsRemainingText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MovesCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> UndoButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetButton;

	// -- Programmatically-built widgets -------------------------------------

	/** Canvas that holds the 15 peg buttons in a triangle layout. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> BoardCanvas;

	/** Brown background border behind the board. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BoardBackground;

	/** Status / instruction text (shown above the board). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** New Game button (built programmatically). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewGameButton;

	/** The 15 peg buttons arranged in a triangle. */
	UPROPERTY()
	TArray<TObjectPtr<UButton>> PegButtons;

	/** The 15 peg border widgets used as circular peg visuals. */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> PegBorders;

	/** Precomputed center positions for each of the 15 holes. */
	TArray<FVector2D> PegPositions;

	/** Whether we built the board programmatically (vs Blueprint UPegBoard). */
	bool bBoardBuiltProgrammatically = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<APegGameMode> PegGameModeRef;

	/** Currently selected peg index for visual highlighting (-1 = none). */
	int32 HighlightedPeg = -1;

	// -- Board layout constants -------------------------------------------

	static constexpr int32 NUM_PEGS = 15;
	static constexpr float PEG_SIZE = 50.f;
	static constexpr float H_SPACING = 62.f;
	static constexpr float V_SPACING = 56.f;
	static constexpr float BOARD_WIDTH = 360.f;
	static constexpr float BOARD_HEIGHT = 320.f;

	// -- Color constants --------------------------------------------------

	static const FLinearColor BoardBrownColor;
	static const FLinearColor PegYellowColor;
	static const FLinearColor HoleDarkColor;
	static const FLinearColor SelectedGreenColor;
	static const FLinearColor ValidJumpGreenColor;
	static const FLinearColor GameAreaBgColor;

	// -- Delegate handlers ------------------------------------------------

	UFUNCTION()
	void HandlePegClicked(int32 Index);

	UFUNCTION()
	void HandlePegSelected(int32 Position);

	UFUNCTION()
	void HandlePegJumped(int32 From, int32 Over, int32 To);

	UFUNCTION()
	void HandlePegGameOver(int32 PegsRemaining);

	UFUNCTION()
	void HandleUndoClicked();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleNewGameClicked();

	// -- Per-peg click handlers (needed for UButton delegate binding) ------

	UFUNCTION() void OnPeg0()  { HandlePegClicked(0); }
	UFUNCTION() void OnPeg1()  { HandlePegClicked(1); }
	UFUNCTION() void OnPeg2()  { HandlePegClicked(2); }
	UFUNCTION() void OnPeg3()  { HandlePegClicked(3); }
	UFUNCTION() void OnPeg4()  { HandlePegClicked(4); }
	UFUNCTION() void OnPeg5()  { HandlePegClicked(5); }
	UFUNCTION() void OnPeg6()  { HandlePegClicked(6); }
	UFUNCTION() void OnPeg7()  { HandlePegClicked(7); }
	UFUNCTION() void OnPeg8()  { HandlePegClicked(8); }
	UFUNCTION() void OnPeg9()  { HandlePegClicked(9); }
	UFUNCTION() void OnPeg10() { HandlePegClicked(10); }
	UFUNCTION() void OnPeg11() { HandlePegClicked(11); }
	UFUNCTION() void OnPeg12() { HandlePegClicked(12); }
	UFUNCTION() void OnPeg13() { HandlePegClicked(13); }
	UFUNCTION() void OnPeg14() { HandlePegClicked(14); }

	// -- Internal helpers -------------------------------------------------

	/** Compute the triangle layout positions for 15 holes. */
	void ComputePegPositions();

	/** Update the visual state of all 15 pegs from game mode state. */
	void UpdateAllPegVisuals();

	/** Update a single peg border color based on state. */
	void SetPegVisual(int32 Index, bool bHasPeg);

	/** Highlight a single peg as selected. */
	void HighlightPeg(int32 Index);

	/** Highlight valid jump destination pegs. */
	void HighlightValidDestinations(const TArray<int32>& Destinations);

	/** Clear all peg highlights back to normal state. */
	void ClearPegHighlights();

	/** Refresh all display elements from game state. */
	void RefreshDisplay();
};
