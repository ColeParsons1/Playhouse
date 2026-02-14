// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "GuessSignGameMode.generated.h"

class USignDatabase;

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** A single ASL sign entry with name, image path, and category. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCSignEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GuessSign")
	FString SignName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GuessSign")
	FString ImagePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GuessSign")
	FString Category; // "Alphabet", "Numbers", "CommonWords"
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSignQuestionPresented, const FString&, SignImagePath, const TArray<FString>&, Choices);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSignAnswerSubmitted, bool, bCorrect, const FString&, CorrectAnswer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSignTimeUp);

// ---------------------------------------------------------------------------
// AGuessSignGameMode
// ---------------------------------------------------------------------------

/**
 * "Guess the Sign" mini-game: the player is shown an ASL sign image and
 * must choose the correct meaning from 4 options within a time limit.
 *
 * Covers the ASL alphabet (A-Z), numbers (0-9), and 30+ common words.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AGuessSignGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AGuessSignGameMode();

	// -- Lifecycle --------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void StartGame() override;

	// -- Core gameplay ----------------------------------------------------

	/** Present the next question: pick a sign and generate 4 choices. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	void PresentQuestion();

	/** Submit the player's answer (0-3 choice index). Returns true if correct. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	bool SubmitAnswer(int32 ChoiceIndex);

	/** Get the current 4 answer choices. */
	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	TArray<FString> GetCurrentChoices() const { return CurrentChoices; }

	/** Get the current sign image path. */
	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	FString GetCurrentSignImagePath() const;

	/** Called when question time expires. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	void OnTimeUp();

	/** Advance to the next question or end the game. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	void NextQuestion();

	/** Get the current accuracy (0.0 - 1.0). */
	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	float GetAccuracy() const;

	// -- Accessors --------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	int32 GetCorrectAnswers() const { return CorrectAnswers; }

	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	int32 GetTotalQuestions() const { return TotalQuestions; }

	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	int32 GetMaxQuestions() const { return MaxQuestions; }

	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	int32 GetCurrentStreak() const { return CurrentStreak; }

	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	float GetTimePerQuestion() const { return TimePerQuestion; }

	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	float GetQuestionTimeRemaining() const;

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|GuessSign")
	FOnSignQuestionPresented OnQuestionPresented;

	UPROPERTY(BlueprintAssignable, Category = "PHC|GuessSign")
	FOnSignAnswerSubmitted OnAnswerSubmitted;

	UPROPERTY(BlueprintAssignable, Category = "PHC|GuessSign")
	FOnSignTimeUp OnSignTimeUp;

protected:
	// -- Configuration ----------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 MaxQuestions = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|GuessSign")
	float TimePerQuestion = 10.f;

	/** Points awarded for a correct answer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 CorrectPoints = 50;

	/** Bonus points per streak level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 StreakBonus = 10;

	// -- State ------------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<USignDatabase> SignDB;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	TArray<FPHCSignEntry> ShuffledSigns;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 CurrentSignIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 CorrectAnswers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 TotalQuestions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 CurrentStreak = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 BestStreak = 0;

	/** The 4 answer choices for the current question. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	TArray<FString> CurrentChoices;

	/** Index of the correct choice (0-3). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	int32 CorrectChoiceIndex = 0;

	/** True while waiting for player input on current question. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GuessSign")
	bool bQuestionActive = false;

	/** Per-question countdown. */
	FTimerHandle QuestionTimer;

	/** Time when the current question was presented. */
	float QuestionStartTime = 0.f;

	// -- Internal ---------------------------------------------------------

	/** Load and shuffle the sign database. */
	void LoadSignDatabase();
};
