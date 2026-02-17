// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/TowerDefense/TDGameMode.h"
#include "Games/TowerDefense/TDHUD.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ATDGameMode::ATDGameMode()
{
	GameType = EPHCGameType::TowerDefense;
	GameHUDClass = UTDHUD::StaticClass();
	MaxGameTime = 0.f; // No time limit; game ends when waves are complete or lives hit 0.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void ATDGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Move enemies along the path.
	MoveEnemies(DeltaTime);

	// Update tower targeting and firing.
	UpdateTowers(DeltaTime);

	// Check if wave is complete.
	if (bWaveActive && Enemies.Num() == 0 && EnemiesSpawned >= EnemiesInWave)
	{
		bWaveActive = false;

		// Award gold bonus for wave completion.
		Gold += 20 + Wave * 5;

		// Add score for wave completion.
		AddScore(100 * Wave);

		// Check for final victory.
		if (Wave >= MaxWaves)
		{
			EndGame(/*bWon=*/ true);
		}
	}
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void ATDGameMode::StartGame()
{
	Super::StartGame();

	// Reset state.
	Gold = StartingGold;
	Lives = StartingLives;
	Wave = 0;
	bWaveActive = false;
	EnemiesSpawned = 0;
	EnemiesInWave = 0;
	EnemiesKilledInWave = 0;
	NextEnemyId = 1;
	Towers.Empty();
	Enemies.Empty();

	// Clear any pending spawn timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	SetupGrid();
}

void ATDGameMode::StartWave()
{
	if (!bGameActive || bWaveActive)
	{
		return;
	}

	Wave++;
	bWaveActive = true;
	EnemiesSpawned = 0;
	EnemiesKilledInWave = 0;

	CurrentWaveConfig = GetWaveConfig(Wave);
	EnemiesInWave = CurrentWaveConfig.EnemyCount;

	// Start spawning enemies at the configured interval.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&ATDGameMode::SpawnEnemy,
			CurrentWaveConfig.SpawnInterval,
			true, // looping
			0.f   // first delay
		);
	}
}

// ---------------------------------------------------------------------------
// Grid setup
// ---------------------------------------------------------------------------

void ATDGameMode::SetupGrid()
{
	// Initialize grid to all empty.
	Grid.SetNumZeroed(GridWidth * GridHeight);

	// Create a winding path from left edge to right edge.
	EnemyPath.Empty();

	// Path design: enter from left, snake through the grid.
	// Row 3: left to middle
	for (int32 X = 0; X <= 6; ++X)
	{
		EnemyPath.Add(FIntPoint(X, 3));
	}
	// Column 6: row 3 to row 1
	for (int32 Y = 2; Y >= 1; --Y)
	{
		EnemyPath.Add(FIntPoint(6, Y));
	}
	// Row 1: column 6 to column 10
	for (int32 X = 7; X <= 10; ++X)
	{
		EnemyPath.Add(FIntPoint(X, 1));
	}
	// Column 10: row 1 to row 5
	for (int32 Y = 2; Y <= 5; ++Y)
	{
		EnemyPath.Add(FIntPoint(10, Y));
	}
	// Row 5: column 10 to column 4
	for (int32 X = 9; X >= 4; --X)
	{
		EnemyPath.Add(FIntPoint(X, 5));
	}
	// Column 4: row 5 to row 7
	for (int32 Y = 6; Y <= 7; --Y)
	{
		EnemyPath.Add(FIntPoint(4, Y));
	}
	// Row 7: column 4 to right edge
	for (int32 X = 5; X < GridWidth; ++X)
	{
		EnemyPath.Add(FIntPoint(X, 7));
	}

	// Mark path cells on the grid.
	for (const FIntPoint& Cell : EnemyPath)
	{
		if (Cell.X >= 0 && Cell.X < GridWidth && Cell.Y >= 0 && Cell.Y < GridHeight)
		{
			Grid[Cell.Y * GridWidth + Cell.X] = 1;
		}
	}
}

// ---------------------------------------------------------------------------
// Tower management
// ---------------------------------------------------------------------------

