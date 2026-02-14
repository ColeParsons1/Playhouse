// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/GuessSign/GuessSignGameMode.h"
#include "Games/GuessSign/GuessSignHUD.h"
#include "Games/GuessSign/SignDatabase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Algo/RandomShuffle.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AGuessSignGameMode::AGuessSignGameMode()
{
	GameType = EPHCGameType::GuessSign;
	GameHUDClass = UGuessSignHUD::StaticClass();
	MaxGameTime = 0.f; // Managed per-question, not globally.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AGuessSignGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AGuessSignGameMode::StartGame()
{
	Super::StartGame();

	CorrectAnswers = 0;
	TotalQuestions = 0;
	CurrentStreak = 0;
	BestStreak = 0;
	CurrentSignIndex = 0;
	bQuestionActive = false;
	CurrentChoices.Empty();

	LoadSignDatabase();
	PresentQuestion();
}

// ---------------------------------------------------------------------------
// Core gameplay
// ---------------------------------------------------------------------------

void AGuessSignGameMode::PresentQuestion()
{
	if (!bGameActive || TotalQuestions >= MaxQuestions)
	{
		// Game over.
		int32 AccuracyBonus = FMath::RoundToInt32(GetAccuracy() * 200.f);
		int32 StreakBonusTotal = BestStreak * StreakBonus;
		AddScore(AccuracyBonus + StreakBonusTotal);
		EndGame(GetAccuracy() >= 0.5f);
		return;
	}

	if (CurrentSignIndex >= ShuffledSigns.Num())
	{
		// Ran out of unique signs; reshuffle.
		Algo::RandomShuffle(ShuffledSigns);
		CurrentSignIndex = 0;
	}

	const FPHCSignEntry& CurrentSign = ShuffledSigns[CurrentSignIndex];

	// Generate 4 choices: 1 correct + 3 distractors.
	CurrentChoices.Empty();

	if (SignDB)
	{
		TArray<FString> Distractors = SignDB->GetRandomChoices(CurrentSign.SignName, 3);
		CurrentChoices.Add(CurrentSign.SignName);
		CurrentChoices.Append(Distractors);

		// Shuffle the choices.
		for (int32 i = CurrentChoices.Num() - 1; i > 0; --i)
		{
			int32 j = FMath::RandRange(0, i);
			CurrentChoices.Swap(i, j);
		}

		// Find where the correct answer ended up.
		CorrectChoiceIndex = CurrentChoices.IndexOfByKey(CurrentSign.SignName);
	}
	else
	{
		// Fallback if database unavailable.
		CurrentChoices = { CurrentSign.SignName, TEXT("Option B"), TEXT("Option C"), TEXT("Option D") };
		CorrectChoiceIndex = 0;
	}

	bQuestionActive = true;
	QuestionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Start the per-question timer.
	GetWorldTimerManager().ClearTimer(QuestionTimer);
	GetWorldTimerManager().SetTimer(
		QuestionTimer, this, &AGuessSignGameMode::OnTimeUp, TimePerQuestion, false);

	OnQuestionPresented.Broadcast(CurrentSign.ImagePath, CurrentChoices);
}

bool AGuessSignGameMode::SubmitAnswer(int32 ChoiceIndex)
{
	if (!bGameActive || !bQuestionActive)
	{
		return false;
	}

	bQuestionActive = false;
	GetWorldTimerManager().ClearTimer(QuestionTimer);
	TotalQuestions++;

	bool bCorrect = (ChoiceIndex == CorrectChoiceIndex);

	if (bCorrect)
	{
		CorrectAnswers++;
		CurrentStreak++;
		BestStreak = FMath::Max(BestStreak, CurrentStreak);

		// Points: base + time bonus + streak bonus.
		float TimeElapsed = GetWorld()
			? (GetWorld()->GetTimeSeconds() - QuestionStartTime)
			: TimePerQuestion;
		float TimeFraction = FMath::Clamp(1.f - (TimeElapsed / TimePerQuestion), 0.f, 1.f);
		int32 TimeBonus = FMath::RoundToInt32(TimeFraction * 30.f);
		int32 StreakPts = FMath::Min(CurrentStreak, 10) * StreakBonus;

		AddScore(CorrectPoints + TimeBonus + StreakPts);
	}
	else
	{
		CurrentStreak = 0;
	}

	FString CorrectAnswer = CurrentChoices.IsValidIndex(CorrectChoiceIndex)
		? CurrentChoices[CorrectChoiceIndex]
		: TEXT("");

	OnAnswerSubmitted.Broadcast(bCorrect, CorrectAnswer);

	return bCorrect;
}

FString AGuessSignGameMode::GetCurrentSignImagePath() const
{
	if (ShuffledSigns.IsValidIndex(CurrentSignIndex))
	{
		return ShuffledSigns[CurrentSignIndex].ImagePath;
	}
	return FString();
}

void AGuessSignGameMode::OnTimeUp()
{
	if (!bGameActive || !bQuestionActive)
	{
		return;
	}

	bQuestionActive = false;
	TotalQuestions++;
	CurrentStreak = 0;

	FString CorrectAnswer = CurrentChoices.IsValidIndex(CorrectChoiceIndex)
		? CurrentChoices[CorrectChoiceIndex]
		: TEXT("");

	OnSignTimeUp.Broadcast();
	OnAnswerSubmitted.Broadcast(false, CorrectAnswer);
}

void AGuessSignGameMode::NextQuestion()
{
	CurrentSignIndex++;
	PresentQuestion();
}

float AGuessSignGameMode::GetAccuracy() const
{
	if (TotalQuestions == 0)
	{
		return 0.f;
	}
	return static_cast<float>(CorrectAnswers) / static_cast<float>(TotalQuestions);
}

float AGuessSignGameMode::GetQuestionTimeRemaining() const
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

void AGuessSignGameMode::LoadSignDatabase()
{
	SignDB = NewObject<USignDatabase>(this);
	if (SignDB)
	{
		SignDB->Initialize();
		ShuffledSigns = SignDB->GetAllSigns();

		// Shuffle the question order.
		for (int32 i = ShuffledSigns.Num() - 1; i > 0; --i)
		{
			int32 j = FMath::RandRange(0, i);
			ShuffledSigns.Swap(i, j);
		}
	}
}
