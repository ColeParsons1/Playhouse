// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PegBoard.generated.h"

class UButton;
class UImage;
class UCanvasPanel;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPegBoardPegClicked, int32, Index);

// ---------------------------------------------------------------------------
// UPegBoard
// ---------------------------------------------------------------------------

/**
 * UMG widget that displays the triangle peg solitaire board.
 * 15 circular button/image pairs arranged in a triangle (1+2+3+4+5 rows).
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UPegBoard : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Create and position the 15 peg slots on the canvas. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void InitializeBoard();

	/** Update peg visibility based on the current game state. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void UpdatePegs(const TArray<bool>& PegStates);

	/** Highlight a selected peg. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void HighlightSelected(int32 SelectedIndex);

	/** Highlight valid jump destinations for the selected peg. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void HighlightValidJumps(const TArray<int32>& Destinations);

	/** Play a jump animation from source over middle to destination. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void AnimateJump(int32 From, int32 Over, int32 To);

	/** Get the screen position of a peg hole for external drawing. */
	UFUNCTION(BlueprintPure, Category = "PHC|PegGame")
	FVector2D GetPegScreenPosition(int32 Index) const;

	/** Clear all highlights. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void ClearHighlights();

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|PegGame")
	FOnPegBoardPegClicked OnPegClicked;

	// -- Constants --------------------------------------------------------

	static constexpr int32 NUM_PEGS = 15;

protected:
	virtual void NativeConstruct() override;

	// -- Bind widgets -----------------------------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> BoardCanvas;

	// -- Configuration ----------------------------------------------------

	/** Diameter of each peg hole in pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PegGame")
	float PegSize = 40.f;

	/** Horizontal spacing between peg centers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PegGame")
	float HorizontalSpacing = 60.f;

	/** Vertical spacing between rows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PegGame")
	float VerticalSpacing = 52.f;

	/** Top-center starting position of the triangle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PegGame")
	FVector2D BoardOrigin = FVector2D(250.f, 40.f);

private:
	/** Dynamically created peg button widgets. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> PegButtons;

	/** Peg visual images (filled circle or empty hole). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PegImages;

	/** Precomputed screen positions for each of the 15 holes. */
	TArray<FVector2D> PegPositions;

	/** Compute the triangle layout positions. */
	void ComputePositions();

	/** Button click handler - routes through index. */
	UFUNCTION()
	void HandlePeg0();  UFUNCTION() void HandlePeg1();
	UFUNCTION() void HandlePeg2();  UFUNCTION() void HandlePeg3();
	UFUNCTION() void HandlePeg4();  UFUNCTION() void HandlePeg5();
	UFUNCTION() void HandlePeg6();  UFUNCTION() void HandlePeg7();
	UFUNCTION() void HandlePeg8();  UFUNCTION() void HandlePeg9();
	UFUNCTION() void HandlePeg10(); UFUNCTION() void HandlePeg11();
	UFUNCTION() void HandlePeg12(); UFUNCTION() void HandlePeg13();
	UFUNCTION() void HandlePeg14();

	void HandlePegAtIndex(int32 Index);

	/** Color constants. */
	static const FLinearColor PegColor;
	static const FLinearColor EmptyColor;
	static const FLinearColor HighlightColor;
	static const FLinearColor SelectedColor;
};