bool ATDGameMode::PlaceTower(FIntPoint Pos, EPHCTowerType Type)
{
	if (!bGameActive)
	{
		return false;
	}

	// Validate bounds.
	if (Pos.X < 0 || Pos.X >= GridWidth || Pos.Y < 0 || Pos.Y >= GridHeight)
	{
		return false;
	}

	// Check cell is empty (buildable).
	const int32 CellIndex = Pos.Y * GridWidth + Pos.X;
	if (Grid[CellIndex] != 0)
	{
		return false;
	}

	// Check gold.
	const int32 Cost = GetTowerCost(Type);
	if (Gold < Cost)
	{
		return false;
	}

	// Deduct gold and place tower.
	Gold -= Cost;
	Grid[CellIndex] = 2;

	FPHCTower NewTower;
	NewTower.Type = Type;
	NewTower.GridPos = Pos;
	NewTower.Level = 1;

	// Set stats based on type.
	switch (Type)
	{
	case EPHCTowerType::Arrow:
		NewTower.Range = 120.f;
		NewTower.Damage = 10.f;
		NewTower.FireRate = 2.f;
		break;
	case EPHCTowerType::Cannon:
		NewTower.Range = 100.f;
		NewTower.Damage = 40.f;
		NewTower.FireRate = 0.5f;
		break;
	case EPHCTowerType::Ice:
		NewTower.Range = 110.f;
		NewTower.Damage = 5.f;
		NewTower.FireRate = 1.f;
		break;
	case EPHCTowerType::Lightning:
		NewTower.Range = 130.f;
		NewTower.Damage = 15.f;
		NewTower.FireRate = 0.8f;
		break;
	}

	Towers.Add(MoveTemp(NewTower));
	return true;
}

bool ATDGameMode::UpgradeTower(int32 TowerIndex)
{
	if (!Towers.IsValidIndex(TowerIndex))
	{
		return false;
	}

	const int32 Cost = GetUpgradeCost(TowerIndex);
	if (Gold < Cost)
	{
		return false;
	}

	Gold -= Cost;

	FPHCTower& Tower = Towers[TowerIndex];
	Tower.Level++;
	Tower.Damage *= 1.3f;
	Tower.Range *= 1.1f;
	Tower.FireRate *= 1.15f;

	return true;
}

void ATDGameMode::SellTower(int32 TowerIndex)
{
	if (!Towers.IsValidIndex(TowerIndex))
	{
		return;
	}

	const int32 Refund = GetSellRefund(TowerIndex);
	Gold += Refund;

	// Clear grid cell.
	const FIntPoint Pos = Towers[TowerIndex].GridPos;
	const int32 CellIndex = Pos.Y * GridWidth + Pos.X;
	if (Grid.IsValidIndex(CellIndex))
	{
		Grid[CellIndex] = 0;
	}

	Towers.RemoveAt(TowerIndex);
}

int32 ATDGameMode::GetTowerCost(EPHCTowerType Type) const
{
	switch (Type)
	{
	case EPHCTowerType::Arrow:		return 25;
	case EPHCTowerType::Cannon:		return 50;
	case EPHCTowerType::Ice:		return 35;
	case EPHCTowerType::Lightning:	return 60;
	default:						return 25;
	}
}

int32 ATDGameMode::GetUpgradeCost(int32 TowerIndex) const
{
	if (!Towers.IsValidIndex(TowerIndex))
	{
		return 0;
	}

	const FPHCTower& Tower = Towers[TowerIndex];
	return GetTowerCost(Tower.Type) * Tower.Level;
}

int32 ATDGameMode::GetSellRefund(int32 TowerIndex) const
{
	if (!Towers.IsValidIndex(TowerIndex))
	{
		return 0;
	}

	const FPHCTower& Tower = Towers[TowerIndex];
	// Refund 60% of total invested gold.
	int32 TotalInvested = GetTowerCost(Tower.Type);
	for (int32 Lvl = 1; Lvl < Tower.Level; ++Lvl)
	{
		TotalInvested += GetTowerCost(Tower.Type) * Lvl;
	}
	return FMath::FloorToInt32(TotalInvested * 0.6f);
}

