// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/LuckySlots/SlotsWidget.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void USlotsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SpinButton)    SpinButton->OnClicked.AddDynamic(this, &USlotsWidget::OnSpinClicked);
	if (BetUpButton)   BetUpButton->OnClicked.AddDynamic(this, &USlotsWidget::OnBetUpClicked);
	if (BetDownButton) BetDownButton->OnClicked.AddDynamic(this, &USlotsWidget::OnBetDownClicked);
	if (PayTableButton) PayTableButton->OnClicked.AddDynamic(this, &USlotsWidget::OnPayTableClicked);

	// Hide win text initially.
	if (WinText)
	{
		WinText->SetVisibility(ESlateVisibility::Hidden);
	}
}

void USlotsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAnimating)
	{
		return;
	}

	AnimClock += InDeltaTime;

	// Cycle through random symbols while animating.
	UImage* Reels[3] = { Reel1, Reel2, Reel3 };

	for (int32 i = 0; i < 3; ++i)
	{
		if (AnimClock < ReelStopTime[i])
		{
			// Still spinning -- show a random symbol for visual effect.
			const int32 RandSym = FMath::RandRange(0, static_cast<int32>(EPHCSlotSymbol::Wild));
			SetReelSymbol(Reels[i], static_cast<EPHCSlotSymbol>(RandSym));
		}
		else if (ReelAnimProgress[i] < 1.f)
		{
			// This reel just stopped.
			ReelAnimProgress[i] = 1.f;
			if (PendingResults.IsValidIndex(i))
			{
				SetReelSymbol(Reels[i], PendingResults[i]);
			}
			++ReelsStopped;

			if (ReelsStopped >= 3)
			{
				OnAllReelsStopped();
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void USlotsWidget::SetGameMode(ASlotsGameMode* InGameMode)
{
	GameModeRef = InGameMode;
	if (GameModeRef)
	{
		GameModeRef->OnSpinComplete.AddDynamic(this, &USlotsWidget::HandleSpinComplete);
		GameModeRef->OnWin.AddDynamic(this, &USlotsWidget::HandleWin);
		RefreshUI();
	}
}

void USlotsWidget::ShowWin(int32 Amount)
{
	if (WinText)
	{
		WinText->SetText(FText::FromString(
			FString::Printf(TEXT("WIN: %d"), Amount)));
		WinText->SetVisibility(ESlateVisibility::Visible);
	}
}

void USlotsWidget::UpdateBalance(int32 Coins)
{
	if (BalanceText)
	{
		BalanceText->SetText(FText::FromString(
			FString::Printf(TEXT("Balance: %d"), Coins)));
	}
}

FLinearColor USlotsWidget::GetSymbolColor(EPHCSlotSymbol Symbol)
{
	switch (Symbol)
	{
	case EPHCSlotSymbol::Cherry:  return FLinearColor(0.9f, 0.1f, 0.1f, 1.f);
	case EPHCSlotSymbol::Lemon:   return FLinearColor(1.f, 1.f, 0.2f, 1.f);
	case EPHCSlotSymbol::Orange:  return FLinearColor(1.f, 0.6f, 0.1f, 1.f);
	case EPHCSlotSymbol::Plum:    return FLinearColor(0.5f, 0.1f, 0.6f, 1.f);
	case EPHCSlotSymbol::Bell:    return FLinearColor(1.f, 0.85f, 0.f, 1.f);
	case EPHCSlotSymbol::Bar:     return FLinearColor(0.3f, 0.3f, 0.3f, 1.f);
	case EPHCSlotSymbol::Seven:   return FLinearColor(0.9f, 0.2f, 0.2f, 1.f);
	case EPHCSlotSymbol::Diamond: return FLinearColor(0.4f, 0.8f, 1.f, 1.f);
	case EPHCSlotSymbol::Wild:    return FLinearColor(0.f, 1.f, 0.5f, 1.f);
	default:                      return FLinearColor::White;
	}
}

FString USlotsWidget::GetSymbolName(EPHCSlotSymbol Symbol)
{
	const UEnum* Enum = StaticEnum<EPHCSlotSymbol>();
	if (Enum)
	{
		return Enum->GetDisplayNameTextByValue(static_cast<int64>(Symbol)).ToString();
	}
	return TEXT("?");
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void USlotsWidget::OnSpinClicked()
{
	if (bAnimating || !GameModeRef)
	{
		return;
	}

	// Hide previous win text.
	if (WinText)
	{
		WinText->SetVisibility(ESlateVisibility::Hidden);
	}

	// Disable spin button during animation.
	if (SpinButton)
	{
		SpinButton->SetIsEnabled(false);
	}

	GameModeRef->Spin();
}

void USlotsWidget::OnBetUpClicked()
{
	if (GameModeRef)
	{
		GameModeRef->SetBet(GameModeRef->GetBetAmount() + 5);
		RefreshUI();
	}
}

void USlotsWidget::OnBetDownClicked()
{
	if (GameModeRef)
	{
		GameModeRef->SetBet(GameModeRef->GetBetAmount() - 5);
		RefreshUI();
	}
}

void USlotsWidget::OnPayTableClicked()
{
	// Placeholder: toggle pay table visibility or open a sub-widget.
	// Implementation depends on additional UMG layout in the designer.
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void USlotsWidget::HandleSpinComplete(const TArray<EPHCSlotSymbol>& Results)
{
	StartReelAnimation(Results);
}

void USlotsWidget::HandleWin(int32 PayoutAmount)
{
	ShowWin(PayoutAmount);
}

// ---------------------------------------------------------------------------
// Reel animation
// ---------------------------------------------------------------------------

void USlotsWidget::StartReelAnimation(const TArray<EPHCSlotSymbol>& Results)
{
	bAnimating = true;
	AnimClock = 0.f;
	ReelsStopped = 0;
	PendingResults = Results;

	for (int32 i = 0; i < 3; ++i)
	{
		ReelAnimProgress[i] = 0.f;
	}
}

void USlotsWidget::OnAllReelsStopped()
{
	bAnimating = false;

	// Re-enable spin button.
	if (SpinButton)
	{
		SpinButton->SetIsEnabled(true);
	}

	RefreshUI();
}

void USlotsWidget::SetReelSymbol(UImage* ReelImage, EPHCSlotSymbol Symbol)
{
	if (ReelImage)
	{
		ReelImage->SetColorAndOpacity(GetSymbolColor(Symbol));
	}
}

void USlotsWidget::RefreshUI()
{
	if (GameModeRef)
	{
		if (BetAmountText)
		{
			BetAmountText->SetText(FText::FromString(
				FString::Printf(TEXT("Bet: %d"), GameModeRef->GetBetAmount())));
		}
		UpdateBalance(GameModeRef->GetBalance());
	}
}
