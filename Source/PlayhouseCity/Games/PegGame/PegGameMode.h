// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "PegGameMode.generated.h"

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** A single possible jump: from a hole, over a middle peg, to a destination. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCPegJump
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PegGame")
	int32 Over = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PegGame")
	int32 Destination = -1;
};

/** Record of a single move (for undo support). */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCPegMove
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PegGame")
	int32 From = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PegGame")
	int32 Over = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PegGame")
	int32 To = -1;
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPegSelected, int32, Position);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPegJumped, int32, From, int32, Over, int32, To);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPegGameOver, int32, PegsRemaining);

// ---------------------------------------------------------------------------
// APegGameMode
// ---------------------------------------------------------------------------

/**
 * Triangle peg solitaire: 15 holes in a triangle, start with 14 pegs
 * (one hole empty). Jump pegs to remove them. Goal: 1 peg remaining.
 *
 * Board layout (indices):
 *         0
 *        1 2
 *       3 4 5
 *      6 7 8 9
 *    10 11 12 13 14
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APegGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	APegGameMode();

	// -- Lifecycle --------------------------------------------------------

	virtual void StartGame() override;

	/** Start the game with a specific hole initially empty. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void StartGameWithEmptyPosition(int32 EmptyPos);

	// -- Core gameplay ----------------------------------------------------

	/** Select a peg to potentially jump with. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void SelectPeg(int32 Position);

	/** Attempt to jump from the currently selected peg to a destination. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void JumpPeg(int32 From, int32 To);

	/** Check if a specific jump is valid. */
	UFUNCTION(BlueprintPure, Category = "PHC|PegGame")
	bool CanJump(int32 From, int32 To) const;

	/** Get all valid jump destinations from the given position. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	TArray<int32> GetValidJumps(int32 From) const;

	/** Check if any moves remain on the board. */
	UFUNCTION(BlueprintPure, Category = "PHC|PegGame")
	bool HasAnyValidMoves() const;

	/** End the game and score based on pegs remaining. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void CheckGameOver();

	/** Get the current peg states (true = peg present). */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	TArray<bool> GetPegPositions() const;

	/** Undo the last move. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PegGame")
	void UndoLastMove();

	// -- Accessors --------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|PegGame")
	int32 GetPegsRemaining() const { return PegsRemaining; }

	UFUNCTION(BlueprintPure, Category = "PHC|PegGame")
	int32 GetSelectedPeg() const { return SelectedPeg; }

	UFUNCTION(BlueprintPure, Category = "PHC|PegGame")
	int32 GetMoveCount() const { return MoveHistory.Num(); }

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|PegGame")
	FOnPegSelected OnPegSelected;

	UPROPERTY(BlueprintAssignable, Category = "PHC|PegGame")
	FOnPegJumped OnPegJumped;

	UPROPERTY(BlueprintAssignable, Category = "PHC|PegGame")
	FOnPegGameOver OnPegGameOver;

	// -- Constants --------------------------------------------------------

	static constexpr int32 NUM_HOLES = 15;

protected:
	// -- State ------------------------------------------------------------

	/** True at index i means a peg is present. */
	bool Pegs[NUM_HOLES];

	UPROPERTY(BlueprintReadOnly, Category = "PHC|PegGame")
	int32 PegsRemaining = 14;

	/** Currently selected peg position (-1 = none). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|PegGame")
	int32 SelectedPeg = -1;

	/** Move history stack for undo. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|PegGame")
	TArray<FPHCPegMove> MoveHistory;

	// -- Jump table -------------------------------------------------------

	/** For each hole, the list of possible (over, destination) jump pairs. */
	TArray<TArray<FPHCPegJump>> JumpTable;

	/** Build the static jump table for the triangle board. */
	void BuildJumpTable();

private:
	/** Register a bidirectional jump possibility. */
	void AddJump(int32 A, int32 Middle, int32 B);
};