FPHCWaveConfig ATDGameMode::GetWaveConfig(int32 WaveNumber) const
{
	FPHCWaveConfig Config;

	// Base enemy count scales with wave.
	Config.EnemyCount = 5 + WaveNumber * 2;
	Config.SpawnInterval = FMath::Max(0.4f, 1.5f - WaveNumber * 0.05f);
	Config.HPMultiplier = 1.f + (WaveNumber - 1) * 0.2f;
	Config.SpeedMultiplier = 1.f + (WaveNumber - 1) * 0.05f;

	// Determine which enemy types appear.
	Config.EnemyTypes.Add(EPHCEnemyType::Basic);

	if (WaveNumber >= 3)
	{
		Config.EnemyTypes.Add(EPHCEnemyType::Fast);
	}
	if (WaveNumber >= 6)
	{
		Config.EnemyTypes.Add(EPHCEnemyType::Tank);
	}
	if (WaveNumber >= 10 && WaveNumber % 5 == 0)
	{
		Config.EnemyTypes.Add(EPHCEnemyType::Boss);
	}

	return Config;
}

// ---------------------------------------------------------------------------
// Enemy spawning
// ---------------------------------------------------------------------------

void ATDGameMode::SpawnEnemy()
{
	if (EnemiesSpawned >= EnemiesInWave)
	{
		// All enemies spawned for this wave, stop the timer.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
		return;
	}

	// Pick a random enemy type from the wave config.
	EPHCEnemyType Type = EPHCEnemyType::Basic;
	if (CurrentWaveConfig.EnemyTypes.Num() > 0)
	{
		Type = CurrentWaveConfig.EnemyTypes[
			FMath::RandRange(0, CurrentWaveConfig.EnemyTypes.Num() - 1)];
	}

	FPHCEnemy NewEnemy;
	NewEnemy.Type = Type;
	NewEnemy.PathIndex = 0;
	NewEnemy.DistanceTraveled = 0.f;
	NewEnemy.bSlowed = false;
	NewEnemy.SlowTimer = 0.f;
	NewEnemy.UniqueId = NextEnemyId++;

	// Set stats based on type, scaled by wave config.
	switch (Type)
	{
	case EPHCEnemyType::Basic:
		NewEnemy.MaxHP = 100.f * CurrentWaveConfig.HPMultiplier;
		NewEnemy.Speed = 40.f * CurrentWaveConfig.SpeedMultiplier;
		break;
	case EPHCEnemyType::Fast:
		NewEnemy.MaxHP = 60.f * CurrentWaveConfig.HPMultiplier;
		NewEnemy.Speed = 70.f * CurrentWaveConfig.SpeedMultiplier;
		break;
	case EPHCEnemyType::Tank:
		NewEnemy.MaxHP = 300.f * CurrentWaveConfig.HPMultiplier;
		NewEnemy.Speed = 25.f * CurrentWaveConfig.SpeedMultiplier;
		break;
	case EPHCEnemyType::Boss:
		NewEnemy.MaxHP = 800.f * CurrentWaveConfig.HPMultiplier;
		NewEnemy.Speed = 20.f * CurrentWaveConfig.SpeedMultiplier;
		break;
	}
	NewEnemy.HP = NewEnemy.MaxHP;

	// Start at the first waypoint.
	if (EnemyPath.Num() > 0)
	{
		NewEnemy.WorldPosition = FVector2D(
			EnemyPath[0].X * CellSize + CellSize * 0.5f,
			EnemyPath[0].Y * CellSize + CellSize * 0.5f);
	}

	Enemies.Add(MoveTemp(NewEnemy));
	EnemiesSpawned++;
}

// ---------------------------------------------------------------------------
// Enemy movement
// ---------------------------------------------------------------------------

