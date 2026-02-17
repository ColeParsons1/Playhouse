// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Games/ColorCards/ColorCardsTypes.h"
#include "ColorCardsCPU.generated.h"

// ---------------------------------------------------------------------------
// UColorCardsCPU
// ---------------------------------------------------------------------------

/**
 * Standalone CPU AI brain for the Color Cards (Uno-style) mini-game.
 *
 * Strategy summary:
 *  - Play Draw-Two when the next opponent has few cards (punish).
 *  - Prefer action cards (Skip, Reverse) to disrupt opponents.
 *  - Match colour over value to maintain flexibility.
 *  - Save Wild / Wild-Draw-Four as a last resort.
 *  - When choosing a Wild colour, pick the colour most represented
 *    in the remaining hand.
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UColorCardsCPU : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Choose which card to play from the hand.
	 *
	 * @param Hand           The CPU player's current hand.
	 * @param CurrentColor   The active colour on the discard pile.
	 * @param CurrentValue   The face value of the top discard card.
	 * @return               Index into Hand of the card to play, or -1 to draw.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards|CPU")
	int32 ChooseCard(
		const TArray<FPHCUnoCard>& Hand,
		EPHCCardColor CurrentColor,
		EPHCCardValue CurrentValue) const;

	/**
	 * Choose a colour when playing a Wild card.
	 *
	 * @param Hand  The CPU player's hand (after removing the Wild).
	 * @return      The colour with the most cards in the remaining hand.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards|CPU")
	EPHCCardColor ChooseWildColor(const TArray<FPHCUnoCard>& Hand) const;

private:
	/** Returns true if the card can legally be played. */
	static bool CanPlay(
		const FPHCUnoCard& Card,
		EPHCCardColor CurrentColor,
		EPHCCardValue CurrentValue);

	/** Score a card for AI desirability (higher = prefer to play). */
	static int32 ScoreCard(const FPHCUnoCard& Card, EPHCCardColor CurrentColor);
};
