// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Connect4/Connect4GameMode.h"
#include "Games/Connect4/Connect4HUD.h"
#include "Engine/World.h"
#include "TimerManager.h"

static constexpr int32 AI_DEPTH = 6;
static constexpr int32 SCORE_WIN = 100000;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AConnect4GameMode::AConnect4GameMode()
{
	GameType = EPHCGameType::Connect4;
	GameHUDClass = UConnect4HUD::StaticClass();
	MaxGameTime = 0.f; // Turn-based.
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void AConnect4GameMode::StartGame()
{
	Super::StartGame();

	FMemory::Memzero(Board, sizeof(Board));
	bPlayerTurn = true;
	MoveCount = 0;

	OnTurnChanged.Broadcast(bPlayerTurn);
}

// ---------------------------------------------------------------------------
// Gameplay
// ---------------------------------------------------------------------------

int32 AConnect4GameMode::DropPiece(int32 Column)
{
	if (!bGameActive || Column < 0 || Column >= NUM_COLS)
	{
		return -1;
	}

	// Only allow the player to drop on their turn.
	if (bPlayerTurn)
	{
		const int32 Row = PlacePiece(Column, /*Player=*/ 1);
		if (Row < 0)
		{
			return -1; // Column full.
		}

		++MoveCount;
		OnPiecePlaced.Broadcast(Column, Row, 1);

		if (CheckWin(Column, Row, 1))
		{
			AddScore(1000);
			OnGameOver.Broadcast(1);
			EndGame(/*bWon=*/ true);
			return Row;
		}

		if (CheckDraw())
		{
			AddScore(100);
			OnGameOver.Broadcast(0);
			EndGame(/*bWon=*/ false);
			return Row;
		}

		bPlayerTurn = false;
		OnTurnChanged.Broadcast(bPlayerTurn);
		ScheduleCPUMove();

		return Row;
	}

	return -1;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool AConnect4GameMode::IsColumnFull(int32 Col) const
{
	if (Col < 0 || Col >= NUM_COLS) return true;
	return Board[Col][NUM_ROWS - 1] != 0;
}

TArray<int32> AConnect4GameMode::GetValidColumns() const
{
	TArray<int32> Valid;
	for (int32 c = 0; c < NUM_COLS; ++c)
	{
		if (!IsColumnFull(c))
		{
			Valid.Add(c);
		}
	}
	return Valid;
}

int32 AConnect4GameMode::GetCell(int32 Col, int32 Row) const
{
	if (Col < 0 || Col >= NUM_COLS || Row < 0 || Row >= NUM_ROWS)
	{
		return -1;
	}
	return Board[Col][Row];
}

// ---------------------------------------------------------------------------
// Board helpers
// ---------------------------------------------------------------------------

int32 AConnect4GameMode::PlacePiece(int32 Col, int32 Player)
{
	for (int32 r = 0; r < NUM_ROWS; ++r)
	{
		if (Board[Col][r] == 0)
		{
			Board[Col][r] = Player;
			return r;
		}
	}
	return -1; // Full.
}

void AConnect4GameMode::RemovePiece(int32 Col)
{
	for (int32 r = NUM_ROWS - 1; r >= 0; --r)
	{
		if (Board[Col][r] != 0)
		{
			Board[Col][r] = 0;
			return;
		}
	}
}

bool AConnect4GameMode::CheckWin(int32 LastCol, int32 LastRow, int32 Player) const
{
	// Direction vectors: horizontal, vertical, diag-up, diag-down.
	static const int32 DirC[] = { 1, 0, 1, 1 };
	static const int32 DirR[] = { 0, 1, 1, -1 };

	for (int32 d = 0; d < 4; ++d)
	{
		int32 Count = 1; // The piece itself.

		// Count in the positive direction.
		for (int32 step = 1; step < 4; ++step)
		{
			int32 c = LastCol + DirC[d] * step;
			int32 r = LastRow + DirR[d] * step;
			if (c < 0 || c >= NUM_COLS || r < 0 || r >= NUM_ROWS) break;
			if (Board[c][r] != Player) break;
			++Count;
		}
		// Count in the negative direction.
		for (int32 step = 1; step < 4; ++step)
		{
			int32 c = LastCol - DirC[d] * step;
			int32 r = LastRow - DirR[d] * step;
			if (c < 0 || c >= NUM_COLS || r < 0 || r >= NUM_ROWS) break;
			if (Board[c][r] != Player) break;
			++Count;
		}

		if (Count >= 4)
		{
			return true;
		}
	}
	return false;
}

bool AConnect4GameMode::CheckDraw() const
{
	for (int32 c = 0; c < NUM_COLS; ++c)
	{
		if (Board[c][NUM_ROWS - 1] == 0)
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// CPU AI
// ---------------------------------------------------------------------------

void AConnect4GameMode::ScheduleCPUMove()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CPUMoveTimerHandle,
			this,
			&AConnect4GameMode::ExecuteCPUMove,
			0.8f,
			/*bLoop=*/ false
		);
	}
}

void AConnect4GameMode::ExecuteCPUMove()
{
	if (!bGameActive || bPlayerTurn)
	{
		return;
	}

	const int32 Col = CPUMove();
	const int32 Row = PlacePiece(Col, /*Player=*/ 2);
	if (Row < 0)
	{
		return; // Should not happen; CPUMove picks valid columns.
	}

	++MoveCount;
	OnPiecePlaced.Broadcast(Col, Row, 2);

	if (CheckWin(Col, Row, 2))
	{
		OnGameOver.Broadcast(2);
		EndGame(/*bWon=*/ false);
		return;
	}

	if (CheckDraw())
	{
		AddScore(100);
		OnGameOver.Broadcast(0);
		EndGame(/*bWon=*/ false);
		return;
	}

	bPlayerTurn = true;
	OnTurnChanged.Broadcast(bPlayerTurn);
}

int32 AConnect4GameMode::CPUMove()
{
	TArray<int32> ValidCols = GetValidColumns();
	if (ValidCols.Num() == 0)
	{
		return 0;
	}

	int32 BestCol = ValidCols[0];
	int32 BestScore = TNumericLimits<int32>::Min();

	for (int32 Col : ValidCols)
	{
		int32 Row = PlacePiece(Col, 2);
		if (Row < 0) continue;

		// Instant win check.
		if (CheckWin(Col, Row, 2))
		{
			RemovePiece(Col);
			return Col;
		}

		int32 Score = Minimax(AI_DEPTH - 1, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max(), /*bMaximizing=*/ false);

		RemovePiece(Col);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCol = Col;
		}
	}

	return BestCol;
}

