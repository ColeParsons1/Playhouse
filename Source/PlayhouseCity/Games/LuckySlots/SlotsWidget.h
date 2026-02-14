// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Games/LuckySlots/SlotsGameMode.h"
#include "SlotsWidget.generated.h"

class UImage;
class UButton;
class UTextBlock;

// ---------------------------------------------------------------------------
// USlotsWidget
// ---------------------------------------------------------------------------

/**
 * UMG widget for the Lucky Slots mini-game.
 *
 * Displays three slot reels, spin / bet controls, balance readout,
 * and a win animation overlay. Each reel is represented by a UImage
 * whose brush / tint changes to depict the current symbol.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API USlotsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Public API -------------------------------------------------------

	/** Inject the game-mode reference. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Slots")
	void SetGameMode(ASlotsGameMode* InGameMode);

	/** Animate a win display for the given amount. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Slots")
	void ShowWin(int32 Amount);

	/** Update the balance text from an external source. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Slots")
	void UpdateBalance(int32 Coins);

	/** Map a symbol enum to a tint colour (placeholder for real art). */
	UFUNCTION(BlueprintPure, Category = "PHC|Slots")
	static FLinearColor GetSymbolColor(EPHCSlotSymbol Symbol);

	/** Map a symbol enum to a display name. */
	UFUNCTION(BlueprintPure, Category = "PHC|Slots")
	static FString GetSymbolName(EPHCSlotSymbol Symbol);

protected:
	// -- Bind widgets -----------------------------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Reel1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Reel2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Reel3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SpinButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BetUpButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BetDownButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BetAmountText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BalanceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WinText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PayTableButton;

	// -- Internal state ---------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<ASlotsGameMode> GameModeRef;

	/** True while the reel-spin animation is playing. */
	bool bAnimating = false;

	/** Per-reel animation progress (0..1). */
	float ReelAnimProgress[3] = { 0.f, 0.f, 0.f };

	/** Stagger offsets so each reel stops in sequence. */
	float ReelStopTime[3] = { 0.5f, 1.0f, 1.5f };

	/** Results to display once the animation finishes. */
	TArray<EPHCSlotSymbol> PendingResults;

	/** How many reels have finished animating. */
	int32 ReelsStopped = 0;

	/** Total animation clock. */
	float AnimClock = 0.f;

	// -- Callbacks --------------------------------------------------------

	UFUNCTION()
	void OnSpinClicked();

	UFUNCTION()
	void OnBetUpClicked();

	UFUNCTION()
	void OnBetDownClicked();

	UFUNCTION()
	void OnPayTableClicked();

	/** Delegate handler from the game mode. */
	UFUNCTION()
	void HandleSpinComplete(const TArray<EPHCSlotSymbol>& Results);

	UFUNCTION()
	void HandleWin(int32 PayoutAmount);

	// -- Helpers ----------------------------------------------------------

	/** Start the reel-spin animation sequence. */
	void StartReelAnimation(const TArray<EPHCSlotSymbol>& Results);

	/** Called when the last reel stops. */
	void OnAllReelsStopped();

	/** Set a reel image to display a specific symbol. */
	void SetReelSymbol(UImage* ReelImage, EPHCSlotSymbol Symbol);

	/** Refresh the bet and balance readouts. */
	void RefreshUI();
};
