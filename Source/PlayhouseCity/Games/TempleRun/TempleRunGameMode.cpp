// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/TempleRun/TempleRunGameMode.h"
#include "Games/TempleRun/TempleRunHUD.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ATempleRunGameMode::ATempleRunGameMode()
{
	GameType = EPHCGameType::TempleRun;
	GameHUDClass = UTempleRunHUD::StaticClass();
	MaxGameTime = 0.f; // Endless - no time limit, game ends on collision.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ATempleRunGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void ATempleRunGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// -- Advance distance -----------------------------------------------------

	const float FrameDistance = RunSpeed * DeltaTime;
	DistanceTraveled += FrameDistance;

	// Award 1 score point per 10 distance units traveled.
	const int32 DistanceScore = FMath::FloorToInt32(DistanceTraveled / 10.f);
	if (DistanceScore > CurrentScore - (CoinsCollected * 10))
	{
		CurrentScore = DistanceScore + (CoinsCollected * 10);
	}

	// -- Update jump / slide timers -------------------------------------------

	if (bIsJumping)
	{
		JumpTimer -= DeltaTime;
		if (JumpTimer <= 0.f)
		{
			bIsJumping = false;
			JumpTimer = 0.f;
		}
	}

	if (bIsSliding)
	{
		SlideTimer -= DeltaTime;
		if (SlideTimer <= 0.f)
		{
			bIsSliding = false;
			SlideTimer = 0.f;
		}
	}

	// -- Move tiles and check collisions --------------------------------------

	// Shift all tile ZPositions relative to the player.
	for (FPHCRunnerTile& Tile : ActiveTiles)
	{
		Tile.ZPosition -= FrameDistance;
	}

	CheckCollision();
	CleanupPassedTiles();

	// -- Spawn new tiles as needed --------------------------------------------

	// Calculate the furthest tile edge.
	float FurthestEdge = 0.f;
	for (const FPHCRunnerTile& Tile : ActiveTiles)
	{
		const float TileEnd = Tile.ZPosition + TileLength;
		if (TileEnd > FurthestEdge)
		{
			FurthestEdge = TileEnd;
		}
	}

	// Keep spawning tiles until the visible distance is filled.
	const float VisibleDistance = TileLength * InitialTileCount;
	while (FurthestEdge < VisibleDistance)
	{
		SpawnTile();
		// After spawn, the newest tile is at the end.
		if (ActiveTiles.Num() > 0)
		{
			FurthestEdge = ActiveTiles.Last().ZPosition + TileLength;
		}
		else
		{
			break;
		}
	}

	// -- Increase difficulty over time ----------------------------------------

	IncreaseDifficulty(DeltaTime);
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void ATempleRunGameMode::StartGame()
{
	Super::StartGame();

	// Reset runner state.
	CurrentLane = 1;
	RunSpeed = BaseRunSpeed;
	DistanceTraveled = 0.f;
	CoinsCollected = 0;
	bIsJumping = false;
	bIsSliding = false;
	JumpTimer = 0.f;
	SlideTimer = 0.f;
	SpawnDistance = 0.f;
	DifficultyScalar = 1.f;

	// Clear and spawn initial tiles.
	ActiveTiles.Empty();
	for (int32 i = 0; i < InitialTileCount; ++i)
	{
		SpawnTile();
	}
}

// ---------------------------------------------------------------------------
// Player actions
// ---------------------------------------------------------------------------

void ATempleRunGameMode::SwipeLane(int32 Direction)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	const int32 NewLane = FMath::Clamp(CurrentLane + Direction, 0, 2);
	CurrentLane = NewLane;
}

void ATempleRunGameMode::Jump()
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Can only jump if grounded (not already jumping or sliding).
	if (!bIsJumping && !bIsSliding)
	{
		bIsJumping = true;
		JumpTimer = JumpDuration;
	}
}

void ATempleRunGameMode::Slide()
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Can only slide if grounded.
	if (!bIsJumping && !bIsSliding)
	{
		bIsSliding = true;
		SlideTimer = SlideDuration;
	}
}

// ---------------------------------------------------------------------------
// Tile management
// ---------------------------------------------------------------------------

