// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/ColorCards/ColorCardsGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AColorCardsGameMode::AColorCardsGameMode()
{
	GameType = EPHCGameType::ColorCards;
	MaxGameTime = 0.f; // Turn-based, no hard timer.
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void AColorCardsGameMode::StartGame()
{
	Super::StartGame();

	Hands.SetNum(NUM_PLAYERS);
	for (auto& Hand : Hands)
	{
		Hand.Reset();
	}

	DrawPile.Reset();
	DiscardPile.Reset();
	CurrentPlayerIndex = 0;
	bClockwise = true;
	DrawStack = 0;
	bWaitingForColorChoice = false;

	CreateDeck();
	DealHands(7);

	// Flip the first card onto the discard pile.
	// If it is a Wild, just set colour to Red.
	FPHCUnoCard FirstCard = DrawPile.Pop();
	DiscardPile.Add(FirstCard);
	CurrentColor = FirstCard.IsWild() ? EPHCCardColor::Red : FirstCard.Color;

	// If the first card is an action card, apply its effects as if "played".
	if (FirstCard.Value == EPHCCardValue::Skip ||
		FirstCard.Value == EPHCCardValue::Reverse ||
		FirstCard.Value == EPHCCardValue::DrawTwo)
	{
		ApplyCardEffects(FirstCard, -1);
	}

	OnTurnChanged.Broadcast(CurrentPlayerIndex);
}

void AColorCardsGameMode::EndGame(bool bWon)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CPUTurnTimerHandle);
	}
	Super::EndGame(bWon);
}

// ---------------------------------------------------------------------------
// Player actions
// ---------------------------------------------------------------------------

bool AColorCardsGameMode::PlayCard(int32 HandIndex)
{
	if (!bGameActive || CurrentPlayerIndex != 0 || bWaitingForColorChoice)
	{
		return false;
	}

	TArray<FPHCUnoCard>& Hand = Hands[0];
	if (!Hand.IsValidIndex(HandIndex))
	{
		return false;
	}

	const FPHCUnoCard Card = Hand[HandIndex];
	if (!CanPlayCard(Card))
	{
		return false;
	}

	// Remove from hand and place on discard.
	Hand.RemoveAt(HandIndex);
	DiscardPile.Add(Card);

	if (Card.IsWild())
	{
		// Player must choose a colour before the turn ends.
		bWaitingForColorChoice = true;
		// Apply draw effects immediately though.
		if (Card.Value == EPHCCardValue::WildDrawFour)
		{
			DrawStack += 4;
		}
		OnCardPlayed.Broadcast(0, Card);
		return true;
	}

	CurrentColor = Card.Color;
	ApplyCardEffects(Card, 0);
	OnCardPlayed.Broadcast(0, Card);

	// Check for winner.
	const int32 Winner = CheckWinner();
	if (Winner >= 0)
	{
		OnWinner.Broadcast(Winner);
		EndGame(Winner == 0);
		return true;
	}

	NextTurn();
	return true;
}

void AColorCardsGameMode::DrawCard()
{
	if (!bGameActive || CurrentPlayerIndex != 0 || bWaitingForColorChoice)
	{
		return;
	}

	if (DrawPile.Num() == 0)
	{
		ShuffleDiscardIntoDraw();
	}
	if (DrawPile.Num() == 0)
	{
		return; // truly empty -- should not happen with reshuffle.
	}

	// If there is a pending draw stack, draw that many.
	const int32 ToDraw = FMath::Max(DrawStack, 1);
	DrawStack = 0;

	for (int32 i = 0; i < ToDraw; ++i)
	{
		if (DrawPile.Num() == 0)
		{
			ShuffleDiscardIntoDraw();
		}
		if (DrawPile.Num() > 0)
		{
			Hands[0].Add(DrawPile.Pop());
		}
	}

	OnCardDrawn.Broadcast(0);
	NextTurn();
}

