// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/MapsQuiz/MapsGameMode.h"
#include "Games/MapsQuiz/MapsQuizHUD.h"
#include "Games/MapsQuiz/CountryDatabase.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AMapsGameMode::AMapsGameMode()
{
	GameType = EPHCGameType::MapsQuiz;
	GameHUDClass = UMapsQuizHUD::StaticClass();
	MaxGameTime = 0.f; // Per-question timer, not global.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AMapsGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AMapsGameMode::StartGame()
{
	Super::StartGame();

	CorrectAnswers = 0;
	CurrentQuestionIndex = 0;
	bQuestionActive = false;
	CurrentChoices.Empty();

	LoadCountryDatabase();
	PresentQuestion();
}

// ---------------------------------------------------------------------------
// Core gameplay
// ---------------------------------------------------------------------------

void AMapsGameMode::PresentQuestion()
{
	if (!bGameActive || CurrentQuestionIndex >= MaxQuestions)
	{
		// End of quiz.
		int32 AccuracyBonus = FMath::RoundToInt32(GetAccuracy() * 300.f);
		AddScore(AccuracyBonus);
		EndGame(GetAccuracy() >= 0.5f);
		return;
	}

	if (CurrentQuestionIndex >= ShuffledCountries.Num())
	{
		// Reshuffle if we run out.
		for (int32 i = ShuffledCountries.Num() - 1; i > 0; --i)
		{
			int32 j = FMath::RandRange(0, i);
			ShuffledCountries.Swap(i, j);
		}
		CurrentQuestionIndex = 0;
	}

	const FPHCCountryEntry& Country = ShuffledCountries[CurrentQuestionIndex];

	// Pick a random question type.
	CurrentQuestionType = static_cast<EPHCMapsQuestionType>(FMath::RandRange(0, 2));

	FString QuestionText;
	FString CorrectAnswer;
	FString ImagePath;

	switch (CurrentQuestionType)
	{
	case EPHCMapsQuestionType::IdentifyCountry:
		QuestionText = TEXT("Which country is shown in this outline?");
		CorrectAnswer = Country.CountryName;
		ImagePath = Country.OutlinePath;
		break;

	case EPHCMapsQuestionType::CapitalFromCountry:
		QuestionText = FString::Printf(TEXT("What is the capital of %s?"), *Country.CountryName);
		CorrectAnswer = Country.Capital;
		ImagePath = FString(); // No image for capital questions (or use flag).
		break;

	case EPHCMapsQuestionType::CountryFromFlag:
		QuestionText = TEXT("Which country does this flag belong to?");
		CorrectAnswer = Country.CountryName;
		ImagePath = FString::Printf(TEXT("/Game/UI/Flags/%s"), *Country.CountryName);
		break;
	}

	CurrentChoices = GenerateChoices(CorrectAnswer, CurrentQuestionType);

	bQuestionActive = true;
	QuestionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Start per-question timer.
	GetWorldTimerManager().ClearTimer(QuestionTimer);
	GetWorldTimerManager().SetTimer(
		QuestionTimer, this, &AMapsGameMode::OnTimeUp, TimePerQuestion, false);

	OnQuestionPresented.Broadcast(QuestionText, ImagePath, CurrentQuestionType, CurrentChoices);
}

bool AMapsGameMode::SubmitAnswer(int32 ChoiceIndex)
{
	if (!bGameActive || !bQuestionActive)
	{
		return false;
	}

	bQuestionActive = false;
	GetWorldTimerManager().ClearTimer(QuestionTimer);

	bool bCorrect = (ChoiceIndex == CorrectChoiceIndex);

	if (bCorrect)
	{
		CorrectAnswers++;

		// Points with time bonus.
		float TimeElapsed = GetWorld()
			? (GetWorld()->GetTimeSeconds() - QuestionStartTime)
			: TimePerQuestion;
		float TimeFraction = FMath::Clamp(1.f - (TimeElapsed / TimePerQuestion), 0.f, 1.f);
		int32 TimeBonus = FMath::RoundToInt32(TimeFraction * 30.f);

		AddScore(CorrectPoints + TimeBonus);
	}

	FString CorrectAnswer = CurrentChoices.IsValidIndex(CorrectChoiceIndex)
		? CurrentChoices[CorrectChoiceIndex]
		: TEXT("");

	OnAnswerSubmitted.Broadcast(bCorrect, CorrectAnswer);

	return bCorrect;
}

TArray<FString> AMapsGameMode::GenerateChoices(const FString& Correct, EPHCMapsQuestionType QuestionType)
{
	TArray<FString> Choices;
	Choices.Add(Correct);

	if (!CountryDB)
	{
		// Fallback.
		Choices.Append({ TEXT("Option B"), TEXT("Option C"), TEXT("Option D") });
		CorrectChoiceIndex = 0;
		return Choices;
	}

	// Determine which field to use for distractors.
	FString Field;
	switch (QuestionType)
	{
	case EPHCMapsQuestionType::IdentifyCountry:
	case EPHCMapsQuestionType::CountryFromFlag:
		Field = TEXT("CountryName");
		break;
	case EPHCMapsQuestionType::CapitalFromCountry:
		Field = TEXT("Capital");
		break;
	}

	TArray<FString> Distractors = CountryDB->GetRandomChoices(Correct, Field, 3);
	Choices.Append(Distractors);

	// Shuffle choices.
	for (int32 i = Choices.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		Choices.Swap(i, j);
	}

	// Find where the correct answer ended up.
	CorrectChoiceIndex = Choices.IndexOfByKey(Correct);

	return Choices;
}

void AMapsGameMode::NextQuestion()
{
	CurrentQuestionIndex++;
	PresentQuestion();
}

void AMapsGameMode::OnTimeUp()
{
	if (!bGameActive || !bQuestionActive)
	{
		return;
	}

	bQuestionActive = false;

	FString CorrectAnswer = CurrentChoices.IsValidIndex(CorrectChoiceIndex)
		? CurrentChoices[CorrectChoiceIndex]
		: TEXT("");

	OnMapsTimeUp.Broadcast();
	OnAnswerSubmitted.Broadcast(false, CorrectAnswer);
}

float AMapsGameMode::GetAccuracy() const
{
	int32 TotalAnswered = CurrentQuestionIndex;
	if (TotalAnswered == 0)
	{
		return 0.f;
	}
	return static_cast<float>(CorrectAnswers) / static_cast<float>(TotalAnswered);
}

float AMapsGameMode::GetQuestionTimeRemaining() const
{
	if (!bQuestionActive || !GetWorld())
	{
		return 0.f;
	}
	float Elapsed = GetWorld()->GetTimeSeconds() - QuestionStartTime;
	return FMath::Max(0.f, TimePerQuestion - Elapsed);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void AMapsGameMode::LoadCountryDatabase()
{
	CountryDB = NewObject<UCountryDatabase>(this);
	if (CountryDB)
	{
		CountryDB->Initialize();

		ShuffledCountries = CountryDB->GetAllCountries();

		// Shuffle the question order.
		for (int32 i = ShuffledCountries.Num() - 1; i > 0; --i)
		{
			int32 j = FMath::RandRange(0, i);
			ShuffledCountries.Swap(i, j);
		}
	}
}
