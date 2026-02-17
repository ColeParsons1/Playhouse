// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/PegGame/PegGameMode.h"
#include "Games/PegGame/PegGameHUD.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

APegGameMode::APegGameMode()
{
	GameType = EPHCGameType::PegGame;
	GameHUDClass = UPegGameHUD::StaticClass();
	MaxGameTime = 0.f; // No time limit for a puzzle game.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void APegGameMode::StartGame()
{
	Super::StartGame();

	BuildJumpTable();
	StartGameWithEmptyPosition(0); // Default: top peg removed.
}

void APegGameMode::StartGameWithEmptyPosition(int32 EmptyPos)
{
	// Fill all pegs.
	for (int32 i = 0; i < NUM_HOLES; ++i)
	{
		Pegs[i] = true;
	}

	// Remove the chosen starting peg.
	if (EmptyPos >= 0 && EmptyPos < NUM_HOLES)
	{
		Pegs[EmptyPos] = false;
	}
	else
	{
		Pegs[0] = false;
	}

	PegsRemaining = 14;
	SelectedPeg = -1;
	MoveHistory.Empty();
}

// ---------------------------------------------------------------------------
// Core gameplay
// ---------------------------------------------------------------------------

void APegGameMode::SelectPeg(int32 Position)
{
	if (!bGameActive)
	{
		return;
	}

	if (Position < 0 || Position >= NUM_HOLES)
	{
		SelectedPeg = -1;
		OnPegSelected.Broadcast(-1);
		return;
	}

	if (!Pegs[Position])
	{
		// Clicked an empty hole. If we have a selected peg, try to jump there.
		if (SelectedPeg >= 0 && CanJump(SelectedPeg, Position))
		{
			JumpPeg(SelectedPeg, Position);
			return;
		}

		SelectedPeg = -1;
		OnPegSelected.Broadcast(-1);
		return;
	}

	// Check if this peg has any valid jumps.
	TArray<int32> Jumps = GetValidJumps(Position);
	if (Jumps.Num() > 0)
	{
		SelectedPeg = Position;
		OnPegSelected.Broadcast(SelectedPeg);
	}
	else
	{
		// No valid jumps from this peg.
		SelectedPeg = -1;
		OnPegSelected.Broadcast(-1);
	}
}

void APegGameMode::JumpPeg(int32 From, int32 To)
{
	if (!bGameActive)
	{
		return;
	}

	if (!CanJump(From, To))
	{
		return;
	}

	// Find the middle peg.
	int32 MiddlePeg = -1;
	if (JumpTable.IsValidIndex(From))
	{
		for (const FPHCPegJump& Jump : JumpTable[From])
		{
			if (Jump.Destination == To)
			{
				MiddlePeg = Jump.Over;
				break;
			}
		}
	}

	if (MiddlePeg < 0)
	{
		return;
	}

	// Execute the jump.
	Pegs[From] = false;
	Pegs[MiddlePeg] = false;
	Pegs[To] = true;
	PegsRemaining--;

	// Record for undo.
	FPHCPegMove Move;
	Move.From = From;
	Move.Over = MiddlePeg;
	Move.To = To;
	MoveHistory.Add(Move);

	SelectedPeg = -1;

	// Score: each successful jump earns points.
	AddScore(10);

	OnPegJumped.Broadcast(From, MiddlePeg, To);

	// Check if the game is over.
	if (!HasAnyValidMoves())
	{
		CheckGameOver();
	}
}

bool APegGameMode::CanJump(int32 From, int32 To) const
{
	if (From < 0 || From >= NUM_HOLES || To < 0 || To >= NUM_HOLES)
	{
		return false;
	}

	// Source must have a peg, destination must be empty.
	if (!Pegs[From] || Pegs[To])
	{
		return false;
	}

	// Check the jump table for a valid path.
	if (!JumpTable.IsValidIndex(From))
	{
		return false;
	}

	for (const FPHCPegJump& Jump : JumpTable[From])
	{
		if (Jump.Destination == To && Pegs[Jump.Over])
		{
			return true;
		}
	}

	return false;
}

TArray<int32> APegGameMode::GetValidJumps(int32 From) const
{
	TArray<int32> Result;

	if (From < 0 || From >= NUM_HOLES || !Pegs[From])
	{
		return Result;
	}

	if (!JumpTable.IsValidIndex(From))
	{
		return Result;
	}

	for (const FPHCPegJump& Jump : JumpTable[From])
	{
		if (Pegs[Jump.Over] && !Pegs[Jump.Destination])
		{
			Result.Add(Jump.Destination);
		}
	}

	return Result;
}

bool APegGameMode::HasAnyValidMoves() const
{
	for (int32 i = 0; i < NUM_HOLES; ++i)
	{
		if (Pegs[i])
		{
			TArray<int32> Jumps = GetValidJumps(i);
			if (Jumps.Num() > 0)
			{
				return true;
			}
		}
	}

	return false;
}

void APegGameMode::CheckGameOver()
{
	if (!bGameActive)
	{
		return;
	}

	// Score bonuses based on pegs remaining.
	// 1 peg = perfect game (500 bonus).
	// 2 pegs = great (200 bonus).
	// 3 pegs = good (100 bonus).
	// 4+ = no bonus.

	if (PegsRemaining == 1)
	{
		AddScore(500);
	}
	else if (PegsRemaining == 2)
	{
		AddScore(200);
	}
	else if (PegsRemaining == 3)
	{
		AddScore(100);
	}

	OnPegGameOver.Broadcast(PegsRemaining);
	EndGame(PegsRemaining == 1);
}

TArray<bool> APegGameMode::GetPegPositions() const
{
	TArray<bool> Result;
	Result.SetNum(NUM_HOLES);
	for (int32 i = 0; i < NUM_HOLES; ++i)
	{
		Result[i] = Pegs[i];
	}
	return Result;
}

void APegGameMode::UndoLastMove()
{
	if (!bGameActive || MoveHistory.Num() == 0)
	{
		return;
	}

	FPHCPegMove LastMove = MoveHistory.Pop();

	// Reverse the jump.
	Pegs[LastMove.From] = true;
	Pegs[LastMove.Over] = true;
	Pegs[LastMove.To] = false;
	PegsRemaining++;

	// Remove the points earned from that jump.
	AddScore(-10);

	SelectedPeg = -1;
	OnPegSelected.Broadcast(-1);
}

// ---------------------------------------------------------------------------
// Jump table construction
// ---------------------------------------------------------------------------

void APegGameMode::BuildJumpTable()
{
	JumpTable.Empty();
	JumpTable.SetNum(NUM_HOLES);

	/*
	 * Triangle board layout:
	 *         0
	 *        1  2
	 *       3  4  5
	 *      6  7  8  9
	 *    10 11 12 13 14
	 *
	 * Row 0: {0}
	 * Row 1: {1, 2}
	 * Row 2: {3, 4, 5}
	 * Row 3: {6, 7, 8, 9}
	 * Row 4: {10, 11, 12, 13, 14}
	 */

	// Horizontal jumps within rows.
	// Row 2.
	AddJump(3, 4, 5);
	// Row 3.
	AddJump(6, 7, 8);
	AddJump(7, 8, 9);
	// Row 4.
	AddJump(10, 11, 12);
	AddJump(11, 12, 13);
	AddJump(12, 13, 14);

	// Diagonal jumps (left-leaning: /).
	AddJump(0, 1, 3);
	AddJump(1, 3, 6);
	AddJump(3, 6, 10);
	AddJump(2, 4, 7);
	AddJump(4, 7, 11);
	AddJump(5, 8, 12);

	// Diagonal jumps (right-leaning: \).
	AddJump(0, 2, 5);
	AddJump(2, 5, 9);
	AddJump(5, 9, 14);
	AddJump(1, 4, 8);
	AddJump(4, 8, 13);
	AddJump(3, 7, 12);
}

void APegGameMode::AddJump(int32 A, int32 Middle, int32 B)
{
	// Add both directions.
	if (JumpTable.IsValidIndex(A) && JumpTable.IsValidIndex(B))
	{
		FPHCPegJump JumpAtoB;
		JumpAtoB.Over = Middle;
		JumpAtoB.Destination = B;
		JumpTable[A].Add(JumpAtoB);

		FPHCPegJump JumpBtoA;
		JumpBtoA.Over = Middle;
		JumpBtoA.Destination = A;
		JumpTable[B].Add(JumpBtoA);
	}
}
