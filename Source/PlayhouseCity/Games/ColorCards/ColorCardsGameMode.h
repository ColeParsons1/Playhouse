// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "Games/ColorCards/ColorCardsTypes.h"
#include "ColorCardsGameMode.generated.h"

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnColorCardPlayed, int32, PlayerIndex, FPHCUnoCard, Card);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorCardDrawn, int32, PlayerIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorCardTurnChanged, int32, NewPlayerIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorCardColorChosen, EPHCCardColor, ChosenColor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnColorCardWinner, int32, WinnerIndex);

// ---------------------------------------------------------------------------
// AColorCardsGameMode
// ---------------------------------------------------------------------------

/**
 * Color Cards mini-game mode (Uno-style).
 *
 * 4 players (index 0 = human, 1-3 = CPU). Each starts with 7 cards.
 * Standard Uno rules: match colour or value, action cards (Skip, Reverse,
 * Draw Two), Wild and Wild Draw Four. First to empty their hand wins.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AColorCardsGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AColorCardsGameMode();

	// -- Game flow --------------------------------------------------------

	virtual void StartGame() override;
	virtual void EndGame(bool bWon) override;

	// -- Player actions ---------------------------------------------------

	/** Attempt to play the card at the given index in the human player's hand. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	bool PlayCard(int32 HandIndex);

	/** Draw a card from the draw pile. If playable, it can optionally be played. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void DrawCard();

	/** Called by the HUD when the human player picks a colour for a Wild. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void ChooseWildColor(EPHCCardColor Color);

	// -- Queries ----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	bool CanPlayCard(const FPHCUnoCard& Card) const;

	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	TArray<FPHCUnoCard> GetPlayerHand() const;

	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	int32 GetHandCount(int32 PlayerIndex) const;

	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	FPHCUnoCard GetTopDiscard() const;

	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	EPHCCardColor GetCurrentColor() const { return CurrentColor; }

	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	int32 GetCurrentPlayerIndex() const { return CurrentPlayerIndex; }

	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	bool IsClockwise() const { return bClockwise; }

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|ColorCards")
	FOnColorCardPlayed OnCardPlayed;

	UPROPERTY(BlueprintAssignable, Category = "PHC|ColorCards")
	FOnColorCardDrawn OnCardDrawn;

	UPROPERTY(BlueprintAssignable, Category = "PHC|ColorCards")
	FOnColorCardTurnChanged OnTurnChanged;

	UPROPERTY(BlueprintAssignable, Category = "PHC|ColorCards")
	FOnColorCardColorChosen OnColorChosen;

	UPROPERTY(BlueprintAssignable, Category = "PHC|ColorCards")
	FOnColorCardWinner OnWinner;

protected:
	// -- State ------------------------------------------------------------

	TArray<FPHCUnoCard> DrawPile;
	TArray<FPHCUnoCard> DiscardPile;

	/** Hands for all 4 players. Index 0 = human. */
	TArray<TArray<FPHCUnoCard>> Hands;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|ColorCards")
	int32 CurrentPlayerIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|ColorCards")
	EPHCCardColor CurrentColor = EPHCCardColor::Red;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|ColorCards")
	bool bClockwise = true;

	/** Accumulated draw penalty (Draw Two / Wild Draw Four stacking). */
	int32 DrawStack = 0;

	/** True while the game is waiting for the player to pick a Wild colour. */
	bool bWaitingForColorChoice = false;

	/** Timer handle for CPU turn delay (so moves are visible). */
	FTimerHandle CPUTurnTimerHandle;

	// -- Internal ---------------------------------------------------------

	/** Build and shuffle a standard 108-card deck. */
	void CreateDeck();

	/** Shuffle the discard pile back into the draw pile. */
	void ShuffleDiscardIntoDraw();

	/** Deal InitialHandSize cards to each player. */
	void DealHands(int32 InitialHandSize = 7);

	/** Advance to the next player (respects direction). */
	void NextTurn();

	/** Execute a CPU player's turn after a short delay. */
	void CPUTurn();

	/** CPU AI: choose which card to play, or -1 to draw. */
	int32 CPUChooseCard(int32 PlayerIdx) const;

	/** CPU AI: choose a colour when playing a Wild. */
	EPHCCardColor CPUChooseWildColor(int32 PlayerIdx) const;

	/** Apply the effects of the card that was just played. */
	void ApplyCardEffects(const FPHCUnoCard& Card, int32 PlayerIdx);

	/** Check if any player has won (empty hand). Returns index or -1. */
	int32 CheckWinner() const;

	static constexpr int32 NUM_PLAYERS = 4;
};
