// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Base/PHCMiniGameMode.h"
#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

APHCMiniGameMode::APHCMiniGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void APHCMiniGameMode::BeginPlay()
{
	Super::BeginPlay();
	CachedGameInstance = Cast<UPHCGameInstance>(GetGameInstance());
}

void APHCMiniGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Count up the timer.
	GameTimer += DeltaTime;

	// If a time limit is configured, check for timeout.
	if (MaxGameTime > 0.f && GameTimer >= MaxGameTime)
	{
		EndGame(/*bWon=*/ false);
	}
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void APHCMiniGameMode::StartGame()
{
	CurrentScore = 0;
	GameTimer = 0.f;
	bGameActive = true;
	bGamePaused = false;
	LastResult = FPHCGameResult();
}

void APHCMiniGameMode::EndGame(bool bWon)
{
	if (!bGameActive)
	{
		return;
	}

	bGameActive = false;
	bGamePaused = false;

	// Update high-score for this session.
	if (CurrentScore > HighScore)
	{
		HighScore = CurrentScore;
	}

	// Build the result payload.
	LastResult = CalculateRewards(CurrentScore, bWon);

	// Submit to the backend asynchronously.
	SubmitScoreToServer();

	// Notify listeners (HUD, etc.).
	OnGameEnded.Broadcast(LastResult);
}

void APHCMiniGameMode::PauseGame()
{
	if (bGameActive && !bGamePaused)
	{
		bGamePaused = true;

		// Freeze the world timer so animations / AI also pause.
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::SetGamePaused(World, true);
		}
	}
}

void APHCMiniGameMode::ResumeGame()
{
	if (bGameActive && bGamePaused)
	{
		bGamePaused = false;

		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::SetGamePaused(World, false);
		}
	}
}

void APHCMiniGameMode::AddScore(int32 Points)
{
	if (bGameActive)
	{
		CurrentScore = FMath::Max(0, CurrentScore + Points);
	}
}

float APHCMiniGameMode::GetTimeRemaining() const
{
	if (MaxGameTime <= 0.f)
	{
		return 0.f; // unlimited
	}
	return FMath::Max(0.f, MaxGameTime - GameTimer);
}

void APHCMiniGameMode::ReturnToLobby()
{
	// Ensure unpaused before travel.
	if (bGamePaused)
	{
		ResumeGame();
	}

	bGameActive = false;

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, LobbyMapName);
	}
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void APHCMiniGameMode::SubmitScoreToServer()
{
	if (!CachedGameInstance)
	{
		return;
	}

	UPHCHttpClient* Http = CachedGameInstance->GetHttpClient();
	if (!Http || !Http->IsAuthenticated())
	{
		return;
	}

	// Convert the game type enum to a string for the API endpoint.
	const UEnum* EnumPtr = StaticEnum<EPHCGameType>();
	FString GameTypeString = EnumPtr
		? EnumPtr->GetNameStringByValue(static_cast<int64>(GameType))
		: TEXT("Unknown");

	// Fire-and-forget score submission.
	Http->SubmitScore(
		GameTypeString,
		CurrentScore,
		FPHCJsonResponseDynamic()
	);
}

FPHCGameResult APHCMiniGameMode::CalculateRewards(int32 Score, bool bWon) const
{
	FPHCGameResult Result;
	Result.GameType = GameType;
	Result.Score = Score;
	Result.bWon = bWon;
	Result.TimePlayed = GameTimer;

	// Base coin reward: 10% of score, with a minimum of 5 if the player won.
	int32 BaseCoins = FMath::Max(Score / 10, bWon ? 5 : 0);
	// Win bonus: +50 coins.
	int32 WinBonus = bWon ? 50 : 0;
	Result.CoinsEarned = BaseCoins + WinBonus;

	// XP: score-driven with a small guaranteed amount for participation.
	int32 BaseXP = FMath::Max(Score / 5, 2);
	int32 XPBonus = bWon ? 25 : 0;
	Result.XPEarned = BaseXP + XPBonus;

	return Result;
}
