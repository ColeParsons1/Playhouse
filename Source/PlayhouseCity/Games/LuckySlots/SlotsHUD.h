// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "Games/LuckySlots/SlotsGameMode.h"
#include "SlotsHUD.generated.h"

class ASlotsGameMode;
class UButton;
class UBorder;
class UTextBlock;
class UHorizontalBox;

/**
 * HUD for the Lucky Slots mini-game.
 *
 * Displays three slot reels (as colored border widgets with symbol text),
 * a large SPIN button, bet adjustment controls (+/-), balance readout,
 * and a win amount display. Dark purple/gold casino colour scheme.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API USlotsHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ASlotsGameMode> SlotsMode;

	// -- Title ------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	// -- Reel display (3 reels) -------------------------------------------

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> ReelBorders;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ReelLabels;

	// -- Controls ---------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UButton> SpinButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> SpinButtonLabel;

	UPROPERTY()
	TObjectPtr<UButton> BetUpButton;

	UPROPERTY()
	TObjectPtr<UButton> BetDownButton;

	// -- Info readouts ----------------------------------------------------

	UPROPERTY()
	TObjectPtr<UTextBlock> BetText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BalanceText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WinText;

private:
	// -- Delegate handlers ------------------------------------------------

	UFUNCTION()
	void HandleSpinComplete(const TArray<EPHCSlotSymbol>& Results);

	UFUNCTION()
	void HandleWin(int32 PayoutAmount);

	// -- Button callbacks -------------------------------------------------

	UFUNCTION()
	void OnSpinClicked();

	UFUNCTION()
	void OnBetUpClicked();

	UFUNCTION()
	void OnBetDownClicked();

	// -- Helpers ----------------------------------------------------------

	/** Refresh bet and balance text from the game mode. */
	void RefreshUI();

	/** Set a reel border and label to display the given symbol. */
	void SetReelDisplay(int32 ReelIndex, EPHCSlotSymbol Symbol);

	/** Map a symbol enum to a display emoji / short string. */
	static FString GetSymbolEmoji(EPHCSlotSymbol Symbol);

	// -- Animation state --------------------------------------------------

	bool bAnimating = false;
	float AnimClock = 0.f;
	int32 ReelsStopped = 0;
	float ReelStopTime[3] = { 0.5f, 1.0f, 1.5f };
	TArray<EPHCSlotSymbol> PendingResults;

	void StartReelAnimation(const TArray<EPHCSlotSymbol>& Results);
	void OnAllReelsStopped();

	static constexpr int32 NUM_REELS = 3;
};
