// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Games/ColorCards/ColorCardsTypes.h"
#include "CardHandWidget.generated.h"

class UScrollBox;
class UButton;
class UImage;
class UTextBlock;
class UCanvasPanel;
class AColorCardsGameMode;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardSelected, int32, HandIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWildColorSelected, EPHCCardColor, Color);

// ---------------------------------------------------------------------------
// UCardHandWidget
// ---------------------------------------------------------------------------

/**
 * UMG widget displaying a player's hand of Color Cards as a scrollable
 * row. Shows the discard pile, draw pile, current-colour indicator,
 * direction arrow, and a colour-picker overlay for Wild cards.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UCardHandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// -- Public API -------------------------------------------------------

	/** Rebuild the hand display from the given cards. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void SetHand(const TArray<FPHCUnoCard>& Cards, bool bInteractable);

	/** Visually highlight cards that are currently playable. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void HighlightPlayableCards(EPHCCardColor InCurrentColor, EPHCCardValue InCurrentValue);

	/** Show the 4-colour picker overlay (for Wild card selection). */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void ShowWildColorPicker();

	/** Hide the colour picker. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void HideWildColorPicker();

	/** Update the discard pile visual. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void UpdateDiscardPile(const FPHCUnoCard& TopCard);

	/** Inject game-mode reference. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void SetGameMode(AColorCardsGameMode* InGameMode);

	/** Update the player-turn indicator text. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void UpdateTurnIndicator(int32 CurrentPlayer);

	/** Update the direction arrow. */
	UFUNCTION(BlueprintCallable, Category = "PHC|ColorCards")
	void UpdateDirection(bool bIsClockwise);

	// -- Helpers ----------------------------------------------------------

	/** Map an EPHCCardColor to an FLinearColor for rendering. */
	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	static FLinearColor GetCardColor(EPHCCardColor InColor);

	/** Build a short display string for a card (e.g. "R 7", "W +4"). */
	UFUNCTION(BlueprintPure, Category = "PHC|ColorCards")
	static FString GetCardDisplayText(const FPHCUnoCard& Card);

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|ColorCards")
	FOnCardSelected OnCardSelected;

	UPROPERTY(BlueprintAssignable, Category = "PHC|ColorCards")
	FOnWildColorSelected OnWildColorSelected;

protected:
	// -- Bind widgets -----------------------------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> CardContainer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DrawPileButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> DiscardPileImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CurrentColorIndicator;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DirectionIndicator;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerTurnIndicator;

	// -- Wild colour picker overlay ---------------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> WildColorPickerOverlay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WildRedButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WildBlueButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WildGreenButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WildYellowButton;

	// -- Internal ---------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<AColorCardsGameMode> GameModeRef;

	/** Buttons created for each card in hand. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CardButtons;

	UFUNCTION()
	void OnDrawPileClicked();

	UFUNCTION()
	void HandleAnyCardClicked();

	UFUNCTION()
	void OnWildRedClicked();

	UFUNCTION()
	void OnWildBlueClicked();

	UFUNCTION()
	void OnWildGreenClicked();

	UFUNCTION()
	void OnWildYellowClicked();

	/** Current hand cache for identification in HandleAnyCardClicked. */
	TArray<FPHCUnoCard> CachedHand;
};
