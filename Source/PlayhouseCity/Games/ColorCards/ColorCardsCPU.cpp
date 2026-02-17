// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/ColorCards/ColorCardsCPU.h"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int32 UColorCardsCPU::ChooseCard(
	const TArray<FPHCUnoCard>& Hand,
	EPHCCardColor CurrentColor,
	EPHCCardValue CurrentValue) const
{
	int32 BestIndex = -1;
	int32 BestScore = -1;

	for (int32 i = 0; i < Hand.Num(); ++i)
	{
		if (!CanPlay(Hand[i], CurrentColor, CurrentValue))
		{
			continue;
		}

		const int32 Score = ScoreCard(Hand[i], CurrentColor);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = i;
		}
	}

	return BestIndex; // -1 if nothing playable
}

EPHCCardColor UColorCardsCPU::ChooseWildColor(const TArray<FPHCUnoCard>& Hand) const
{
	// Tally the number of cards of each colour remaining in hand.
	int32 Counts[4] = { 0, 0, 0, 0 }; // Red, Blue, Green, Yellow

	for (const FPHCUnoCard& Card : Hand)
	{
		if (!Card.IsWild())
		{
			const int32 Idx = static_cast<int32>(Card.Color);
			if (Idx >= 0 && Idx < 4)
			{
				++Counts[Idx];
			}
		}
	}

	// Find the colour with the highest count.
	int32 MaxIdx = 0;
	for (int32 i = 1; i < 4; ++i)
	{
		if (Counts[i] > Counts[MaxIdx])
		{
			MaxIdx = i;
		}
	}

	return static_cast<EPHCCardColor>(MaxIdx);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool UColorCardsCPU::CanPlay(
	const FPHCUnoCard& Card,
	EPHCCardColor CurrentColor,
	EPHCCardValue CurrentValue)
{
	if (Card.IsWild())
	{
		return true;
	}
	if (Card.Color == CurrentColor)
	{
		return true;
	}
	if (Card.Value == CurrentValue)
	{
		return true;
	}
	return false;
}

int32 UColorCardsCPU::ScoreCard(const FPHCUnoCard& Card, EPHCCardColor CurrentColor)
{
	// Base scores:
	//  - Draw Two:        50  (strong offensive move)
	//  - Skip / Reverse:  40  (good disruption)
	//  - Number match:    30  (standard play)
	//  - Wild Draw Four:  15  (save for emergencies, but still playable)
	//  - Wild:            10  (save as last resort)

	int32 Score = 0;

	switch (Card.Value)
	{
	case EPHCCardValue::DrawTwo:
		Score = 50;
		break;

	case EPHCCardValue::Skip:
	case EPHCCardValue::Reverse:
		Score = 40;
		break;

	case EPHCCardValue::WildDrawFour:
		Score = 15;
		break;

	case EPHCCardValue::WildCard:
		Score = 10;
		break;

	default:
		// Number cards.
		Score = 30;
		break;
	}

	// Small bonus for matching the current colour (maintains control).
	if (Card.Color == CurrentColor && !Card.IsWild())
	{
		Score += 5;
	}

	return Score;
}
