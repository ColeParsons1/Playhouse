// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Economy/PHCDailyBonusWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"

#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "TimerManager.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCDailyBonusWidget::UPHCDailyBonusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCDailyBonusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ClaimButton)
	{
		ClaimButton->OnClicked.AddDynamic(this, &UPHCDailyBonusWidget::OnClaimClicked);
	}

	BuildStreakDots();
	LoadBonusInfo();

	// Start the countdown timer (updates every second).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CountdownTimerHandle,
			FTimerDelegate::CreateUObject(this, &UPHCDailyBonusWidget::UpdateCountdownTimer),
			1.0f,
			/*bLoop=*/ true
		);
	}
}

void UPHCDailyBonusWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------

void UPHCDailyBonusWidget::LoadBonusInfo()
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCDailyBonus: HttpClient unavailable."));
		return;
	}

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCDailyBonusWidget::OnBonusInfoApiResponse);
	Http->GetDailyBonusInfo(Callback);
}

void UPHCDailyBonusWidget::OnBonusInfoReceived(const FPHCDailyBonus& Bonus)
{
	CurrentBonus = Bonus;

	// Update streak count display.
	if (StreakCountText)
	{
		StreakCountText->SetText(FText::FromString(
			FString::Printf(TEXT("Day %d of %d"), Bonus.CurrentStreak, StreakCycleDays)));
	}

	// Update reward display.
	if (RewardText)
	{
		FString RewardStr;
		if (Bonus.CoinsReward > 0 && Bonus.GemsReward > 0)
		{
			RewardStr = FString::Printf(TEXT("%d Coins + %d Gems"), Bonus.CoinsReward, Bonus.GemsReward);
		}
		else if (Bonus.CoinsReward > 0)
		{
			RewardStr = FString::Printf(TEXT("%d Coins"), Bonus.CoinsReward);
		}
		else if (Bonus.GemsReward > 0)
		{
			RewardStr = FString::Printf(TEXT("%d Gems"), Bonus.GemsReward);
		}
		else
		{
			RewardStr = TEXT("No reward available");
		}
		RewardText->SetText(FText::FromString(RewardStr));
	}

	// Update info text.
	if (BonusInfoText)
	{
		if (Bonus.bClaimedToday)
		{
			BonusInfoText->SetText(FText::FromString(TEXT("Today's bonus already claimed!")));
		}
		else
		{
			BonusInfoText->SetText(FText::FromString(TEXT("Claim your daily bonus below.")));
		}
	}

	UpdateStreakVisual(Bonus.CurrentStreak);
	SetClaimable(!Bonus.bClaimedToday);
}

void UPHCDailyBonusWidget::OnClaimClicked()
{
	if (bClaimInFlight || CurrentBonus.bClaimedToday)
	{
		return;
	}

	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		return;
	}

	bClaimInFlight = true;
	SetClaimable(false);

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCDailyBonusWidget::OnClaimResult);
	Http->ClaimDailyBonus(Callback);
}

void UPHCDailyBonusWidget::UpdateStreakVisual(int32 Streak)
{
	// Highlight dots for completed days, dim for upcoming days.
	for (int32 i = 0; i < DotWidgets.Num(); ++i)
	{
		UBorder* DotBorder = Cast<UBorder>(DotWidgets[i]);
		if (!DotBorder)
		{
			continue;
		}

		if (i < Streak)
		{
			// Completed day - gold.
			DotBorder->SetBrushColor(FLinearColor(1.0f, 0.84f, 0.0f, 1.0f));
		}
		else if (i == Streak && !CurrentBonus.bClaimedToday)
		{
			// Current day, claimable - pulsing bright accent.
			DotBorder->SetBrushColor(FLinearColor(0.3f, 0.8f, 1.0f, 1.0f));
		}
		else
		{
			// Future day - dimmed.
			DotBorder->SetBrushColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
		}
	}
}

