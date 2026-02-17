// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "GuessSignHUD.generated.h"

class UImage;
class UBorder;
class UButton;
class UTextBlock;
class UProgressBar;
class USizeBox;
class UVerticalBox;
class AGuessSignGameMode;

// ---------------------------------------------------------------------------
// UGuessSignHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the "Guess the Sign" (ASL) mini-game.
 *
 * Displays the sign image, four answer choice buttons, a countdown timer bar,
 * progress indicator, streak counter, and feedback overlay.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UGuessSignHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Bind to the GuessSign-specific game mode. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	void SetGuessSignGameMode(AGuessSignGameMode* InGameMode);

protected:
	// -- Bind widgets -----------------------------------------------------

	/** The displayed ASL sign image. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SignImage;

	/** Large border serving as the sign display area background. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SignDisplayBorder;

	/** Large letter text shown inside the sign display area. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SignLetterText;

	/** Description text below the letter (e.g. "ASL Sign for..."). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SignDescriptionText;

	/** Four answer choice buttons. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Choice1Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Choice2Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Choice3Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Choice4Button;

	/** Text labels for each choice button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Choice1Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Choice2Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Choice3Text;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Choice4Text;

	/** Border wrappers for each choice button (for color feedback). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Choice1Border;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Choice2Border;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Choice3Border;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Choice4Border;

	/** Progress / round counter display ("5/20"). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	/** Countdown timer bar for the current question. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TimerBar;

	/** Current answer streak display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StreakText;

	/** Feedback overlay text ("Correct!" / "Wrong!"). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FeedbackText;

	/** "Next" button that appears after answering. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NextButton;

	/** Text label inside the Next button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NextButtonText;

private:
	UPROPERTY(Transient)
	TObjectPtr<AGuessSignGameMode> GuessSignModeRef;

	/** Whether we are currently showing feedback (disables choice buttons). */
	bool bShowingFeedback = false;

	// -- Delegate handlers ------------------------------------------------

	UFUNCTION()
	void HandleQuestionPresented(const FString& SignImagePath, const TArray<FString>& Choices);

	UFUNCTION()
	void HandleAnswerSubmitted(bool bCorrect, const FString& CorrectAnswer);

	UFUNCTION()
	void HandleTimeUp();

	// -- Button callbacks -------------------------------------------------

	UFUNCTION()
	void OnChoice1Clicked();

	UFUNCTION()
	void OnChoice2Clicked();

	UFUNCTION()
	void OnChoice3Clicked();

	UFUNCTION()
	void OnChoice4Clicked();

	UFUNCTION()
	void OnNextClicked();

	/** Submit the answer for the given choice index. */
	void SubmitChoice(int32 Index);

	/** Enable or disable the 4 choice buttons. */
	void SetChoiceButtonsEnabled(bool bEnabled);

	/** Reset all choice button border colors to default. */
	void ResetChoiceBorderColors();

	/** Update the progress text and streak display. */
	void RefreshStatusDisplay();
};
