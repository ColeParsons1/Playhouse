// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Bingo/BingoGameMode.h"
#include "Games/Bingo/BingoHUD.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr int32 kBingoGM_Grid     = 5;
static constexpr int32 kBingoGM_MaxNum  = 75;
static constexpr int32 kBingoGM_CpuCount = 19;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ABingoGameMode::ABingoGameMode()
{
	GameType = EPHCGameType::Bingo;
	GameHUDClass = UBingoHUD::StaticClass();
	MaxGameTime = 0.f; // No hard time limit -- game ends on bingo.
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void ABingoGameMode::StartGame()
{
	Super::StartGame();

	// Build the number pool (1 - 75) and shuffle.
	RemainingNumbers.Reset(kBingoGM_MaxNum);
	for (int32 i = 1; i <= kBingoGM_MaxNum; ++i)
	{
		RemainingNumbers.Add(i);
	}
	for (int32 i = RemainingNumbers.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		RemainingNumbers.Swap(i, j);
	}

	CalledNumbers.Reset();

	// Generate player card.
	PlayerCard = GenerateCard();
	PlayerMarked.SetNum(kBingoGM_Grid);
	for (int32 r = 0; r < kBingoGM_Grid; ++r)
	{
		PlayerMarked[r].Init(false, kBingoGM_Grid);
	}
	// Centre cell is free.
	PlayerMarked[2][2] = true;

	// Generate CPU cards.
	CPUCards.SetNum(kBingoGM_CpuCount);
	CPUMarked.SetNum(kBingoGM_CpuCount);
	for (int32 c = 0; c < kBingoGM_CpuCount; ++c)
	{
		CPUCards[c] = GenerateCard();
		CPUMarked[c].SetNum(kBingoGM_Grid);
		for (int32 r = 0; r < kBingoGM_Grid; ++r)
		{
			CPUMarked[c][r].Init(false, kBingoGM_Grid);
		}
		CPUMarked[c][2][2] = true; // free space
	}
	ActiveCPUCount = kBingoGM_CpuCount;

	// Start the call timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CallTimerHandle,
			this,
			&ABingoGameMode::CallNextNumber,
			CallInterval,
			/*bLoop=*/ true
		);
	}
}

void ABingoGameMode::EndGame(bool bWon)
{
	// Stop calling numbers.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CallTimerHandle);
	}

	Super::EndGame(bWon);
}

// ---------------------------------------------------------------------------
// Gameplay
// ---------------------------------------------------------------------------

bool ABingoGameMode::MarkCell(int32 Row, int32 Col)
{
	if (!bGameActive || Row < 0 || Row >= kBingoGM_Grid || Col < 0 || Col >= kBingoGM_Grid)
	{
		return false;
	}

	// Already marked?
	if (PlayerMarked[Row][Col])
	{
		return false;
	}

	// The number at this cell must have been called.
	const int32 Number = PlayerCard[Row][Col];
	if (!CalledNumbers.Contains(Number))
	{
		return false;
	}

	PlayerMarked[Row][Col] = true;

	// Award a small score for each correct mark.
	AddScore(10);
	return true;
}

