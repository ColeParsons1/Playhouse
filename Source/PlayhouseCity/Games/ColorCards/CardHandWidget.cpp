// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/ColorCards/CardHandWidget.h"
#include "Games/ColorCards/ColorCardsGameMode.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/ScrollBoxSlot.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UCardHandWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (DrawPileButton)
	{
		DrawPileButton->OnClicked.AddDynamic(this, &UCardHandWidget::OnDrawPileClicked);
	}

	// Wild colour picker buttons.
	if (WildRedButton)    WildRedButton->OnClicked.AddDynamic(this, &UCardHandWidget::OnWildRedClicked);
	if (WildBlueButton)   WildBlueButton->OnClicked.AddDynamic(this, &UCardHandWidget::OnWildBlueClicked);
	if (WildGreenButton)  WildGreenButton->OnClicked.AddDynamic(this, &UCardHandWidget::OnWildGreenClicked);
	if (WildYellowButton) WildYellowButton->OnClicked.AddDynamic(this, &UCardHandWidget::OnWildYellowClicked);

	// Hide colour picker initially.
	if (WildColorPickerOverlay)
	{
		WildColorPickerOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UCardHandWidget::SetHand(const TArray<FPHCUnoCard>& Cards, bool bInteractable)
{
	CachedHand = Cards;
	CardButtons.Reset();

	if (!CardContainer)
	{
		return;
	}

	CardContainer->ClearChildren();

	for (int32 i = 0; i < Cards.Num(); ++i)
	{
		UButton* Btn = NewObject<UButton>(this);
		UTextBlock* Txt = NewObject<UTextBlock>(this);

		if (!Btn || !Txt)
		{
			continue;
		}

		Txt->SetText(FText::FromString(GetCardDisplayText(Cards[i])));
		Txt->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo Font = Txt->GetFont();
		Font.Size = 16;
		Txt->SetFont(Font);

		Btn->AddChild(Txt);

		// Tint the button background to the card colour.
		FButtonStyle Style = Btn->GetStyle();
		const FLinearColor BtnColor = GetCardColor(Cards[i].Color);
		Style.Normal.TintColor = FSlateColor(BtnColor);
		Style.Hovered.TintColor = FSlateColor(BtnColor * 1.2f);
		Style.Pressed.TintColor = FSlateColor(BtnColor * 0.8f);
		Btn->SetStyle(Style);
		Btn->SetIsEnabled(bInteractable);

		Btn->OnClicked.AddDynamic(this, &UCardHandWidget::HandleAnyCardClicked);

		CardContainer->AddChild(Btn);
		CardButtons.Add(Btn);
	}
}

void UCardHandWidget::HighlightPlayableCards(EPHCCardColor InCurrentColor, EPHCCardValue InCurrentValue)
{
	for (int32 i = 0; i < CachedHand.Num(); ++i)
	{
		if (!CardButtons.IsValidIndex(i) || !CardButtons[i])
		{
			continue;
		}

		const FPHCUnoCard& C = CachedHand[i];
		const bool bPlayable = C.IsWild() || C.Color == InCurrentColor || C.Value == InCurrentValue;

		CardButtons[i]->SetIsEnabled(bPlayable);
	}
}

void UCardHandWidget::ShowWildColorPicker()
{
	if (WildColorPickerOverlay)
	{
		WildColorPickerOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCardHandWidget::HideWildColorPicker()
{
	if (WildColorPickerOverlay)
	{
		WildColorPickerOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCardHandWidget::UpdateDiscardPile(const FPHCUnoCard& TopCard)
{
	if (DiscardPileImage)
	{
		DiscardPileImage->SetColorAndOpacity(GetCardColor(TopCard.Color));
	}

	// Also update the current-colour indicator.
	if (CurrentColorIndicator && GameModeRef)
	{
		CurrentColorIndicator->SetColorAndOpacity(GetCardColor(GameModeRef->GetCurrentColor()));
	}
}

void UCardHandWidget::SetGameMode(AColorCardsGameMode* InGameMode)
{
	GameModeRef = InGameMode;
}

void UCardHandWidget::UpdateTurnIndicator(int32 CurrentPlayer)
{
	if (PlayerTurnIndicator)
	{
		if (CurrentPlayer == 0)
		{
			PlayerTurnIndicator->SetText(FText::FromString(TEXT("Your Turn")));
		}
		else
		{
			PlayerTurnIndicator->SetText(FText::FromString(
				FString::Printf(TEXT("Player %d's Turn"), CurrentPlayer + 1)));
		}
	}
}

void UCardHandWidget::UpdateDirection(bool bIsClockwise)
{
	if (DirectionIndicator)
	{
		DirectionIndicator->SetText(FText::FromString(
			bIsClockwise ? TEXT(">>>") : TEXT("<<<")));
	}
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

FLinearColor UCardHandWidget::GetCardColor(EPHCCardColor InColor)
{
	switch (InColor)
	{
	case EPHCCardColor::Red:    return FLinearColor(0.85f, 0.1f, 0.1f, 1.f);
	case EPHCCardColor::Blue:   return FLinearColor(0.1f, 0.3f, 0.85f, 1.f);
	case EPHCCardColor::Green:  return FLinearColor(0.1f, 0.7f, 0.2f, 1.f);
	case EPHCCardColor::Yellow: return FLinearColor(0.9f, 0.85f, 0.1f, 1.f);
	case EPHCCardColor::Wild:   return FLinearColor(0.3f, 0.3f, 0.3f, 1.f);
	default:                    return FLinearColor::White;
	}
}

FString UCardHandWidget::GetCardDisplayText(const FPHCUnoCard& Card)
{
	FString ColorPrefix;
	switch (Card.Color)
	{
	case EPHCCardColor::Red:    ColorPrefix = TEXT("R"); break;
	case EPHCCardColor::Blue:   ColorPrefix = TEXT("B"); break;
	case EPHCCardColor::Green:  ColorPrefix = TEXT("G"); break;
	case EPHCCardColor::Yellow: ColorPrefix = TEXT("Y"); break;
	case EPHCCardColor::Wild:   ColorPrefix = TEXT("W"); break;
	}

	FString ValueStr;
	switch (Card.Value)
	{
	case EPHCCardValue::Zero:           ValueStr = TEXT("0"); break;
	case EPHCCardValue::One:            ValueStr = TEXT("1"); break;
	case EPHCCardValue::Two:            ValueStr = TEXT("2"); break;
	case EPHCCardValue::Three:          ValueStr = TEXT("3"); break;
	case EPHCCardValue::Four:           ValueStr = TEXT("4"); break;
	case EPHCCardValue::Five:           ValueStr = TEXT("5"); break;
	case EPHCCardValue::Six:            ValueStr = TEXT("6"); break;
	case EPHCCardValue::Seven:          ValueStr = TEXT("7"); break;
	case EPHCCardValue::Eight:          ValueStr = TEXT("8"); break;
	case EPHCCardValue::Nine:           ValueStr = TEXT("9"); break;
	case EPHCCardValue::Skip:           ValueStr = TEXT("S"); break;
	case EPHCCardValue::Reverse:        ValueStr = TEXT("R"); break;
	case EPHCCardValue::DrawTwo:        ValueStr = TEXT("+2"); break;
	case EPHCCardValue::WildCard:       ValueStr = TEXT("W"); break;
	case EPHCCardValue::WildDrawFour:   ValueStr = TEXT("+4"); break;
	}

	return FString::Printf(TEXT("%s %s"), *ColorPrefix, *ValueStr);
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UCardHandWidget::OnDrawPileClicked()
{
	if (GameModeRef)
	{
		GameModeRef->DrawCard();
	}
}

void UCardHandWidget::HandleAnyCardClicked()
{
	// Identify which card button was clicked via IsHovered().
	for (int32 i = 0; i < CardButtons.Num(); ++i)
	{
		if (CardButtons[i] && CardButtons[i]->IsHovered())
		{
			OnCardSelected.Broadcast(i);

			if (GameModeRef)
			{
				const bool bPlayed = GameModeRef->PlayCard(i);

				// If the played card was a Wild, show the colour picker.
				if (bPlayed && CachedHand.IsValidIndex(i) && CachedHand[i].IsWild())
				{
					ShowWildColorPicker();
				}
			}
			break;
		}
	}
}

void UCardHandWidget::OnWildRedClicked()
{
	HideWildColorPicker();
	if (GameModeRef) GameModeRef->ChooseWildColor(EPHCCardColor::Red);
	OnWildColorSelected.Broadcast(EPHCCardColor::Red);
}

void UCardHandWidget::OnWildBlueClicked()
{
	HideWildColorPicker();
	if (GameModeRef) GameModeRef->ChooseWildColor(EPHCCardColor::Blue);
	OnWildColorSelected.Broadcast(EPHCCardColor::Blue);
}

void UCardHandWidget::OnWildGreenClicked()
{
	HideWildColorPicker();
	if (GameModeRef) GameModeRef->ChooseWildColor(EPHCCardColor::Green);
	OnWildColorSelected.Broadcast(EPHCCardColor::Green);
}

void UCardHandWidget::OnWildYellowClicked()
{
	HideWildColorPicker();
	if (GameModeRef) GameModeRef->ChooseWildColor(EPHCCardColor::Yellow);
	OnWildColorSelected.Broadcast(EPHCCardColor::Yellow);
}
