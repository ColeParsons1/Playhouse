// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCMiniGameHUD.generated.h"

class APHCMiniGameMode;
class UTextBlock;
class UButton;
class UCanvasPanel;

// ---------------------------------------------------------------------------
// UPHCMiniGameHUD
// ---------------------------------------------------------------------------

/**
 * Base UMG widget used as the in-game HUD for every mini-game.
 *
 * Provides:
 *  - Live score and timer readout
 *  - Pause / resume overlay
 *  - Quit-with-confirmation flow
 *  - Game-over overlay showing final results, coins earned, and
 *    play-again / return-to-lobby buttons
 *
 * Subclasses add game-specific widgets and override NativeConstruct()
 * while calling Super to retain base wiring.
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCMiniGameHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	// -- Widget lifecycle --------------------------------------------------

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Public API -------------------------------------------------------

	/** Inject the game-mode reference so the HUD can drive game flow. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGameHUD")
	void SetGameMode(APHCMiniGameMode* InGameMode);

	/** Update the displayed score value. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGameHUD")
	void UpdateScore(int32 Score);

	/** Update the displayed timer (seconds remaining). */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGameHUD")
	void UpdateTimer(float SecondsRemaining);

	/** Present the game-over overlay with the given result data. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGameHUD")
	void ShowGameOver(const FPHCGameResult& Result);

protected:
	// -- Bind-widgets (matched by name in the UMG designer) ---------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GameNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PauseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	// -- Pause overlay ----------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> PauseOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitConfirmButton;

	// -- Game-over overlay ------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameOverOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FinalScoreText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinsEarnedText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayAgainButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReturnButton;

	// -- Internal state ---------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<APHCMiniGameMode> GameModeRef;

	// -- Button callbacks -------------------------------------------------

	UFUNCTION()
	void OnPauseClicked();

	UFUNCTION()
	void OnResumeClicked();

	UFUNCTION()
	void OnQuitClicked();

	UFUNCTION()
	void OnQuitConfirmed();

	UFUNCTION()
	void OnPlayAgainClicked();

	UFUNCTION()
	void OnReturnClicked();

	// -- Delegate handler -------------------------------------------------

	/** Bound to APHCMiniGameMode::OnGameEnded so the HUD reacts automatically. */
	UFUNCTION()
	void HandleGameEnded(const FPHCGameResult& Result);
};
