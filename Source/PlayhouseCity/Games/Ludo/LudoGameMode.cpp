// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Ludo/LudoGameMode.h"
#include "Games/Ludo/LudoHUD.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

int32 ALudoGameMode::GetStartOffset(EPHCLudoColor Color)
{
	switch (Color)
	{
	case EPHCLudoColor::Red:    return 0;
	case EPHCLudoColor::Blue:   return 13;
	case EPHCLudoColor::Green:  return 26;
	case EPHCLudoColor::Yellow: return 39;
	default:                    return 0;
	}
}

const TArray<int32>& ALudoGameMode::GetSafeZones()
{
	// Star/safe squares at every color's start and 8 positions after start.
	static TArray<int32> Zones = { 0, 8, 13, 21, 26, 34, 39, 47 };
	return Zones;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ALudoGameMode::ALudoGameMode()
{
	GameType = EPHCGameType::Ludo;
	GameHUDClass = ULudoHUD::StaticClass();
	MaxGameTime = 0.f; // Turn-based, no global timer.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ALudoGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void ALudoGameMode::StartGame()
{
	Super::StartGame();

	Players.Empty();
	FinishOrder.Empty();
	CurrentPlayerIndex = 0;
	DiceValue = 0;
	bDiceRolled = false;
	ConsecutiveSixes = 0;

	// Initialize 4 players: Red (human), Blue, Green, Yellow (CPU).
	const EPHCLudoColor Colors[] = {
		EPHCLudoColor::Red, EPHCLudoColor::Blue,
		EPHCLudoColor::Green, EPHCLudoColor::Yellow
	};

	for (int32 P = 0; P < 4; ++P)
	{
		FPHCLudoPlayer Player;
		Player.Color = Colors[P];
		Player.bIsHuman = (P == 0);
		Player.bHasFinished = false;
		Player.Pieces.SetNum(4);

		for (int32 i = 0; i < 4; ++i)
		{
			Player.Pieces[i].Position = -1;
			Player.Pieces[i].bAtHome = true;
			Player.Pieces[i].bFinished = false;
			Player.Pieces[i].Color = Colors[P];
		}

		Players.Add(Player);
	}

	OnTurnChanged.Broadcast(CurrentPlayerIndex);
}

// ---------------------------------------------------------------------------
// Core gameplay
// ---------------------------------------------------------------------------

int32 ALudoGameMode::RollDice()
{
	if (!bGameActive || bDiceRolled)
	{
		return 0;
	}

	DiceValue = FMath::RandRange(1, 6);
	bDiceRolled = true;

	if (DiceValue == 6)
	{
		ConsecutiveSixes++;
		// Three consecutive 6s: lose turn.
		if (ConsecutiveSixes >= 3)
		{
			ConsecutiveSixes = 0;
			bDiceRolled = false;
			OnDiceRolled.Broadcast(DiceValue);
			NextTurn();
			return DiceValue;
		}
	}
	else
	{
		ConsecutiveSixes = 0;
	}

	OnDiceRolled.Broadcast(DiceValue);

	// If no valid moves, auto-advance turn.
	TArray<int32> ValidMoves = GetValidMoves();
	if (ValidMoves.Num() == 0)
	{
		bDiceRolled = false;
		NextTurn();
	}

	return DiceValue;
}

void ALudoGameMode::MovePiece(int32 PieceIndex)
{
	if (!bGameActive || !bDiceRolled)
	{
		return;
	}
	if (!Players.IsValidIndex(CurrentPlayerIndex))
	{
		return;
	}

	FPHCLudoPlayer& CurrentPlayer = Players[CurrentPlayerIndex];
	if (!CurrentPlayer.Pieces.IsValidIndex(PieceIndex))
	{
		return;
	}
	if (!CanMovePiece(PieceIndex))
	{
		return;
	}

	FPHCLudoPiece& Piece = CurrentPlayer.Pieces[PieceIndex];

	if (Piece.bAtHome && DiceValue == 6)
	{
		// Move out of home to the start square.
		Piece.bAtHome = false;
		Piece.Position = 0; // Relative position 0 = color's start.

		int32 AbsPos = GetAbsolutePosition(0, CurrentPlayer.Color);
		CheckCapture(AbsPos, CurrentPlayer.Color);
	}
	else if (!Piece.bAtHome && !Piece.bFinished)
	{
		int32 NewRelPos = ComputeNewPosition(Piece, DiceValue);

		if (NewRelPos == TRACK_LENGTH + HOME_COLUMN_LENGTH)
		{
			// Exact landing on finish.
			Piece.Position = NewRelPos;
			Piece.bFinished = true;
		}
		else if (NewRelPos < TRACK_LENGTH)
		{
			// Still on the shared track.
			Piece.Position = NewRelPos;
			int32 AbsPos = GetAbsolutePosition(NewRelPos, CurrentPlayer.Color);
			CheckCapture(AbsPos, CurrentPlayer.Color);
		}
		else
		{
			// In the home column.
			Piece.Position = NewRelPos;
		}
	}

	OnPieceMoved.Broadcast(CurrentPlayerIndex, PieceIndex);

	// Check if this player has finished all pieces.
	if (CheckFinished(CurrentPlayerIndex))
	{
		CurrentPlayer.bHasFinished = true;
		if (!FinishOrder.Contains(CurrentPlayerIndex))
		{
			FinishOrder.Add(CurrentPlayerIndex);
		}

		// If human finished, calculate score.
		if (CurrentPlayerIndex == 0)
		{
			int32 PlaceBonus = 0;
			if (FinishOrder.Num() == 1) PlaceBonus = 500;      // 1st
			else if (FinishOrder.Num() == 2) PlaceBonus = 300;  // 2nd
			else if (FinishOrder.Num() == 3) PlaceBonus = 100;  // 3rd

			AddScore(PlaceBonus);
			EndGame(FinishOrder[0] == 0); // Won if finished first.
			return;
		}

		// If 3 players finished, end for the last remaining.
		if (FinishOrder.Num() >= 3)
		{
			// Find the remaining player.
			for (int32 i = 0; i < 4; ++i)
			{
				if (!FinishOrder.Contains(i))
				{
					FinishOrder.Add(i);
					break;
				}
			}
			// Human did not finish first if we got here.
			bool bHumanWon = (FinishOrder.Num() > 0 && FinishOrder[0] == 0);
			EndGame(bHumanWon);
			return;
		}
	}

	bDiceRolled = false;

	// Extra turn on 6 (unless 3 consecutive).
	if (DiceValue == 6 && ConsecutiveSixes < 3)
	{
		// Same player goes again, do not call NextTurn().
		// For CPU, trigger another turn after delay.
		if (!CurrentPlayer.bIsHuman)
		{
			GetWorldTimerManager().SetTimer(
				CPUTimerHandle, this, &ALudoGameMode::CPUTurn, 0.8f, false);
		}
	}
	else
	{
		NextTurn();
	}
}

bool ALudoGameMode::CanMovePiece(int32 PieceIndex) const
{
	if (!Players.IsValidIndex(CurrentPlayerIndex))
	{
		return false;
	}

	const FPHCLudoPlayer& CurrentPlayer = Players[CurrentPlayerIndex];
	if (!CurrentPlayer.Pieces.IsValidIndex(PieceIndex))
	{
		return false;
	}

	const FPHCLudoPiece& Piece = CurrentPlayer.Pieces[PieceIndex];

	if (Piece.bFinished)
	{
		return false;
	}

	// Piece is at home: need a 6 to leave.
	if (Piece.bAtHome)
	{
		return DiceValue == 6;
	}

	// Piece is on the board: check if the move would overshoot the finish.
	int32 NewPos = ComputeNewPosition(Piece, DiceValue);
	if (NewPos > TRACK_LENGTH + HOME_COLUMN_LENGTH)
	{
		return false; // Overshoot, not allowed.
	}

	return true;
}

TArray<int32> ALudoGameMode::GetValidMoves() const
{
	TArray<int32> Result;
	if (!Players.IsValidIndex(CurrentPlayerIndex))
	{
		return Result;
	}

	for (int32 i = 0; i < Players[CurrentPlayerIndex].Pieces.Num(); ++i)
	{
		if (CanMovePiece(i))
		{
			Result.Add(i);
		}
	}

	return Result;
}

void ALudoGameMode::CheckCapture(int32 AbsolutePosition, EPHCLudoColor MovingColor)
{
	if (IsSafeZone(AbsolutePosition))
	{
		return; // Cannot capture on safe zones.
	}

	for (int32 P = 0; P < Players.Num(); ++P)
	{
		if (Players[P].Color == MovingColor)
		{
			continue;
		}

		for (int32 i = 0; i < Players[P].Pieces.Num(); ++i)
		{
			FPHCLudoPiece& OtherPiece = Players[P].Pieces[i];
			if (OtherPiece.bAtHome || OtherPiece.bFinished)
			{
				continue;
			}

			// Check if the other piece is in the home column (can't be captured there).
			if (OtherPiece.Position >= TRACK_LENGTH)
			{
				continue;
			}

			int32 OtherAbsPos = GetAbsolutePosition(OtherPiece.Position, Players[P].Color);
			if (OtherAbsPos == AbsolutePosition)
			{
				// Capture: send back home.
				OtherPiece.Position = -1;
				OtherPiece.bAtHome = true;

				// Bonus score for capturing.
				if (CurrentPlayerIndex == 0)
				{
					AddScore(25);
				}

				OnPieceCaptured.Broadcast(P, i);
			}
		}
	}
}

bool ALudoGameMode::CheckFinished(int32 PlayerIndex) const
{
	if (!Players.IsValidIndex(PlayerIndex))
	{
		return false;
	}

	for (const FPHCLudoPiece& Piece : Players[PlayerIndex].Pieces)
	{
		if (!Piece.bFinished)
		{
			return false;
		}
	}

	return true;
}

void ALudoGameMode::CPUTurn()
{
	if (!bGameActive)
	{
		return;
	}
	if (!Players.IsValidIndex(CurrentPlayerIndex))
	{
		return;
	}
	if (Players[CurrentPlayerIndex].bIsHuman || Players[CurrentPlayerIndex].bHasFinished)
	{
		NextTurn();
		return;
	}

	// Roll the dice.
	RollDice();

	if (!bDiceRolled)
	{
		// No valid moves, turn was already advanced.
		return;
	}

	TArray<int32> ValidMoves = GetValidMoves();
	if (ValidMoves.Num() == 0)
	{
		bDiceRolled = false;
		NextTurn();
		return;
	}

	// CPU strategy: prioritize captures, then leaving home, then advancing furthest piece.
	int32 BestMove = ValidMoves[0];
	int32 BestScore = -1;

	for (int32 MoveIdx : ValidMoves)
	{
		const FPHCLudoPiece& Piece = Players[CurrentPlayerIndex].Pieces[MoveIdx];
		int32 MoveScore = 0;

		if (Piece.bAtHome && DiceValue == 6)
		{
			MoveScore = 50; // Good to leave home.
		}
		else if (!Piece.bAtHome)
		{
			int32 NewPos = ComputeNewPosition(Piece, DiceValue);

			// Check for potential capture.
			if (NewPos < TRACK_LENGTH)
			{
				int32 AbsPos = GetAbsolutePosition(NewPos, Players[CurrentPlayerIndex].Color);
				if (!IsSafeZone(AbsPos))
				{
					for (int32 P = 0; P < Players.Num(); ++P)
					{
						if (Players[P].Color == Players[CurrentPlayerIndex].Color) continue;
						for (const FPHCLudoPiece& Other : Players[P].Pieces)
						{
							if (!Other.bAtHome && !Other.bFinished && Other.Position < TRACK_LENGTH)
							{
								int32 OtherAbs = GetAbsolutePosition(Other.Position, Players[P].Color);
								if (OtherAbs == AbsPos)
								{
									MoveScore = 100; // Capture is best.
								}
							}
						}
					}
				}
			}

			// Reaching finish is also very good.
			if (NewPos == TRACK_LENGTH + HOME_COLUMN_LENGTH)
			{
				MoveScore = FMath::Max(MoveScore, 90);
			}

			// Entering home column is desirable.
			if (NewPos >= TRACK_LENGTH && MoveScore < 90)
			{
				MoveScore = FMath::Max(MoveScore, 60);
			}

			// General advancement score.
			if (MoveScore == 0)
			{
				MoveScore = NewPos; // Prefer advancing further pieces.
			}
		}

		if (MoveScore > BestScore)
		{
			BestScore = MoveScore;
			BestMove = MoveIdx;
		}
	}

	MovePiece(BestMove);
}

void ALudoGameMode::NextTurn()
{
	if (!bGameActive)
	{
		return;
	}

	// Advance to next non-finished player.
	int32 Attempts = 0;
	do
	{
		CurrentPlayerIndex = (CurrentPlayerIndex + 1) % 4;
		Attempts++;
	}
	while (Players.IsValidIndex(CurrentPlayerIndex) &&
		   Players[CurrentPlayerIndex].bHasFinished &&
		   Attempts < 4);

	ConsecutiveSixes = 0;
	bDiceRolled = false;

	OnTurnChanged.Broadcast(CurrentPlayerIndex);

	// If it's a CPU player, auto-play after a short delay.
	if (Players.IsValidIndex(CurrentPlayerIndex) && !Players[CurrentPlayerIndex].bIsHuman)
	{
		GetWorldTimerManager().SetTimer(
			CPUTimerHandle, this, &ALudoGameMode::CPUTurn, 1.0f, false);
	}
}

bool ALudoGameMode::IsSafeZone(int32 AbsolutePosition) const
{
	const TArray<int32>& Zones = GetSafeZones();
	return Zones.Contains(AbsolutePosition);
}

int32 ALudoGameMode::GetAbsolutePosition(int32 RelativePos, EPHCLudoColor Color) const
{
	if (RelativePos < 0 || RelativePos >= TRACK_LENGTH)
	{
		// In home column or at home, no shared-track absolute position.
		return -1;
	}

	int32 Offset = GetStartOffset(Color);
	return (RelativePos + Offset) % TRACK_LENGTH;
}

TArray<FPHCLudoPiece> ALudoGameMode::GetAllPieces() const
{
	TArray<FPHCLudoPiece> AllPieces;
	for (const FPHCLudoPlayer& Player : Players)
	{
		AllPieces.Append(Player.Pieces);
	}
	return AllPieces;
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

int32 ALudoGameMode::ComputeNewPosition(const FPHCLudoPiece& Piece, int32 DiceRoll) const
{
	if (Piece.bAtHome || Piece.bFinished)
	{
		return Piece.Position;
	}

	int32 CurrentPos = Piece.Position;
	int32 NewPos = CurrentPos + DiceRoll;

	// If still on the shared track and about to enter / already in home column,
	// the position wraps into the home column range (52-57).
	// Positions 0-51 are on the shared track; 52-57 are the home column;
	// 58 = exact finish (TRACK_LENGTH + HOME_COLUMN_LENGTH).

	// Already in the home column.
	if (CurrentPos >= TRACK_LENGTH)
	{
		return NewPos;
	}

	// On the shared track: check if we pass the home entry.
	if (NewPos > HOME_ENTRY_RELATIVE)
	{
		// Entering the home column.
		int32 StepsIntoHome = NewPos - HOME_ENTRY_RELATIVE - 1;
		NewPos = TRACK_LENGTH + StepsIntoHome;
	}

	return NewPos;
}