int32 AConnect4GameMode::Minimax(int32 Depth, int32 Alpha, int32 Beta, bool bMaximizing)
{
	// Terminal checks.
	// We cannot call CheckWin here without knowing the last move,
	// so we use the heuristic evaluator for non-zero depth, and
	// do a full evaluation at depth 0.

	if (Depth == 0 || CheckDraw())
	{
		return EvaluateBoard();
	}

	TArray<int32> ValidCols = GetValidColumns();
	if (ValidCols.Num() == 0)
	{
		return EvaluateBoard();
	}

	if (bMaximizing)
	{
		// CPU (player 2) maximizes.
		int32 MaxEval = TNumericLimits<int32>::Min();

		for (int32 Col : ValidCols)
		{
			int32 Row = PlacePiece(Col, 2);
			if (Row < 0) continue;

			if (CheckWin(Col, Row, 2))
			{
				RemovePiece(Col);
				return SCORE_WIN + Depth; // Prefer faster wins.
			}

			int32 Eval = Minimax(Depth - 1, Alpha, Beta, false);
			RemovePiece(Col);

			MaxEval = FMath::Max(MaxEval, Eval);
			Alpha = FMath::Max(Alpha, Eval);
			if (Beta <= Alpha)
			{
				break; // Beta cutoff.
			}
		}
		return MaxEval;
	}
	else
	{
		// Player (player 1) minimizes (from CPU perspective).
		int32 MinEval = TNumericLimits<int32>::Max();

		for (int32 Col : ValidCols)
		{
			int32 Row = PlacePiece(Col, 1);
			if (Row < 0) continue;

			if (CheckWin(Col, Row, 1))
			{
				RemovePiece(Col);
				return -(SCORE_WIN + Depth);
			}

			int32 Eval = Minimax(Depth - 1, Alpha, Beta, true);
			RemovePiece(Col);

			MinEval = FMath::Min(MinEval, Eval);
			Beta = FMath::Min(Beta, Eval);
			if (Beta <= Alpha)
			{
				break; // Alpha cutoff.
			}
		}
		return MinEval;
	}
}

