// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/LuckySlots/SlotsGameMode.h"
#include "Core/PHCGameInstance.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ASlotsGameMode::ASlotsGameMode()
{
	GameType = EPHCGameType::LuckySlots;
	MaxGameTime = 0.f; // No timer -- play until out of coins or quit.

	InitPayTable();
	InitWeights();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void ASlotsGameMode::StartGame()
{
	Super::StartGame();

	bSpinning = false;
	ReelResults.Reset();

	// Seed the balance from the player's coin count.
	if (CachedGameInstance)
	{
		CoinBalance = CachedGameInstance->LocalPlayerProfile.Coins;
	}
	else
	{
		CoinBalance = 500; // Fallback for testing.
	}

	// Clamp the bet to the available balance.
	BetAmount = FMath::Clamp(BetAmount, MinBet, FMath::Min(MaxBet, CoinBalance));
}

// ---------------------------------------------------------------------------
// Gameplay
// ---------------------------------------------------------------------------

void ASlotsGameMode::Spin()
{
	if (bSpinning || !bGameActive)
	{
		return;
	}

	if (CoinBalance < BetAmount)
	{
		// Not enough coins -- end the game.
		EndGame(/*bWon=*/ false);
		return;
	}

	bSpinning = true;

	// Deduct the bet.
	CoinBalance -= BetAmount;

	// Generate 3 reel results.
	ReelResults.Reset(3);
	for (int32 i = 0; i < 3; ++i)
	{
		ReelResults.Add(GenerateSymbol());
	}

	bSpinning = false;

	OnSpinComplete.Broadcast(ReelResults);

	// Check for a win.
	const int32 Multiplier = CheckWin(ReelResults);
	if (Multiplier > 0)
	{
		const int32 Payout = BetAmount * Multiplier;
		CoinBalance += Payout;
		AddScore(Payout);
		OnWin.Broadcast(Payout);
	}
}

void ASlotsGameMode::SetBet(int32 Amount)
{
	BetAmount = FMath::Clamp(Amount, MinBet, FMath::Min(MaxBet, CoinBalance));
}

// ---------------------------------------------------------------------------
// Symbol generation
// ---------------------------------------------------------------------------

EPHCSlotSymbol ASlotsGameMode::GenerateSymbol() const
{
	int32 Roll = FMath::RandRange(1, TotalWeight);

	for (const auto& Pair : SymbolWeights)
	{
		Roll -= Pair.Value;
		if (Roll <= 0)
		{
			return Pair.Key;
		}
	}

	// Fallback (should never reach here).
	return EPHCSlotSymbol::Cherry;
}

int32 ASlotsGameMode::CheckWin(const TArray<EPHCSlotSymbol>& Results) const
{
	if (Results.Num() < 3)
	{
		return 0;
	}

	EPHCSlotSymbol A = Results[0];
	EPHCSlotSymbol B = Results[1];
	EPHCSlotSymbol C = Results[2];

	// Replace Wilds with the most beneficial match.
	auto ResolveWild = [](EPHCSlotSymbol Sym, EPHCSlotSymbol Alt) -> EPHCSlotSymbol
	{
		return (Sym == EPHCSlotSymbol::Wild) ? Alt : Sym;
	};

	// Simple wild substitution for 3-of-a-kind.
	// If all three are Wild, treat as Diamond (highest payout).
	if (A == EPHCSlotSymbol::Wild && B == EPHCSlotSymbol::Wild && C == EPHCSlotSymbol::Wild)
	{
		const int32* Multiplier = PayTable.Find(EPHCSlotSymbol::Diamond);
		return Multiplier ? *Multiplier : 0;
	}

	// Find a non-wild symbol to anchor.
	EPHCSlotSymbol Anchor = EPHCSlotSymbol::Cherry;
	if (A != EPHCSlotSymbol::Wild) Anchor = A;
	else if (B != EPHCSlotSymbol::Wild) Anchor = B;
	else Anchor = C;

	EPHCSlotSymbol RA = ResolveWild(A, Anchor);
	EPHCSlotSymbol RB = ResolveWild(B, Anchor);
	EPHCSlotSymbol RC = ResolveWild(C, Anchor);

	if (RA == RB && RB == RC)
	{
		const int32* Multiplier = PayTable.Find(RA);
		return Multiplier ? *Multiplier : 0;
	}

	// Two of a kind (any two match, counting wilds) = small win.
	if (RA == RB || RB == RC || RA == RC)
	{
		return 1; // 1x bet for a pair.
	}

	return 0;
}

// ---------------------------------------------------------------------------
// Initialisation helpers
// ---------------------------------------------------------------------------

void ASlotsGameMode::InitPayTable()
{
	PayTable.Reset();
	PayTable.Add(EPHCSlotSymbol::Cherry,  3);
	PayTable.Add(EPHCSlotSymbol::Lemon,   5);
	PayTable.Add(EPHCSlotSymbol::Orange,  8);
	PayTable.Add(EPHCSlotSymbol::Plum,    10);
	PayTable.Add(EPHCSlotSymbol::Bell,    15);
	PayTable.Add(EPHCSlotSymbol::Bar,     25);
	PayTable.Add(EPHCSlotSymbol::Seven,   50);
	PayTable.Add(EPHCSlotSymbol::Diamond, 100);
	PayTable.Add(EPHCSlotSymbol::Wild,    200);
}

void ASlotsGameMode::InitWeights()
{
	SymbolWeights.Reset();

	// More common symbols have higher weights.
	SymbolWeights.Add(EPHCSlotSymbol::Cherry,  25);
	SymbolWeights.Add(EPHCSlotSymbol::Lemon,   20);
	SymbolWeights.Add(EPHCSlotSymbol::Orange,  18);
	SymbolWeights.Add(EPHCSlotSymbol::Plum,    15);
	SymbolWeights.Add(EPHCSlotSymbol::Bell,    10);
	SymbolWeights.Add(EPHCSlotSymbol::Bar,     6);
	SymbolWeights.Add(EPHCSlotSymbol::Seven,   3);
	SymbolWeights.Add(EPHCSlotSymbol::Diamond, 2);
	SymbolWeights.Add(EPHCSlotSymbol::Wild,    1);

	TotalWeight = 0;
	for (const auto& Pair : SymbolWeights)
	{
		TotalWeight += Pair.Value;
	}
}
