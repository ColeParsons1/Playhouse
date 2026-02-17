// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Cornhole/CornholeGameMode.h"
#include "Games/Cornhole/CornholeHUD.h"
#include "Games/Cornhole/CornholeBoard.h"
#include "Games/Cornhole/BeanBag.h"
#include "Core/PHCTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ACornholeGameMode::ACornholeGameMode()
{
	GameType = EPHCGameType::Cornhole;
	GameHUDClass = UCornholeHUD::StaticClass();
	MaxGameTime = 0.f; // no time limit -- round-based
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ACornholeGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnBoard();
	StartGame();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void ACornholeGameMode::StartGame()
{
	Super::StartGame();

	PlayerRoundScore = 0;
	CPURoundScore = 0;
	PlayerTotalScore = 0;
	CPUTotalScore = 0;
	PlayerBagsThrown = 0;
	CPUBagsThrown = 0;
	CurrentRound = 1;
	bPlayerTurn = true;

	// Clean up any leftover bags from a previous session.
	for (ABeanBag* Bag : AllBags)
	{
		if (Bag && IsValid(Bag))
		{
			Bag->Destroy();
		}
	}
	AllBags.Empty();
	ActiveBag = nullptr;

	StartNewRound();
}

// ---------------------------------------------------------------------------

void ACornholeGameMode::ThrowBag(FVector Direction, float Power)
{
	if (!bGameActive || !bPlayerTurn)
	{
		return;
	}

	// Spawn or reset the active bag for the player.
	SpawnBag(/*bIsPlayer=*/ true);

	if (!ActiveBag)
	{
		return;
	}

	// Compute the launch velocity from direction and scaled power.
	Direction.Normalize();
	const FVector Velocity = Direction * Power * ThrowPowerScale;
	ActiveBag->Launch(Velocity);

	PlayerBagsThrown++;
}

// ---------------------------------------------------------------------------
// Bean-bag settled callback
// ---------------------------------------------------------------------------

void ACornholeGameMode::OnBagSettled(ABeanBag* Bag)
{
	if (!Bag || !bGameActive)
	{
		return;
	}

	const int32 Score = Bag->ScoreValue;
	const bool bIsPlayer = (Bag->BagColor.R > 0.5f); // red = player

	if (bIsPlayer)
	{
		PlayerRoundScore += Score;
	}
	else
	{
		CPURoundScore += Score;
	}

	// Let listeners know a throw has completed.
	OnThrowComplete.Broadcast(Score, bIsPlayer);

	// Alternate turns.
	if (bPlayerTurn)
	{
		bPlayerTurn = false;

		// If the CPU still has bags to throw, schedule the CPU turn.
		if (CPUBagsThrown < BagsPerRound)
		{
			ScheduleCPUThrow();
		}
		else
		{
			// CPU is done; check for round end.
			CheckRoundEnd();
		}
	}
	else
	{
		bPlayerTurn = true;

		// If the player has thrown all bags, check for round end.
		if (PlayerBagsThrown >= BagsPerRound)
		{
			// CPU may still have bags remaining.
			if (CPUBagsThrown < BagsPerRound)
			{
				ScheduleCPUThrow();
			}
			else
			{
				CheckRoundEnd();
			}
		}
	}
}

// ---------------------------------------------------------------------------
// CPU AI
// ---------------------------------------------------------------------------

void ACornholeGameMode::ScheduleCPUThrow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CPUThrowHandle,
			this,
			&ACornholeGameMode::CPUThrow,
			1.5f,
			/*bLoop=*/ false);
	}
}

void ACornholeGameMode::CPUThrow()
{
	if (!bGameActive || !Board)
	{
		return;
	}

	SpawnBag(/*bIsPlayer=*/ false);
	if (!ActiveBag)
	{
		return;
	}

	// Aim towards the board centre with some random offset (~70 % accuracy).
	const FVector BoardCenter = Board->GetActorLocation();
	FVector AimTarget = BoardCenter;

	const float MaxOffset = 30.f;   // cm scatter range
	const float Accuracy = 0.7f;
	const float Scatter = MaxOffset * (1.f - Accuracy);

	AimTarget.X += FMath::FRandRange(-Scatter, Scatter);
	AimTarget.Y += FMath::FRandRange(-Scatter, Scatter);

	FVector Direction = AimTarget - CPUThrowOrigin;
	Direction.Normalize();

	// Vary power slightly.
	const float BasePower = 0.65f;
	const float PowerVariance = 0.15f;
	const float Power = FMath::FRandRange(
		BasePower - PowerVariance,
		BasePower + PowerVariance);

	const FVector Velocity = Direction * Power * ThrowPowerScale;
	ActiveBag->Launch(Velocity);

	CPUBagsThrown++;
}

// ---------------------------------------------------------------------------
// Round management
// ---------------------------------------------------------------------------

void ACornholeGameMode::CheckRoundEnd()
{
	if (PlayerBagsThrown < BagsPerRound || CPUBagsThrown < BagsPerRound)
	{
		return; // still bags remaining
	}

	// Cancellation scoring: only the winning side earns the difference.
	const int32 Difference = PlayerRoundScore - CPURoundScore;
	if (Difference > 0)
	{
		PlayerTotalScore += Difference;
	}
	else if (Difference < 0)
	{
		CPUTotalScore += FMath::Abs(Difference);
	}
	// If tied, neither side scores (by official rules).

	OnRoundEnd.Broadcast(PlayerRoundScore, CPURoundScore);

	// Check for match victory.
	if (PlayerTotalScore >= RoundsToWin * 3) // e.g. 9 points for 3 rounds
	{
		AddScore(PlayerTotalScore);
		EndGame(/*bWon=*/ true);
		return;
	}

	if (CPUTotalScore >= RoundsToWin * 3)
	{
		AddScore(PlayerTotalScore);
		EndGame(/*bWon=*/ false);
		return;
	}

	// Start another round.
	CurrentRound++;
	StartNewRound();
}

void ACornholeGameMode::StartNewRound()
{
	PlayerRoundScore = 0;
	CPURoundScore = 0;
	PlayerBagsThrown = 0;
	CPUBagsThrown = 0;
	bPlayerTurn = true;

	// Destroy bags from the previous round.
	for (ABeanBag* Bag : AllBags)
	{
		if (Bag && IsValid(Bag))
		{
			Bag->Destroy();
		}
	}
	AllBags.Empty();
	ActiveBag = nullptr;
}

// ---------------------------------------------------------------------------
// Spawning helpers
// ---------------------------------------------------------------------------

void ACornholeGameMode::SpawnBoard()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Board = World->SpawnActor<ACornholeBoard>(
		ACornholeBoard::StaticClass(),
		FVector(200.f, 0.f, 0.f),
		FRotator::ZeroRotator,
		Params);
}

void ACornholeGameMode::SpawnBag(bool bIsPlayer)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = bIsPlayer ? PlayerThrowOrigin : CPUThrowOrigin;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABeanBag* Bag = World->SpawnActor<ABeanBag>(
		ABeanBag::StaticClass(),
		Origin,
		FRotator::ZeroRotator,
		Params);

	if (Bag)
	{
		// Red for player, blue for CPU.
		Bag->BagColor = bIsPlayer
			? FLinearColor(0.9f, 0.1f, 0.1f, 1.f)
			: FLinearColor(0.1f, 0.2f, 0.9f, 1.f);

		Bag->OnBagSettled.AddDynamic(this, &ACornholeGameMode::OnBagSettled);
		AllBags.Add(Bag);
	}

	ActiveBag = Bag;
}