int32 AConnect4GameMode::EvaluateBoard() const
{
	int32 Score = 0;

	// Centre column preference.
	const int32 CenterCol = NUM_COLS / 2;
	int32 CenterCount = 0;
	for (int32 r = 0; r < NUM_ROWS; ++r)
	{
		if (Board[CenterCol][r] == 2) ++CenterCount;
	}
	Score += CenterCount * 3;

	// Score all windows of 4.
	// Horizontal.
	for (int32 r = 0; r < NUM_ROWS; ++r)
	{
		for (int32 c = 0; c <= NUM_COLS - 4; ++c)
		{
			int32 CPUCount = 0, PlayerCount = 0, EmptyCount = 0;
			for (int32 i = 0; i < 4; ++i)
			{
				int32 Cell = Board[c + i][r];
				if (Cell == 2) ++CPUCount;
				else if (Cell == 1) ++PlayerCount;
				else ++EmptyCount;
			}
			Score += ScoreWindow(CPUCount, PlayerCount, EmptyCount);
		}
	}

	// Vertical.
	for (int32 c = 0; c < NUM_COLS; ++c)
	{
		for (int32 r = 0; r <= NUM_ROWS - 4; ++r)
		{
			int32 CPUCount = 0, PlayerCount = 0, EmptyCount = 0;
			for (int32 i = 0; i < 4; ++i)
			{
				int32 Cell = Board[c][r + i];
				if (Cell == 2) ++CPUCount;
				else if (Cell == 1) ++PlayerCount;
				else ++EmptyCount;
			}
			Score += ScoreWindow(CPUCount, PlayerCount, EmptyCount);
		}
	}

	// Diagonal (positive slope: bottom-left to top-right).
	for (int32 c = 0; c <= NUM_COLS - 4; ++c)
	{
		for (int32 r = 0; r <= NUM_ROWS - 4; ++r)
		{
			int32 CPUCount = 0, PlayerCount = 0, EmptyCount = 0;
			for (int32 i = 0; i < 4; ++i)
			{
				int32 Cell = Board[c + i][r + i];
				if (Cell == 2) ++CPUCount;
				else if (Cell == 1) ++PlayerCount;
				else ++EmptyCount;
			}
			Score += ScoreWindow(CPUCount, PlayerCount, EmptyCount);
		}
	}

	// Diagonal (negative slope: top-left to bottom-right).
	for (int32 c = 0; c <= NUM_COLS - 4; ++c)
	{
		for (int32 r = 3; r < NUM_ROWS; ++r)
		{
			int32 CPUCount = 0, PlayerCount = 0, EmptyCount = 0;
			for (int32 i = 0; i < 4; ++i)
			{
				int32 Cell = Board[c + i][r - i];
				if (Cell == 2) ++CPUCount;
				else if (Cell == 1) ++PlayerCount;
				else ++EmptyCount;
			}
			Score += ScoreWindow(CPUCount, PlayerCount, EmptyCount);
		}
	}

	return Score;
}

int32 AConnect4GameMode::ScoreWindow(int32 CPUCount, int32 OpponentCount, int32 EmptyCount) const
{
	// Only score windows that belong to one side (mixed = 0).
	if (CPUCount > 0 && OpponentCount > 0)
	{
		return 0;
	}

	if (CPUCount == 4) return 1000;
	if (CPUCount == 3 && EmptyCount == 1) return 50;
	if (CPUCount == 2 && EmptyCount == 2) return 10;

	if (OpponentCount == 4) return -1000;
	if (OpponentCount == 3 && EmptyCount == 1) return -80; // Slightly more defensive.
	if (OpponentCount == 2 && EmptyCount == 2) return -10;

	return 0;
}
