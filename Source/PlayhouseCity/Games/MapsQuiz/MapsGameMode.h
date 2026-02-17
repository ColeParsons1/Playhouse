// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "MapsGameMode.generated.h"

class UCountryDatabase;

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** A single country entry for the Maps Quiz. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCCountryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|MapsQuiz")
	FString CountryName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|MapsQuiz")
	FString Capital;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|MapsQuiz")
	FString Continent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|MapsQuiz")
	FString FlagEmoji;

	/** Path to the country outline / silhouette image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|MapsQuiz")
	FString OutlinePath;
};

/** Describes the type of question being asked. */
UENUM(BlueprintType)
enum class EPHCMapsQuestionType : uint8
{
	IdentifyCountry		UMETA(DisplayName = "Identify Country from Outline"),
	CapitalFromCountry	UMETA(DisplayName = "Capital from Country"),
	CountryFromFlag		UMETA(DisplayName = "Country from Flag")
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnMapsQuestionPresented,
	const FString&, QuestionText,
	const FString&, ImagePath,
	EPHCMapsQuestionType, QuestionType,
	const TArray<FString>&, Choices);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnMapsAnswerSubmitted,
	bool, bCorrect,
	const FString&, CorrectAnswer);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMapsTimeUp);

// ---------------------------------------------------------------------------
// AMapsGameMode
// ---------------------------------------------------------------------------

/**
 * Maps Quiz mini-game: test geography knowledge with questions about
 * country outlines, capitals, and flags.
 *
 * Three question types cycle randomly:
 *  1. Identify a country from its outline silhouette.
 *  2. Choose the capital city of a named country.
 *  3. Identify a country from its flag.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AMapsGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AMapsGameMode();

	// -- Lifecycle --------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void StartGame() override;

	// -- Core gameplay ----------------------------------------------------

	/** Present the next question. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	void PresentQuestion();

	/** Submit the player's answer (0-3). Returns true if correct. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	bool SubmitAnswer(int32 ChoiceIndex);

	/** Generate 4 answer choices given the correct answer and question type. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	TArray<FString> GenerateChoices(const FString& Correct, EPHCMapsQuestionType QuestionType);

	/** Advance to the next question. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	void NextQuestion();

	/** Called when per-question time expires. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	void OnTimeUp();

	// -- Accessors --------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|MapsQuiz")
	int32 GetCorrectAnswers() const { return CorrectAnswers; }

	UFUNCTION(BlueprintPure, Category = "PHC|MapsQuiz")
	int32 GetCurrentQuestionNumber() const { return CurrentQuestionIndex + 1; }

	UFUNCTION(BlueprintPure, Category = "PHC|MapsQuiz")
	int32 GetMaxQuestions() const { return MaxQuestions; }

	UFUNCTION(BlueprintPure, Category = "PHC|MapsQuiz")
	float GetAccuracy() const;

	UFUNCTION(BlueprintPure, Category = "PHC|MapsQuiz")
	float GetQuestionTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "PHC|MapsQuiz")
	EPHCMapsQuestionType GetCurrentQuestionType() const { return CurrentQuestionType; }

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|MapsQuiz")
	FOnMapsQuestionPresented OnQuestionPresented;

	UPROPERTY(BlueprintAssignable, Category = "PHC|MapsQuiz")
	FOnMapsAnswerSubmitted OnAnswerSubmitted;

	UPROPERTY(BlueprintAssignable, Category = "PHC|MapsQuiz")
	FOnMapsTimeUp OnMapsTimeUp;

protected:
	// -- Configuration ----------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|MapsQuiz")
	int32 MaxQuestions = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|MapsQuiz")
	float TimePerQuestion = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|MapsQuiz")
	int32 CorrectPoints = 50;

	// -- State ------------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<UCountryDatabase> CountryDB;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|MapsQuiz")
	TArray<FPHCCountryEntry> ShuffledCountries;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|MapsQuiz")
	int32 CurrentQuestionIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|MapsQuiz")
	int32 CorrectAnswers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|MapsQuiz")
	EPHCMapsQuestionType CurrentQuestionType = EPHCMapsQuestionType::IdentifyCountry;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|MapsQuiz")
	TArray<FString> CurrentChoices;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|MapsQuiz")
	int32 CorrectChoiceIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|MapsQuiz")
	bool bQuestionActive = false;

	FTimerHandle QuestionTimer;
	float QuestionStartTime = 0.f;

	// -- Internal ---------------------------------------------------------

	void LoadCountryDatabase();
};
