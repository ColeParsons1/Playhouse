// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "Games/MapsQuiz/MapsGameMode.h"
#include "MapsQuizHUD.generated.h"

class UImage;
class UBorder;
class UButton;
class UTextBlock;
class UProgressBar;
class UCanvasPanel;
class USizeBox;

// ---------------------------------------------------------------------------
// UMapsQuizHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the Maps Quiz mini-game.
 *
 * Displays a question, map outline or flag image, four answer buttons,
 * progress indicator, countdown timer bar, and feedback overlay.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UMapsQuizHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Bind to the MapsQuiz-specific game mode. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	void SetMapsGameMode(AMapsGameMode* InGameMode);

protected:
	// -- Bind widgets -----------------------------------------------------

	/** The question prompt text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuestionText;

	/** Dark-blue border that frames the country clue display area. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CountryDisplayBorder;

	/** Text shown inside the display area when the clue is text-based. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountryClueText;

	/** Image widget for country outline or flag display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MapImage;

	/** Secondary image for flag display (when both map and flag are needed). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> FlagImage;

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

	/** Progress display ("5/20"). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	/** Countdown timer bar for the current question. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TimerBar;

	/** Feedback overlay panel (Correct!/Wrong!). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> FeedbackOverlay;

	/** Feedback text within the overlay. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FeedbackText;

	/** Accuracy display text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AccuracyText;

	/** "Next" button shown after answering. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NextButton;

	/** Label inside the Next button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NextButtonText;

	/** Background border wrapping the timer bar. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TimerBarBorder;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMapsGameMode> MapsGameModeRef;

	bool bShowingFeedback = false;

	// -- Delegate handlers ------------------------------------------------

	UFUNCTION()
	void HandleQuestionPresented(const FString& InQuestionText, const FString& ImagePath,
								 EPHCMapsQuestionType QuestionType, const TArray<FString>& Choices);

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

	void SubmitChoice(int32 Index);
	void SetChoiceButtonsEnabled(bool bEnabled);
	void RefreshStatusDisplay();
};
