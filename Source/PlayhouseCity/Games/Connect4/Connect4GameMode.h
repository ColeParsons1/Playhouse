// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "Connect4GameMode.generated.h"

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnConnect4PiecePlaced, int32, Column, int32, Row, int32, Player);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnect4TurnChanged, bool, bIsPlayerTurn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnect4GameOver, int32, Winner);

// ---------------------------------------------------------------------------
// AConnect4GameMode
// ---------------------------------------------------------------------------

/**
 * Connect 4 mini-game mode.
 *
 * Standard 7-column x 6-row vertical grid. Player = 1, CPU = 2.
 * Player drops pieces by column; CPU uses minimax with alpha-beta
 * pruning (depth 6) for competitive AI.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AConnect4GameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AConnect4GameMode();

	// -- Game flow --------------------------------------------------------

	virtual void StartGame() override;

	// -- Gameplay ---------------------------------------------------------

	/**
	 * Drop a piece into the given column.
	 * @return  The row the piece landed in, or -1 if the column is full.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Connect4")
	int32 DropPiece(int32 Column);

	// -- Queries ----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|Connect4")
	bool IsPlayerTurn() const { return bPlayerTurn; }

	UFUNCTION(BlueprintPure, Category = "PHC|Connect4")
	bool IsColumnFull(int32 Col) const;

	UFUNCTION(BlueprintPure, Category = "PHC|Connect4")
	TArray<int32> GetValidColumns() const;

	UFUNCTION(BlueprintPure, Category = "PHC|Connect4")
	int32 GetCell(int32 Col, int32 Row) const;

	UFUNCTION(BlueprintPure, Category = "PHC|Connect4")
	int32 GetMoveCount() const { return MoveCount; }

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Connect4")
	FOnConnect4PiecePlaced OnPiecePlaced;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Connect4")
	FOnConnect4TurnChanged OnTurnChanged;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Connect4")
	FOnConnect4GameOver OnGameOver;

	// -- Constants --------------------------------------------------------

	static constexpr int32 NUM_COLS = 7;
	static constexpr int32 NUM_ROWS = 6;

protected:
	// -- Board state ------------------------------------------------------

	/** Board[Col][Row]: 0=empty, 1=player, 2=CPU. Row 0 is the bottom. */
	int32 Board[NUM_COLS][NUM_ROWS];

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Connect4")
	bool bPlayerTurn = true;

	int32 MoveCount = 0;

	FTimerHandle CPUMoveTimerHandle;

	// -- AI ---------------------------------------------------------------

	/** Entry point: choose the best column for the CPU. */
	int32 CPUMove();

	/**
	 * Minimax with alpha-beta pruning.
	 * @param Depth         Remaining search depth.
	 * @param Alpha         Best already-explored option along the path to root for the maximizer.
	 * @param Beta          Best already-explored option along the path to root for the minimizer.
	 * @param bMaximizing   True when evaluating for CPU (player 2).
	 * @return              Heuristic board score.
	 */
	int32 Minimax(int32 Depth, int32 Alpha, int32 Beta, bool bMaximizing);

	/** Heuristic evaluation of the current board from the CPU's perspective. */
	int32 EvaluateBoard() const;

	// -- Board helpers ----------------------------------------------------

	/** Check if the last placed piece creates a four-in-a-row. */
	bool CheckWin(int32 LastCol, int32 LastRow, int32 Player) const;

	/** Check if the board is completely full. */
	bool CheckDraw() const;

	/** Place a piece on the board without game-flow side effects (for AI search). */
	int32 PlacePiece(int32 Col, int32 Player);

	/** Remove the top piece from a column (undo for AI search). */
	void RemovePiece(int32 Col);

	/** Schedule the CPU's move after a short delay. */
	void ScheduleCPUMove();

	/** Execute the CPU's move (timer callback). */
	void ExecuteCPUMove();

	/** Score a window of 4 cells for the heuristic evaluator. */
	int32 ScoreWindow(int32 PlayerCount, int32 OpponentCount, int32 EmptyCount) const;
};