void AColorCardsGameMode::ChooseWildColor(EPHCCardColor Color)
{
	if (!bWaitingForColorChoice)
	{
		return;
	}

	bWaitingForColorChoice = false;
	CurrentColor = Color;
	OnColorChosen.Broadcast(Color);

	// Check for winner (edge case: played last card as Wild).
	const int32 Winner = CheckWinner();
	if (Winner >= 0)
	{
		OnWinner.Broadcast(Winner);
		EndGame(Winner == 0);
		return;
	}

	NextTurn();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool AColorCardsGameMode::CanPlayCard(const FPHCUnoCard& Card) const
{
	if (Card.IsWild())
	{
		return true;
	}

	// Match colour.
	if (Card.Color == CurrentColor)
	{
		return true;
	}

	// Match value with top of discard.
	if (DiscardPile.Num() > 0)
	{
		const FPHCUnoCard& Top = DiscardPile.Last();
		if (Card.Value == Top.Value)
		{
			return true;
		}
	}

	return false;
}

TArray<FPHCUnoCard> AColorCardsGameMode::GetPlayerHand() const
{
	return Hands.IsValidIndex(0) ? Hands[0] : TArray<FPHCUnoCard>();
}

int32 AColorCardsGameMode::GetHandCount(int32 PlayerIndex) const
{
	return Hands.IsValidIndex(PlayerIndex) ? Hands[PlayerIndex].Num() : 0;
}

FPHCUnoCard AColorCardsGameMode::GetTopDiscard() const
{
	if (DiscardPile.Num() > 0)
	{
		return DiscardPile.Last();
	}
	return FPHCUnoCard();
}

// ---------------------------------------------------------------------------
// Deck management
// ---------------------------------------------------------------------------

void AColorCardsGameMode::CreateDeck()
{
	DrawPile.Reset();

	// Coloured cards: 4 colours x (one 0 + two each of 1-9, Skip, Reverse, DrawTwo) = 100 cards.
	const TArray<EPHCCardColor> Colors = {
		EPHCCardColor::Red,
		EPHCCardColor::Blue,
		EPHCCardColor::Green,
		EPHCCardColor::Yellow
	};

	for (EPHCCardColor Col : Colors)
	{
		// One zero per colour.
		DrawPile.Add({ Col, EPHCCardValue::Zero });

		// Two each of 1-9 and action cards.
		const TArray<EPHCCardValue> Values = {
			EPHCCardValue::One, EPHCCardValue::Two, EPHCCardValue::Three,
			EPHCCardValue::Four, EPHCCardValue::Five, EPHCCardValue::Six,
			EPHCCardValue::Seven, EPHCCardValue::Eight, EPHCCardValue::Nine,
			EPHCCardValue::Skip, EPHCCardValue::Reverse, EPHCCardValue::DrawTwo
		};
		for (EPHCCardValue Val : Values)
		{
			DrawPile.Add({ Col, Val });
			DrawPile.Add({ Col, Val });
		}
	}

	// Wild cards: 4 Wild + 4 Wild Draw Four = 8.
	for (int32 i = 0; i < 4; ++i)
	{
		DrawPile.Add({ EPHCCardColor::Wild, EPHCCardValue::WildCard });
		DrawPile.Add({ EPHCCardColor::Wild, EPHCCardValue::WildDrawFour });
	}

	// Shuffle (Fisher-Yates).
	for (int32 i = DrawPile.Num() - 1; i > 0; --i)
	{
		DrawPile.Swap(i, FMath::RandRange(0, i));
	}
}

void AColorCardsGameMode::ShuffleDiscardIntoDraw()
{
	if (DiscardPile.Num() <= 1)
	{
		return; // Keep the top card on the discard.
	}

	const FPHCUnoCard Top = DiscardPile.Last();
	DiscardPile.Pop();

	DrawPile.Append(DiscardPile);
	DiscardPile.Reset();
	DiscardPile.Add(Top);

	// Shuffle.
	for (int32 i = DrawPile.Num() - 1; i > 0; --i)
	{
		DrawPile.Swap(i, FMath::RandRange(0, i));
	}
}

void AColorCardsGameMode::DealHands(int32 InitialHandSize)
{
	for (int32 p = 0; p < NUM_PLAYERS; ++p)
	{
		for (int32 c = 0; c < InitialHandSize; ++c)
		{
			if (DrawPile.Num() > 0)
			{
				Hands[p].Add(DrawPile.Pop());
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Turn management
// ---------------------------------------------------------------------------

void AColorCardsGameMode::NextTurn()
{
	const int32 Direction = bClockwise ? 1 : -1;
	CurrentPlayerIndex = (CurrentPlayerIndex + Direction + NUM_PLAYERS) % NUM_PLAYERS;

	OnTurnChanged.Broadcast(CurrentPlayerIndex);

	// If it is a CPU player's turn, schedule AI move after a brief delay.
	if (CurrentPlayerIndex != 0 && bGameActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CPUTurnTimerHandle,
				this,
				&AColorCardsGameMode::CPUTurn,
				1.0f,
				/*bLoop=*/ false
			);
		}
	}
}

void AColorCardsGameMode::CPUTurn()
{
	if (!bGameActive || CurrentPlayerIndex == 0)
	{
		return;
	}

	const int32 Idx = CurrentPlayerIndex;

	// Resolve pending draw stack first.
	if (DrawStack > 0)
	{
		for (int32 i = 0; i < DrawStack; ++i)
		{
			if (DrawPile.Num() == 0) ShuffleDiscardIntoDraw();
			if (DrawPile.Num() > 0) Hands[Idx].Add(DrawPile.Pop());
		}
		DrawStack = 0;
		OnCardDrawn.Broadcast(Idx);
		NextTurn();
		return;
	}

	const int32 CardIdx = CPUChooseCard(Idx);
	if (CardIdx >= 0)
	{
		FPHCUnoCard Card = Hands[Idx][CardIdx];
		Hands[Idx].RemoveAt(CardIdx);
		DiscardPile.Add(Card);

		if (Card.IsWild())
		{
			CurrentColor = CPUChooseWildColor(Idx);
			OnColorChosen.Broadcast(CurrentColor);
			if (Card.Value == EPHCCardValue::WildDrawFour)
			{
				DrawStack += 4;
			}
		}
		else
		{
			CurrentColor = Card.Color;
			ApplyCardEffects(Card, Idx);
		}

		OnCardPlayed.Broadcast(Idx, Card);

		const int32 Winner = CheckWinner();
		if (Winner >= 0)
		{
			OnWinner.Broadcast(Winner);
			EndGame(Winner == 0);
			return;
		}
	}
	else
	{
		// Draw a card.
		if (DrawPile.Num() == 0) ShuffleDiscardIntoDraw();
		if (DrawPile.Num() > 0)
		{
			Hands[Idx].Add(DrawPile.Pop());
		}
		OnCardDrawn.Broadcast(Idx);
	}

	NextTurn();
}

int32 AColorCardsGameMode::CPUChooseCard(int32 PlayerIdx) const
{
	if (!Hands.IsValidIndex(PlayerIdx))
	{
		return -1;
	}

	const TArray<FPHCUnoCard>& Hand = Hands[PlayerIdx];

	// Prefer action cards, then number matches, then wilds.
	int32 BestIdx = -1;
	int32 BestPriority = -1; // higher = better

	for (int32 i = 0; i < Hand.Num(); ++i)
	{
		if (!CanPlayCard(Hand[i]))
		{
			continue;
		}

		int32 Priority = 0;
		const FPHCUnoCard& C = Hand[i];

		if (C.Value == EPHCCardValue::Skip || C.Value == EPHCCardValue::Reverse)
		{
			Priority = 3; // action cards first
		}
		else if (C.Value == EPHCCardValue::DrawTwo)
		{
			Priority = 4; // draw-two is very strong
		}
		else if (C.IsWild())
		{
			Priority = 1; // hold wilds unless nothing else playable
		}
		else
		{
			Priority = 2; // number card
		}

		if (Priority > BestPriority)
		{
			BestPriority = Priority;
			BestIdx = i;
		}
	}

	return BestIdx;
}

EPHCCardColor AColorCardsGameMode::CPUChooseWildColor(int32 PlayerIdx) const
{
	if (!Hands.IsValidIndex(PlayerIdx))
	{
		return EPHCCardColor::Red;
	}

	// Count coloured cards in hand, pick the most common.
	int32 Counts[4] = { 0, 0, 0, 0 };
	for (const FPHCUnoCard& C : Hands[PlayerIdx])
	{
		if (!C.IsWild())
		{
			Counts[static_cast<uint8>(C.Color)]++;
		}
	}

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

void AColorCardsGameMode::ApplyCardEffects(const FPHCUnoCard& Card, int32 PlayerIdx)
{
	switch (Card.Value)
	{
	case EPHCCardValue::Skip:
	{
		// Skip the next player by advancing an extra step.
		const int32 Dir = bClockwise ? 1 : -1;
		CurrentPlayerIndex = (CurrentPlayerIndex + Dir + NUM_PLAYERS) % NUM_PLAYERS;
		break;
	}
	case EPHCCardValue::Reverse:
	{
		bClockwise = !bClockwise;
		// In a 2-player game reverse acts as skip, but with 4 players
		// it just reverses direction (already handled by NextTurn).
		break;
	}
	case EPHCCardValue::DrawTwo:
	{
		DrawStack += 2;
		break;
	}
	default:
		break;
	}
}

int32 AColorCardsGameMode::CheckWinner() const
{
	for (int32 i = 0; i < NUM_PLAYERS; ++i)
	{
		if (Hands.IsValidIndex(i) && Hands[i].Num() == 0)
		{
			return i;
		}
	}
	return -1;
}