void ABingoGameMode::OnBingoCalled()
{
	if (!bGameActive)
	{
		return;
	}

	if (CheckBingo(PlayerMarked))
	{
		// Big score bonus for the bingo win.
		AddScore(500);
		OnBingoAchieved.Broadcast(/*bIsPlayer=*/ true, /*WinnerIndex=*/ 0);
		EndGame(/*bWon=*/ true);
	}
	else
	{
		// Penalty for false bingo call.
		AddScore(-50);
	}
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

int32 ABingoGameMode::GetLastCalledNumber() const
{
	return CalledNumbers.Num() > 0 ? CalledNumbers.Last() : 0;
}

int32 ABingoGameMode::GetCPUOpponentsRemaining() const
{
	return ActiveCPUCount;
}

TArray<int32> ABingoGameMode::GetPlayerCardFlat() const
{
	TArray<int32> Flat;
	Flat.Reserve(kBingoGM_Grid * kBingoGM_Grid);
	for (int32 r = 0; r < kBingoGM_Grid; ++r)
	{
		for (int32 c = 0; c < kBingoGM_Grid; ++c)
		{
			Flat.Add(PlayerCard.IsValidIndex(r) && PlayerCard[r].IsValidIndex(c)
				? PlayerCard[r][c]
				: 0);
		}
	}
	return Flat;
}

TArray<bool> ABingoGameMode::GetPlayerMarkedFlat() const
{
	TArray<bool> Flat;
	Flat.Reserve(kBingoGM_Grid * kBingoGM_Grid);
	for (int32 r = 0; r < kBingoGM_Grid; ++r)
	{
		for (int32 c = 0; c < kBingoGM_Grid; ++c)
		{
			Flat.Add(PlayerMarked.IsValidIndex(r) && PlayerMarked[r].IsValidIndex(c)
				? PlayerMarked[r][c]
				: false);
		}
	}
	return Flat;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void ABingoGameMode::CallNextNumber()
{
	if (!bGameActive || RemainingNumbers.Num() == 0)
	{
		// No more numbers -- end as a draw.
		EndGame(/*bWon=*/ false);
		return;
	}

	const int32 Number = RemainingNumbers.Pop(EAllowShrinking::No);
	CalledNumbers.Add(Number);

	// Broadcast to HUD / widgets.
	OnNumberCalled.Broadcast(Number);

	// Check all CPU cards.
	CheckAllCPUCards(Number);
}

void ABingoGameMode::CheckAllCPUCards(int32 CalledNumber)
{
	for (int32 c = 0; c < kBingoGM_CpuCount; ++c)
	{
		int32 Row, Col;
		if (FindNumberOnCard(CPUCards[c], CalledNumber, Row, Col))
		{
			CPUMarked[c][Row][Col] = true;
		}

		// Check if this CPU just got bingo.
		if (CheckBingo(CPUMarked[c]))
		{
			--ActiveCPUCount;
			OnBingoAchieved.Broadcast(/*bIsPlayer=*/ false, /*WinnerIndex=*/ c + 1);

			// CPU wins -- player loses.
			EndGame(/*bWon=*/ false);
			return;
		}
	}
}

TArray<TArray<int32>> ABingoGameMode::GenerateCard()
{
	// Column ranges: B(1-15), I(16-30), N(31-45), G(46-60), O(61-75).
	static const int32 ColMin[kBingoGM_Grid] = { 1, 16, 31, 46, 61 };
	static const int32 ColMax[kBingoGM_Grid] = { 15, 30, 45, 60, 75 };

	TArray<TArray<int32>> Card;
	Card.SetNum(kBingoGM_Grid);

	for (int32 col = 0; col < kBingoGM_Grid; ++col)
	{
		// Build a pool of 15 numbers for this column.
		TArray<int32> Pool;
		for (int32 n = ColMin[col]; n <= ColMax[col]; ++n)
		{
			Pool.Add(n);
		}
		// Shuffle.
		for (int32 i = Pool.Num() - 1; i > 0; --i)
		{
			Pool.Swap(i, FMath::RandRange(0, i));
		}

		// Pick the first 5 for the column.
		for (int32 row = 0; row < kBingoGM_Grid; ++row)
		{
			if (!Card[row].IsValidIndex(0))
			{
				Card[row].SetNum(kBingoGM_Grid);
			}
			Card[row][col] = Pool[row];
		}
	}

	// Centre cell is the free space (convention: store 0).
	Card[2][2] = 0;

	return Card;
}

bool ABingoGameMode::CheckBingo(const TArray<TArray<bool>>& Marked)
{
	if (Marked.Num() < kBingoGM_Grid)
	{
		return false;
	}

	// Check rows.
	for (int32 r = 0; r < kBingoGM_Grid; ++r)
	{
		bool bRowComplete = true;
		for (int32 c = 0; c < kBingoGM_Grid; ++c)
		{
			if (!Marked[r][c]) { bRowComplete = false; break; }
		}
		if (bRowComplete) return true;
	}

	// Check columns.
	for (int32 c = 0; c < kBingoGM_Grid; ++c)
	{
		bool bColComplete = true;
		for (int32 r = 0; r < kBingoGM_Grid; ++r)
		{
			if (!Marked[r][c]) { bColComplete = false; break; }
		}
		if (bColComplete) return true;
	}

	// Check diagonals.
	{
		bool bDiag1 = true, bDiag2 = true;
		for (int32 i = 0; i < kBingoGM_Grid; ++i)
		{
			if (!Marked[i][i]) bDiag1 = false;
			if (!Marked[i][kBingoGM_Grid - 1 - i]) bDiag2 = false;
		}
		if (bDiag1 || bDiag2) return true;
	}

	return false;
}

bool ABingoGameMode::FindNumberOnCard(const TArray<TArray<int32>>& Card, int32 Number, int32& OutRow, int32& OutCol)
{
	for (int32 r = 0; r < kBingoGM_Grid; ++r)
	{
		for (int32 c = 0; c < kBingoGM_Grid; ++c)
		{
			if (Card[r][c] == Number)
			{
				OutRow = r;
				OutCol = c;
				return true;
			}
		}
	}
	return false;
}