void ATempleRunGameMode::SpawnTile()
{
	FPHCRunnerTile NewTile;

	// Position the tile at the end of the existing track.
	if (ActiveTiles.Num() > 0)
	{
		NewTile.ZPosition = ActiveTiles.Last().ZPosition + TileLength;
	}
	else
	{
		NewTile.ZPosition = 0.f;
	}

	// -- Generate obstacles ---------------------------------------------------

	// Obstacle probability increases with difficulty.
	const float ObstacleChance = FMath::Clamp(0.3f + (DifficultyScalar - 1.f) * 0.1f, 0.3f, 0.8f);

	if (FMath::FRand() < ObstacleChance && DistanceTraveled > TileLength)
	{
		// Decide how many obstacles (1-2 based on difficulty).
		const int32 ObstacleCount = (FMath::FRand() < (DifficultyScalar * 0.2f)) ? 2 : 1;

		TSet<int32> BlockedLanes;

		for (int32 i = 0; i < ObstacleCount; ++i)
		{
			FPHCRunnerObstacle Obstacle;

			// Choose a random obstacle type.
			const float TypeRoll = FMath::FRand();
			if (TypeRoll < 0.25f)
			{
				Obstacle.Type = EPHCObstacleType::Low;
				Obstacle.Lane = FMath::RandRange(0, 2);
			}
			else if (TypeRoll < 0.50f)
			{
				Obstacle.Type = EPHCObstacleType::High;
				Obstacle.Lane = FMath::RandRange(0, 2);
			}
			else if (TypeRoll < 0.67f)
			{
				Obstacle.Type = EPHCObstacleType::BlockLeft;
				Obstacle.Lane = 0;
				BlockedLanes.Add(0);
			}
			else if (TypeRoll < 0.84f)
			{
				Obstacle.Type = EPHCObstacleType::BlockCenter;
				Obstacle.Lane = 1;
				BlockedLanes.Add(1);
			}
			else
			{
				Obstacle.Type = EPHCObstacleType::BlockRight;
				Obstacle.Lane = 2;
				BlockedLanes.Add(2);
			}

			NewTile.Obstacles.Add(Obstacle);
		}

		// Ensure playability: never block all three lanes without
		// a jump or slide option.
		if (!ValidateTilePlayability(NewTile))
		{
			// Simplify to a single obstacle.
			if (NewTile.Obstacles.Num() > 1)
			{
				NewTile.Obstacles.SetNum(1);
			}
		}
	}

	// -- Generate coins -------------------------------------------------------

	const float CoinChance = 0.4f;
	if (FMath::FRand() < CoinChance)
	{
		// Place coins in 1-3 lanes.
		const int32 CoinLaneCount = FMath::RandRange(1, 3);
		TArray<int32> AvailableLanes = {0, 1, 2};

		for (int32 i = 0; i < CoinLaneCount && AvailableLanes.Num() > 0; ++i)
		{
			const int32 PickIndex = FMath::RandRange(0, AvailableLanes.Num() - 1);
			NewTile.CoinLanes.Add(AvailableLanes[PickIndex]);
			NewTile.CoinCollected.Add(false);
			AvailableLanes.RemoveAt(PickIndex);
		}
	}

	ActiveTiles.Add(MoveTemp(NewTile));
}

