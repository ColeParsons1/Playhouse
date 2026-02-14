// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "Games/KitchenChaos/KitchenGameMode.h"
#include "KitchenHUD.generated.h"

class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UButton;
class UVerticalBox;
class AKitchenGameMode;

// ---------------------------------------------------------------------------
// UKitchenHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the Kitchen Chaos cooking mini-game.
 *
 * Displays active orders with countdown timers, an ingredient palette,
 * cooking station buttons, the current plate contents, and the held item.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UKitchenHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	// -- Widget lifecycle -----------------------------------------------------

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Visual updates -------------------------------------------------------

	/** Refresh the order cards display from the active order list. */
	UFUNCTION(BlueprintCallable, Category = "PHC|KitchenHUD")
	void UpdateOrders(const TArray<FPHCOrder>& Orders);

	/** Update the display of what the player is holding. */
	UFUNCTION(BlueprintCallable, Category = "PHC|KitchenHUD")
	void UpdateHeldItem(const FPHCHeldItem& Item);

	/** Update the plate contents display. */
	UFUNCTION(BlueprintCallable, Category = "PHC|KitchenHUD")
	void UpdatePlate(const TArray<FPHCHeldItem>& Plate);

	/** Play a celebration animation when an order is completed. */
	UFUNCTION(BlueprintCallable, Category = "PHC|KitchenHUD")
	void ShowOrderComplete(int32 Score);

	/** Play a warning animation when an order expires. */
	UFUNCTION(BlueprintCallable, Category = "PHC|KitchenHUD")
	void ShowOrderFailed();

	// -- Utility --------------------------------------------------------------

	/** Get a visual brush/color representing an ingredient. */
	UFUNCTION(BlueprintPure, Category = "PHC|KitchenHUD")
	FLinearColor GetIngredientColor(EPHCIngredient Ingredient) const;

	/** Get a visual brush/color representing a cooking station. */
	UFUNCTION(BlueprintPure, Category = "PHC|KitchenHUD")
	FLinearColor GetStationColor(EPHCCookingStation Station) const;

protected:
	// -- Bind widgets ---------------------------------------------------------

	/** Canvas wrapping all kitchen-specific content below the top bar. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> KitchenCanvas;

	/** Horizontal bar showing active order cards. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> OrdersBar;

	// -- Ingredient buttons ---------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BunButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PattyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LettuceButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TomatoButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CheeseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> OnionButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FishButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RiceButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NoriButton;

	// -- Station buttons ------------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GrillButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ChopButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ServeButton;

	// -- Plate and held item --------------------------------------------------

	/** Shows current plate contents as ingredient images. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PlateDisplay;

	/** Shows the currently held item. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeldItemImage;

	/** Text label for the held item. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeldItemText;

	/** Button to submit the current plate as an order. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SubmitButton;

	/** Button to clear the plate. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ClearPlateButton;

	/** Text for order completion feedback. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FeedbackText;

	// -- Dynamic order card visuals -------------------------------------------

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVerticalBox>> OrderCardWidgets;

	/** Order-slot border widgets (up to 3 visible at once). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> OrderSlotWidgets;

	/** Per-order timer progress bars. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> OrderTimerBars;

	/** Combo / streak text shown when completing multiple orders. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ComboText;

	/** Current-dish display text (what's on the plate). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CurrentDishText;

private:
	// -- Ingredient button callbacks ------------------------------------------

	UFUNCTION() void OnBunClicked();
	UFUNCTION() void OnPattyClicked();
	UFUNCTION() void OnLettuceClicked();
	UFUNCTION() void OnTomatoClicked();
	UFUNCTION() void OnCheeseClicked();
	UFUNCTION() void OnOnionClicked();
	UFUNCTION() void OnFishClicked();
	UFUNCTION() void OnRiceClicked();
	UFUNCTION() void OnNoriClicked();

	// -- Station button callbacks ---------------------------------------------

	UFUNCTION() void OnGrillClicked();
	UFUNCTION() void OnChopClicked();
	UFUNCTION() void OnFryClicked();
	UFUNCTION() void OnServeClicked();

	// -- Action button callbacks ----------------------------------------------

	UFUNCTION() void OnSubmitClicked();
	UFUNCTION() void OnClearClicked();

	// -- Delegate handlers ----------------------------------------------------

	UFUNCTION() void HandleOrderCompleted(int32 ScoreEarned);
	UFUNCTION() void HandleOrderFailed();

	// -- Helpers --------------------------------------------------------------

	void OnIngredientClicked(EPHCIngredient Ingredient);
	void OnStationClicked(EPHCCookingStation Station);

	/** Cached reference to the kitchen game mode. */
	UPROPERTY(Transient)
	TObjectPtr<AKitchenGameMode> KitchenMode;

	/** Timer for clearing feedback text. */
	FTimerHandle FeedbackTimerHandle;
};
