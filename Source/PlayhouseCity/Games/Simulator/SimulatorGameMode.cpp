// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Simulator/SimulatorGameMode.h"
#include "Games/Simulator/SimulatorHUD.h"

ASimulatorGameMode::ASimulatorGameMode()
{
	GameType = EPHCGameType::Simulator;
	GameHUDClass = USimulatorHUD::StaticClass();
	MaxGameTime = 0.f; // Endless until crash
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ASimulatorGameMode::StartGame()
{
	Super::StartGame();

	PlayerLane = 1;
	CurrentSpeed = StartSpeed;
	DistanceTraveled = 0.f;
	CoinsCollected = 0;
	SpawnAccumulator = 0.f;
	RoadOffset = 0.f;
	Obstacles.Empty();
}

void ASimulatorGameMode::EndGame(bool bWon)
{
	// Final score = distance + coin bonus.
	AddScore(FMath::FloorToInt32(DistanceTraveled / 10.f) + CoinsCollected * 50);
	Super::EndGame(bWon);
}

void ASimulatorGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Increase speed over time.
	CurrentSpeed = FMath::Min(CurrentSpeed + SpeedIncreaseRate * DeltaTime, MaxSpeed);

	// Track distance.
	const float DistThisFrame = CurrentSpeed * DeltaTime;
	DistanceTraveled += DistThisFrame;

	// Update road visual offset.
	RoadOffset = FMath::Fmod(RoadOffset + DistThisFrame * 0.002f, 1.f);

	// Spawn obstacles.
	SpawnAccumulator += DeltaTime;
	// Spawn faster as speed increases.
	const float AdjustedInterval = ObstacleSpawnInterval * (StartSpeed / FMath::Max(CurrentSpeed, 1.f));
	if (SpawnAccumulator >= AdjustedInterval)
	{
		SpawnAccumulator -= AdjustedInterval;
		SpawnObstacle();
	}

	// Move obstacles.
	UpdateObstacles(DeltaTime);

	// Check collisions.
	CheckCollisions();

	// Update running score.
	const int32 NewScore = FMath::FloorToInt32(DistanceTraveled / 10.f) + CoinsCollected * 50;
	if (NewScore > CurrentScore)
	{
		CurrentScore = NewScore;
	}
}

void ASimulatorGameMode::ChangeLane(int32 Direction)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	const int32 NewLane = FMath::Clamp(PlayerLane + Direction, 0, NumLanes - 1);
	PlayerLane = NewLane;
}

void ASimulatorGameMode::SpawnObstacle()
{
	FDrivingObstacle Obs;
	Obs.Lane = FMath::RandRange(0, NumLanes - 1);
	Obs.PositionY = -0.05f; // Start just above the visible area.

	// Decide if it's a coin or an obstacle.
	if (FMath::FRand() < CoinSpawnChance)
	{
		Obs.Type = 2; // Coin
	}
	else
	{
		Obs.Type = FMath::RandRange(0, 1); // Car or barrier
	}

	Obstacles.Add(Obs);

	// Sometimes spawn a second obstacle in a different lane for challenge.
	if (CurrentSpeed > StartSpeed * 1.5f && FMath::FRand() < 0.4f)
	{
		FDrivingObstacle Obs2;
		Obs2.Lane = (Obs.Lane + FMath::RandRange(1, NumLanes - 1)) % NumLanes;
		Obs2.PositionY = -0.05f;
		Obs2.Type = FMath::RandRange(0, 1);
		Obstacles.Add(Obs2);
	}
}

void ASimulatorGameMode::UpdateObstacles(float DeltaTime)
{
	// Obstacle speed relative to screen (normalized 0..1 range).
	const float ObsSpeed = (CurrentSpeed / 400.f) * DeltaTime;

	for (int32 i = Obstacles.Num() - 1; i >= 0; --i)
	{
		if (!Obstacles[i].bActive)
		{
			continue;
		}

		Obstacles[i].PositionY += ObsSpeed;

		// Remove obstacles that have passed the bottom.
		if (Obstacles[i].PositionY > 1.1f)
		{
			Obstacles.RemoveAt(i);
		}
	}
}

void ASimulatorGameMode::CheckCollisions()
{
	// Player car is at approximately Y=0.85 (near bottom).
	const float PlayerY = 0.85f;
	const float CollisionThreshold = 0.06f;

	for (int32 i = Obstacles.Num() - 1; i >= 0; --i)
	{
		FDrivingObstacle& Obs = Obstacles[i];
		if (!Obs.bActive) continue;
		if (Obs.Lane != PlayerLane) continue;
		if (FMath::Abs(Obs.PositionY - PlayerY) > CollisionThreshold) continue;

		if (Obs.Type == 2)
		{
			// Coin collected!
			++CoinsCollected;
			Obs.bActive = false;
			Obstacles.RemoveAt(i);
			OnCoinCollected.Broadcast(CoinsCollected);
		}
		else
		{
			// Crash!
			OnCrash.Broadcast();
			EndGame(/*bWon=*/ false);
			return;
		}
	}
}