bool ATempleRunGameMode::ValidateTilePlayability(const FPHCRunnerTile& Tile) const
{
	// Check if every lane is fully blocked (no jump/slide workaround).
	bool bLaneBlocked[3] = {false, false, false};

	for (const FPHCRunnerObstacle& Obs : Tile.Obstacles)
	{
		switch (Obs.Type)
		{
		case EPHCObstacleType::BlockLeft:
			bLaneBlocked[0] = true;
			break;
		case EPHCObstacleType::BlockCenter:
			bLaneBlocked[1] = true;
			break;
		case EPHCObstacleType::BlockRight:
			bLaneBlocked[2] = true;
			break;
		case EPHCObstacleType::Low:
		case EPHCObstacleType::High:
			// These can be jumped/slid - they don't truly block a lane.
			break;
		}
	}

	// At least one lane must remain unblocked.
	for (int32 i = 0; i < 3; ++i)
	{
		if (!bLaneBlocked[i])
		{
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------

void ATempleRunGameMode::CheckCollision()
{
	if (!bGameActive)
	{
		return;
	}

	// The player position is conceptually at ZPosition ~= 0 (the front).
	// Check tiles whose ZPosition range overlaps the player.
	const float PlayerZ = 0.f;
	const float CollisionThreshold = 30.f; // Half-width of the collision zone.

	for (int32 TileIdx = 0; TileIdx < ActiveTiles.Num(); ++TileIdx)
	{
		FPHCRunnerTile& Tile = ActiveTiles[TileIdx];
		const float TileFront = Tile.ZPosition;
		const float TileBack = Tile.ZPosition + TileLength;

		// Only check tiles the player is currently on.
		if (TileFront > CollisionThreshold || TileBack < -CollisionThreshold)
		{
			continue;
		}

		// -- Check obstacle collisions ----------------------------------------

		for (const FPHCRunnerObstacle& Obs : Tile.Obstacles)
		{
			// Only collide if in the same lane (for lane-based obstacles)
			// or if it's a Low/High type in the player's lane.
			bool bInPlayerLane = (Obs.Lane == CurrentLane);

			if (!bInPlayerLane)
			{
				continue;
			}

			switch (Obs.Type)
			{
			case EPHCObstacleType::Low:
				// Must be jumping to avoid.
				if (!bIsJumping)
				{
					OnHitObstacle();
					return;
				}
				break;

			case EPHCObstacleType::High:
				// Must be sliding to avoid.
				if (!bIsSliding)
				{
					OnHitObstacle();
					return;
				}
				break;

			case EPHCObstacleType::BlockLeft:
			case EPHCObstacleType::BlockCenter:
			case EPHCObstacleType::BlockRight:
				// Cannot dodge in same lane - collision.
				OnHitObstacle();
				return;
			}
		}

		// -- Check coin collection --------------------------------------------

		for (int32 CoinIdx = 0; CoinIdx < Tile.CoinLanes.Num(); ++CoinIdx)
		{
			if (!Tile.CoinCollected.IsValidIndex(CoinIdx) || Tile.CoinCollected[CoinIdx])
			{
				continue;
			}

			if (Tile.CoinLanes[CoinIdx] == CurrentLane)
			{
				CollectCoin(TileIdx, CoinIdx);
			}
		}
	}
}

void ATempleRunGameMode::CollectCoin(int32 TileIndex, int32 CoinIndex)
{
	if (!ActiveTiles.IsValidIndex(TileIndex))
	{
		return;
	}

	FPHCRunnerTile& Tile = ActiveTiles[TileIndex];
	if (!Tile.CoinCollected.IsValidIndex(CoinIndex) || Tile.CoinCollected[CoinIndex])
	{
		return;
	}

	Tile.CoinCollected[CoinIndex] = true;
	CoinsCollected++;
	AddScore(10);
}

void ATempleRunGameMode::OnHitObstacle()
{
	EndGame(/*bWon=*/ false);
}

// ---------------------------------------------------------------------------
// Difficulty
// ---------------------------------------------------------------------------

void ATempleRunGameMode::IncreaseDifficulty(float DeltaTime)
{
	// Gradually increase speed.
	RunSpeed = FMath::Min(RunSpeed + SpeedIncreaseRate * DeltaTime, MaxRunSpeed);

	// Track difficulty scalar based on distance.
	DifficultyScalar = 1.f + (DistanceTraveled / 5000.f);
}

// ---------------------------------------------------------------------------
// Tile cleanup
// ---------------------------------------------------------------------------

void ATempleRunGameMode::CleanupPassedTiles()
{
	// Remove tiles that have scrolled past the player.
	const float RemoveThreshold = -TileLength * 2.f;

	for (int32 i = ActiveTiles.Num() - 1; i >= 0; --i)
	{
		if (ActiveTiles[i].ZPosition + TileLength < RemoveThreshold)
		{
			ActiveTiles.RemoveAt(i);
		}
	}
}
