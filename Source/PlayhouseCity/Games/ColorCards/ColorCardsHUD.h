// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "Games/ColorCards/ColorCardsTypes.h"
#include "ColorCardsHUD.generated.h"

class AColorCardsGameMode;
class UBorder;
class UButton;
class UTextBlock;
class UScrollBox;
class UCanvasPanel;
class USizeBox;

/**
 * HUD for the Color Cards (Uno-style) mini-game.
 *
 * Displays the player's hand as a scrollable row of card buttons,
 * the discard pile with current colour, CPU hand counts, a draw
 * button, turn status, and a colour chooser overlay for Wild cards.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UColorCardsHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<AColorCardsGameMode> ColorCardsMode;

	// -- CPU hand indicator (top) -----------------------------------------

	UPROPERTY()
	TObjectPtr<UTextBlock> CPUHandText;

	// -- Discard pile (centre) --------------------------------------------

	UPROPERTY()
	TObjectPtr<UBorder> DiscardBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> DiscardValueText;

	// -- Current active colour indicator ----------------------------------

	UPROPERTY()
	TObjectPtr<UBorder> CurrentColorBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> CurrentColorLabel;

	// -- Turn status text -------------------------------------------------

	UPROPERTY()
	TObjectPtr<UTextBlock> TurnStatusText;

	// -- Player hand (bottom, scrollable row) -----------------------------

	UPROPERTY()
	TObjectPtr<UScrollBox> HandScrollBox;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> CardButtons;

	// -- Draw card button -------------------------------------------------

	UPROPERTY()
	TObjectPtr<UButton> DrawCardButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> DrawCardLabel;

	// -- Wild colour chooser overlay --------------------------------------

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ColorChooserOverlay;

	UPROPERTY()
	TObjectPtr<UButton> WildRedButton;

	UPROPERTY()
	TObjectPtr<UButton> WildBlueButton;

	UPROPERTY()
	TObjectPtr<UButton> WildGreenButton;

	UPROPERTY()
	TObjectPtr<UButton> WildYellowButton;

private:
	// -- Refresh helpers --------------------------------------------------

	void RefreshHand();
	void RefreshDiscardPile();
	void RefreshCPUHandCounts();
	void RefreshTurnStatus();
	void SetHandInteractable(bool bEnabled);

	// -- Static helpers ---------------------------------------------------

	static FLinearColor CardColorToLinear(EPHCCardColor InColor);
	static FString CardValueToString(const FPHCUnoCard& Card);

	// -- Game mode delegate handlers --------------------------------------

	UFUNCTION()
	void HandleCardPlayed(int32 PlayerIndex, FPHCUnoCard Card);

	UFUNCTION()
	void HandleCardDrawn(int32 PlayerIndex);

	UFUNCTION()
	void HandleTurnChanged(int32 NewPlayerIndex);

	UFUNCTION()
	void HandleColorChosen(EPHCCardColor ChosenColor);

	UFUNCTION()
	void HandleWinner(int32 WinnerIndex);

	// -- Button callbacks -------------------------------------------------

	UFUNCTION()
	void OnDrawCardClicked();

	UFUNCTION()
	void OnWildRedClicked();

	UFUNCTION()
	void OnWildBlueClicked();

	UFUNCTION()
	void OnWildGreenClicked();

	UFUNCTION()
	void OnWildYellowClicked();

	// Card-button click helpers (we store the hand index in a member to
	// identify which card was clicked, mirroring the CardHandWidget pattern).
	UFUNCTION()
	void HandleAnyCardClicked();

	/** Cached copy of the player's hand for card-click identification. */
	TArray<FPHCUnoCard> CachedHand;

	static constexpr int32 NUM_PLAYERS = 4;
};
