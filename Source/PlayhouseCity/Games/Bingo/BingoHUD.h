// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "BingoHUD.generated.h"

class UBingoCard;
class UWrapBox;
class UButton;
class UTextBlock;
class ABingoGameMode;

// ---------------------------------------------------------------------------
// UBingoHUD
// ---------------------------------------------------------------------------

/**
 * HUD overlay for the Bingo mini-game.
 *
 * Extends the base mini-game HUD with a bingo card widget, a board
 * showing all called numbers, a "Bingo!" call button, and a readout
 * of how many CPU opponents are still in the game.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UBingoHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

	// -- Public API -------------------------------------------------------

	/** React to a new number being called by the game mode. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void OnNumberCalled(int32 Number);

	/** Update the label showing how many CPU opponents remain. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void UpdateCPUStatus(int32 CPUsRemaining);

protected:
	// -- Bind widgets -----------------------------------------------------

	/** The main bingo card widget (UBingoCard, placed in UMG). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBingoCard> BingoCardWidget;

	/** Wrap box (or similar) displaying all numbers called so far. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> CalledBoardWidget;

	/** Button the player presses to declare bingo. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BingoCallButton;

	/** Text showing the number of active CPU opponents. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CPUCountText;

	// -- Internal ---------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<ABingoGameMode> BingoModeRef;

	UFUNCTION()
	void OnBingoClicked();

	/** Bound to ABingoGameMode::OnNumberCalled. */
	UFUNCTION()
	void HandleNumberCalled(int32 Number);

	/** Bound to ABingoGameMode::OnBingoAchieved. */
	UFUNCTION()
	void HandleBingoAchieved(bool bIsPlayer, int32 WinnerIndex);
};