void ATDGameMode::MoveEnemies(float DT)
{
	for (int32 i = Enemies.Num() - 1; i >= 0; --i)
	{
		FPHCEnemy& Enemy = Enemies[i];

		// Update slow timer.
		if (Enemy.bSlowed)
		{
			Enemy.SlowTimer -= DT;
			if (Enemy.SlowTimer <= 0.f)
			{
				Enemy.bSlowed = false;
				Enemy.SlowTimer = 0.f;
			}
		}

		// Calculate effective speed.
		const float EffectiveSpeed = Enemy.bSlowed
			? Enemy.Speed * IceSlowMultiplier
			: Enemy.Speed;

		// Move toward next waypoint.
		if (Enemy.PathIndex + 1 >= EnemyPath.Num())
		{
			// Enemy has reached the end of the path.
			OnEnemyReachedEnd(i);
			continue;
		}

		const FVector2D CurrentWP = GridToWorld(EnemyPath[Enemy.PathIndex]);
		const FVector2D NextWP = GridToWorld(EnemyPath[Enemy.PathIndex + 1]);
		const FVector2D Direction = (NextWP - Enemy.WorldPosition);
		const float SegmentDistance = Direction.Size();

		if (SegmentDistance < 1.f)
		{
			// Arrived at next waypoint.
			Enemy.PathIndex++;
			Enemy.WorldPosition = NextWP;
			continue;
		}

		const FVector2D MoveDir = Direction.GetSafeNormal();
		const float MoveAmount = EffectiveSpeed * DT;
		Enemy.WorldPosition += MoveDir * FMath::Min(MoveAmount, SegmentDistance);
		Enemy.DistanceTraveled += MoveAmount;
	}
}

// ---------------------------------------------------------------------------
// Tower updates
// ---------------------------------------------------------------------------

void ATDGameMode::UpdateTowers(float DT)
{
	for (FPHCTower& Tower : Towers)
	{
		Tower.FireTimer += DT;

		// Check if it's time to fire.
		const float FireInterval = (Tower.FireRate > 0.f) ? (1.f / Tower.FireRate) : 1.f;
		if (Tower.FireTimer < FireInterval)
		{
			continue;
		}

		// Find a target.
		const int32 TargetIdx = FindTargetForTower(Tower);
		Tower.TargetEnemyIndex = TargetIdx;

		if (TargetIdx < 0)
		{
			continue;
		}

		// Fire!
		Tower.FireTimer = 0.f;
		DamageEnemy(TargetIdx, Tower.Damage, Tower.Type);
	}
}

int32 ATDGameMode::FindTargetForTower(const FPHCTower& Tower) const
{
	const FVector2D TowerPos = GridToWorld(Tower.GridPos);
	int32 BestIndex = -1;
	float BestDistTraveled = -1.f;

	for (int32 i = 0; i < Enemies.Num(); ++i)
	{
		const FPHCEnemy& Enemy = Enemies[i];
		const float Dist = FVector2D::Distance(TowerPos, Enemy.WorldPosition);

		if (Dist <= Tower.Range)
		{
			// Prefer the enemy furthest along the path (closest to the end).
			if (Enemy.DistanceTraveled > BestDistTraveled)
			{
				BestDistTraveled = Enemy.DistanceTraveled;
				BestIndex = i;
			}
		}
	}

	return BestIndex;
}

// ---------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------

