// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PHCScratchCardWidget.generated.h"

class UImage;
class UProgressBar;
class UButton;
class UTextBlock;
class UTexture2D;
class UPHCHttpClient;

/**
 * Delegate broadcast when a scratch-card prize is claimed.
 * @param PrizeAmount  Number of coins awarded.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScratchCardClaimed, int32, PrizeAmount);

/**
 * UPHCScratchCardWidget
 *
 * A touch/mouse gesture scratch card. The player scratches off an opaque
 * mask overlay to reveal a prize image underneath. Once more than 60% of
 * the mask is cleared, the card auto-reveals and the player can claim the
 * prize.
 *
 * The scratch mask is a dynamically-generated UTexture2D whose alpha channel
 * is erased at the scratch position on each pointer move event.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCScratchCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCScratchCardWidget(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// -- Input overrides (scratch gesture) ------------------------------------
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	// -- Delegates -------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|ScratchCard")
	FOnScratchCardClaimed OnPrizeClaimed;

protected:
	// -- Bind Widgets ----------------------------------------------------------

	/** The opaque scratch overlay image. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> ScratchCanvas;

	/** The prize reveal image behind the canvas. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> RevealImage;

	/** Progress bar showing scratch completion percentage. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ScratchProgressBar;

	/** Button to claim the prize once revealed. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ClaimButton;

	/** Text displaying the prize amount. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> PrizeText;

	// -- Configuration ---------------------------------------------------------

	/** Width of the scratch mask texture in pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|ScratchCard")
	int32 MaskWidth = 256;

	/** Height of the scratch mask texture in pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|ScratchCard")
	int32 MaskHeight = 256;

	/** Radius of the scratch brush in pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|ScratchCard")
	int32 BrushRadius = 20;

	/** Percentage threshold (0.0 - 1.0) to auto-reveal. Default 0.6 = 60%. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|ScratchCard")
	float RevealThreshold = 0.6f;

	// -- Methods ---------------------------------------------------------------

	/** Claim the revealed prize and add to balance. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ScratchCard")
	void OnClaimClicked();

	/** Reset the scratch card to a fresh state. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ScratchCard")
	void ResetCard();

private:
	// -- Scratch mask operations -----------------------------------------------

	/** Initialize the scratch mask texture (fully opaque). */
	void InitializeScratchMask();

	/** Erase mask pixels in a circle around the given local position. */
	void ScratchAtPosition(const FVector2D& LocalPosition);

	/** Recalculate the percentage of the mask that has been scratched. */
	void UpdateScratchPercentage();

	/** Check if the scratch threshold is reached and auto-reveal. */
	void CheckRevealThreshold();

	/** Perform full reveal (clear entire mask, show prize). */
	void AutoReveal();

	/** Upload the modified mask data to the GPU texture. */
	void UpdateMaskTexture();

	/** Fetch the scratch card prize from the backend. */
	void FetchPrize();

	/** Callback after the ScratchCard API returns. */
	UFUNCTION()
	void OnPrizeReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Callback after the ClaimScratchCard API returns. */
	UFUNCTION()
	void OnClaimResult(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Convenience accessor for the HttpClient via GameInstance. */
	UPHCHttpClient* GetHttpClient() const;

	// -- Data ------------------------------------------------------------------

	/** The dynamically modified scratch mask texture. */
	UPROPERTY()
	TObjectPtr<UTexture2D> ScratchMask;

	/** Raw pixel buffer for the mask (BGRA, one byte per channel). */
	TArray<uint8> MaskPixels;

	/** Percentage of mask pixels that have been cleared (0.0 - 1.0). */
	float ScratchPercentage = 0.0f;

	/** The prize amount for this scratch card session. */
	int32 PrizeAmount = 0;

	/** True once the prize has been fully revealed. */
	bool bRevealed = false;

	/** True while the user is actively scratching (mouse/touch down). */
	bool bIsScratching = false;

	/** True while a claim request is in-flight. */
	bool bClaimInFlight = false;

	/** Total number of pixels in the mask (for percentage calculation). */
	int32 TotalPixels = 0;

	/** Number of mask pixels that have been cleared. */
	int32 ClearedPixels = 0;
};
