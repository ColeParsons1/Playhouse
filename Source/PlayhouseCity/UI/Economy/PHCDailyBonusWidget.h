// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCDailyBonusWidget.generated.h"

class UTextBlock;
class UButton;
class UHorizontalBox;
class UPHCHttpClient;

/**
 * Delegate broadcast when a daily bonus is successfully claimed.
 * @param CoinsAwarded  Number of coins received.
 * @param GemsAwarded   Number of gems received.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDailyBonusClaimed, int32, CoinsAwarded, int32, GemsAwarded);

/**
 * UPHCDailyBonusWidget
 *
 * Displays the daily login bonus streak (7-day cycle) with visual dot
 * indicators. Allows the player to claim the current day's bonus if
 * eligible, and shows a countdown timer to the next bonus window.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCDailyBonusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCDailyBonusWidget(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// -- Public delegates ------------------------------------------------------

	/** Broadcast after a successful claim. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|DailyBonus")
	FOnDailyBonusClaimed OnBonusClaimed;

protected:
	// -- Bind Widgets ----------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StreakCountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> RewardText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ClaimButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> StreakDots;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> BonusInfoText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NextBonusTimerText;

	// -- Methods ---------------------------------------------------------------

	/** Fetch daily bonus information from the backend. */
	UFUNCTION(BlueprintCallable, Category = "PHC|DailyBonus")
	void LoadBonusInfo();

	/** Handle the bonus info response. */
	UFUNCTION(BlueprintCallable, Category = "PHC|DailyBonus")
	void OnBonusInfoReceived(const FPHCDailyBonus& Bonus);

	/** Attempt to claim the daily bonus. */
	UFUNCTION(BlueprintCallable, Category = "PHC|DailyBonus")
	void OnClaimClicked();

	/** Update the 7-day streak dot indicators. */
	UFUNCTION(BlueprintCallable, Category = "PHC|DailyBonus")
	void UpdateStreakVisual(int32 Streak);

	/** Enable or disable the claim button. */
	UFUNCTION(BlueprintCallable, Category = "PHC|DailyBonus")
	void SetClaimable(bool bCanClaim);

private:
	// -- Internal helpers ------------------------------------------------------

	/** Build the 7 streak dot widgets. */
	void BuildStreakDots();

	/** Timer tick for the countdown timer. */
	void UpdateCountdownTimer();

	/** Callback after GetDailyBonusInfo API returns. */
	UFUNCTION()
	void OnBonusInfoApiResponse(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Callback after ClaimDailyBonus API returns. */
	UFUNCTION()
	void OnClaimResult(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Convenience accessor for the HttpClient via GameInstance. */
	UPHCHttpClient* GetHttpClient() const;

	/** Current daily bonus state. */
	FPHCDailyBonus CurrentBonus;

	/** Whether the claim request is in-flight. */
	bool bClaimInFlight = false;

	/** Timer handle for the countdown to next bonus. */
	FTimerHandle CountdownTimerHandle;

	/** UTC time when the next bonus becomes available. */
	FDateTime NextBonusTime;

	/** Dot indicator widgets stored for direct access. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> DotWidgets;

	/** Number of days in one streak cycle. */
	static constexpr int32 StreakCycleDays = 7;
};