void ATDGameMode::DamageEnemy(int32 EnemyIndex, float Damage, EPHCTowerType SourceType)
{
	if (!Enemies.IsValidIndex(EnemyIndex))
	{
		return;
	}

	FPHCEnemy& Enemy = Enemies[EnemyIndex];
	Enemy.HP -= Damage;

	// Apply tower-specific effects.
	switch (SourceType)
	{
	case EPHCTowerType::Ice:
		Enemy.bSlowed = true;
		Enemy.SlowTimer = IceSlowDuration;
		break;

	case EPHCTowerType::Cannon:
	{
		// Splash damage to nearby enemies.
		const FVector2D ImpactPos = Enemy.WorldPosition;
		for (int32 i = Enemies.Num() - 1; i >= 0; --i)
		{
			if (i == EnemyIndex)
			{
				continue;
			}
			const float Dist = FVector2D::Distance(ImpactPos, Enemies[i].WorldPosition);
			if (Dist <= CannonSplashRadius)
			{
				const float SplashDamage = Damage * 0.5f;
				Enemies[i].HP -= SplashDamage;
				if (Enemies[i].HP <= 0.f)
				{
					OnEnemyDeath(i);
					// Adjust our target index if needed.
					if (i < EnemyIndex)
					{
						EnemyIndex--;
					}
				}
			}
		}
		break;
	}

	case EPHCTowerType::Lightning:
	{
		// Chain to nearby enemies.
		TSet<int32> ChainedIndices;
		ChainedIndices.Add(EnemyIndex);
		FVector2D ChainPos = Enemy.WorldPosition;
		float ChainDamage = Damage * 0.6f;

		for (int32 Chain = 0; Chain < LightningChainCount; ++Chain)
		{
			int32 NearestIdx = -1;
			float NearestDist = 150.f; // Chain range.

			for (int32 i = 0; i < Enemies.Num(); ++i)
			{
				if (ChainedIndices.Contains(i))
				{
					continue;
				}
				const float Dist = FVector2D::Distance(ChainPos, Enemies[i].WorldPosition);
				if (Dist < NearestDist)
				{
					NearestDist = Dist;
					NearestIdx = i;
				}
			}

			if (NearestIdx >= 0)
			{
				ChainedIndices.Add(NearestIdx);
				Enemies[NearestIdx].HP -= ChainDamage;
				ChainPos = Enemies[NearestIdx].WorldPosition;
				ChainDamage *= 0.7f; // Diminishing chain damage.
			}
			else
			{
				break;
			}
		}

		// Process deaths from chain damage (in reverse to preserve indices).
		TArray<int32> DeathIndices;
		for (int32 Idx : ChainedIndices)
		{
			if (Idx != EnemyIndex && Enemies.IsValidIndex(Idx) && Enemies[Idx].HP <= 0.f)
			{
				DeathIndices.Add(Idx);
			}
		}
		DeathIndices.Sort([](const int32& A, const int32& B) { return A > B; });
		for (int32 DeathIdx : DeathIndices)
		{
			OnEnemyDeath(DeathIdx);
			if (DeathIdx < EnemyIndex)
			{
				EnemyIndex--;
			}
		}
		break;
	}

	default:
		break;
	}

	// Check if the primary target died.
	if (Enemies.IsValidIndex(EnemyIndex) && Enemies[EnemyIndex].HP <= 0.f)
	{
		OnEnemyDeath(EnemyIndex);
	}
}

// ---------------------------------------------------------------------------
// Enemy events
// ---------------------------------------------------------------------------

void ATDGameMode::OnEnemyDeath(int32 Index)
{
	if (!Enemies.IsValidIndex(Index))
	{
		return;
	}

	const FPHCEnemy& Enemy = Enemies[Index];

	// Award gold based on enemy type.
	int32 Bounty = 5;
	switch (Enemy.Type)
	{
	case EPHCEnemyType::Basic:	Bounty = 5; break;
	case EPHCEnemyType::Fast:	Bounty = 7; break;
	case EPHCEnemyType::Tank:	Bounty = 15; break;
	case EPHCEnemyType::Boss:	Bounty = 50; break;
	}
	Gold += Bounty;

	// Award score.
	AddScore(Bounty * 2);

	EnemiesKilledInWave++;
	Enemies.RemoveAt(Index);

	// Update tower target indices that may have shifted.
	for (FPHCTower& Tower : Towers)
	{
		if (Tower.TargetEnemyIndex == Index)
		{
			Tower.TargetEnemyIndex = -1;
		}
		else if (Tower.TargetEnemyIndex > Index)
		{
			Tower.TargetEnemyIndex--;
		}
	}
}

void ATDGameMode::OnEnemyReachedEnd(int32 Index)
{
	if (!Enemies.IsValidIndex(Index))
	{
		return;
	}

	Lives--;
	Enemies.RemoveAt(Index);

	// Update tower target indices.
	for (FPHCTower& Tower : Towers)
	{
		if (Tower.TargetEnemyIndex == Index)
		{
			Tower.TargetEnemyIndex = -1;
		}
		else if (Tower.TargetEnemyIndex > Index)
		{
			Tower.TargetEnemyIndex--;
		}
	}

	CheckGameOver();
}

void ATDGameMode::CheckGameOver()
{
	if (Lives <= 0)
	{
		Lives = 0;
		EndGame(/*bWon=*/ false);
	}
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

FVector2D ATDGameMode::GridToWorld(FIntPoint Cell) const
{
	return FVector2D(
		Cell.X * CellSize + CellSize * 0.5f,
		Cell.Y * CellSize + CellSize * 0.5f);
}