void UPHCDailyBonusWidget::SetClaimable(bool bCanClaim)
{
	if (ClaimButton)
	{
		ClaimButton->SetIsEnabled(bCanClaim);

		// Visual feedback.
		const FLinearColor BtnColor = bCanClaim
			? FLinearColor(0.2f, 0.8f, 0.2f, 1.0f)   // Green = claimable
			: FLinearColor(0.3f, 0.3f, 0.3f, 0.5f);   // Grey = not available
		ClaimButton->SetColorAndOpacity(BtnColor);
	}
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UPHCDailyBonusWidget::BuildStreakDots()
{
	if (!StreakDots)
	{
		return;
	}

	StreakDots->ClearChildren();
	DotWidgets.Empty(StreakCycleDays);

	for (int32 i = 0; i < StreakCycleDays; ++i)
	{
		UBorder* Dot = NewObject<UBorder>(this);
		if (!Dot)
		{
			continue;
		}

		Dot->SetBrushColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
		Dot->SetPadding(FMargin(2.f));

		// Add a day-number label inside the dot.
		UTextBlock* DayLabel = NewObject<UTextBlock>(Dot);
		if (DayLabel)
		{
			DayLabel->SetText(FText::AsNumber(i + 1));
			FSlateFontInfo FontInfo = DayLabel->GetFont();
			FontInfo.Size = 10;
			DayLabel->SetFont(FontInfo);
			DayLabel->SetJustification(ETextJustify::Center);
			Dot->AddChild(DayLabel);
		}

		UHorizontalBoxSlot* Slot = StreakDots->AddChildToHorizontalBox(Dot);
		if (Slot)
		{
			Slot->SetPadding(FMargin(4.f, 0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		DotWidgets.Add(Dot);
	}
}

void UPHCDailyBonusWidget::UpdateCountdownTimer()
{
	if (CurrentBonus.bClaimedToday && NextBonusTime > FDateTime::MinValue())
	{
		const FTimespan Remaining = NextBonusTime - FDateTime::UtcNow();

		if (Remaining.GetTotalSeconds() <= 0)
		{
			if (NextBonusTimerText)
			{
				NextBonusTimerText->SetText(FText::FromString(TEXT("Bonus available now!")));
			}

			// Re-fetch bonus info to update claimable state.
			CurrentBonus.bClaimedToday = false;
			SetClaimable(true);

			// Stop the timer until next claim.
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(CountdownTimerHandle);
			}
			return;
		}

		const int32 Hours = static_cast<int32>(Remaining.GetTotalHours());
		const int32 Minutes = Remaining.GetMinutes();
		const int32 Seconds = Remaining.GetSeconds();

		if (NextBonusTimerText)
		{
			NextBonusTimerText->SetText(FText::FromString(
				FString::Printf(TEXT("Next bonus in: %02d:%02d:%02d"), Hours, Minutes, Seconds)));
		}
	}
	else if (!CurrentBonus.bClaimedToday)
	{
		if (NextBonusTimerText)
		{
			NextBonusTimerText->SetText(FText::FromString(TEXT("Claim your bonus now!")));
		}
	}
}

void UPHCDailyBonusWidget::OnBonusInfoApiResponse(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCDailyBonus: Failed to load bonus info. Status=%d"), StatusCode);
		if (BonusInfoText)
		{
			BonusInfoText->SetText(FText::FromString(TEXT("Failed to load daily bonus.")));
		}
		return;
	}

	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCDailyBonus: Failed to parse bonus JSON."));
		return;
	}

	FPHCDailyBonus Bonus;
	Bonus.CurrentStreak = static_cast<int32>(JsonObj->GetNumberField(TEXT("current_streak")));
	Bonus.CoinsReward = static_cast<int32>(JsonObj->GetNumberField(TEXT("coins_reward")));
	Bonus.GemsReward = static_cast<int32>(JsonObj->GetNumberField(TEXT("gems_reward")));
	Bonus.bClaimedToday = JsonObj->GetBoolField(TEXT("claimed_today"));

	// Parse next bonus time for countdown.
	FString NextBonusStr;
	if (JsonObj->TryGetStringField(TEXT("next_bonus_time"), NextBonusStr) && !NextBonusStr.IsEmpty())
	{
		FDateTime::ParseIso8601(*NextBonusStr, NextBonusTime);
	}
	else
	{
		// Default: next midnight UTC.
		const FDateTime Now = FDateTime::UtcNow();
		NextBonusTime = FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay()) + FTimespan(1, 0, 0, 0);
	}

	OnBonusInfoReceived(Bonus);
}

void UPHCDailyBonusWidget::OnClaimResult(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	bClaimInFlight = false;

	if (bSuccess)
	{
		TSharedPtr<FJsonObject> JsonObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			const int32 CoinsAwarded = static_cast<int32>(JsonObj->GetNumberField(TEXT("coins_awarded")));
			const int32 GemsAwarded = static_cast<int32>(JsonObj->GetNumberField(TEXT("gems_awarded")));
			const int32 NewStreak = static_cast<int32>(JsonObj->GetNumberField(TEXT("new_streak")));

			// Update the GameInstance balance.
			UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
			if (GI)
			{
				if (JsonObj->HasField(TEXT("total_coins")))
				{
					GI->LocalPlayerProfile.Coins = static_cast<int32>(JsonObj->GetNumberField(TEXT("total_coins")));
				}
				if (JsonObj->HasField(TEXT("total_gems")))
				{
					GI->LocalPlayerProfile.Gems = static_cast<int32>(JsonObj->GetNumberField(TEXT("total_gems")));
				}
			}

			// Update local state.
			CurrentBonus.bClaimedToday = true;
			CurrentBonus.CurrentStreak = NewStreak;
			UpdateStreakVisual(NewStreak);
			SetClaimable(false);

			if (RewardText)
			{
				FString RewardStr;
				if (CoinsAwarded > 0 && GemsAwarded > 0)
				{
					RewardStr = FString::Printf(TEXT("Claimed: %d Coins + %d Gems!"), CoinsAwarded, GemsAwarded);
				}
				else if (CoinsAwarded > 0)
				{
					RewardStr = FString::Printf(TEXT("Claimed: %d Coins!"), CoinsAwarded);
				}
				else
				{
					RewardStr = FString::Printf(TEXT("Claimed: %d Gems!"), GemsAwarded);
				}
				RewardText->SetText(FText::FromString(RewardStr));
			}

			if (BonusInfoText)
			{
				BonusInfoText->SetText(FText::FromString(TEXT("Come back tomorrow for more!")));
			}

			OnBonusClaimed.Broadcast(CoinsAwarded, GemsAwarded);

			UE_LOG(LogTemp, Log, TEXT("PHCDailyBonus: Claimed %d coins, %d gems. Streak: %d"),
				CoinsAwarded, GemsAwarded, NewStreak);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCDailyBonus: Claim failed. Status=%d"), StatusCode);
		SetClaimable(!CurrentBonus.bClaimedToday);

		if (BonusInfoText)
		{
			BonusInfoText->SetText(FText::FromString(TEXT("Claim failed. Please try again.")));
		}
	}
}

UPHCHttpClient* UPHCDailyBonusWidget::GetHttpClient() const
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	return GI ? GI->GetHttpClient() : nullptr;
}
