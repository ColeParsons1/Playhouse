// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/AngryDucks/AngryDucksGameMode.h"
#include "Games/AngryDucks/AngryDucksHUD.h"
#include "Games/AngryDucks/DuckProjectile.h"
#include "Games/AngryDucks/DestructibleBlock.h"
#include "Games/AngryDucks/PigTarget.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------

AAngryDucksGameMode::AAngryDucksGameMode()
{
	GameType = EPHCGameType::AngryDucks;
	GameHUDClass = UAngryDucksHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------

void AAngryDucksGameMode::BeginPlay()
{
	Super::BeginPlay();
	StartGame();
}

void AAngryDucksGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Handle settle delay -- after a duck settles we wait briefly before
	// preparing the next one so the player can see the destruction.
	if (bWaitingAfterSettle)
	{
		SettleDelayTimer -= DeltaTime;
		if (SettleDelayTimer <= 0.f)
		{
			bWaitingAfterSettle = false;
			CheckRoundState();
		}
	}
}

// ---------------------------------------------------------------------------

void AAngryDucksGameMode::StartGame()
{
	Super::StartGame();

	CurrentLevel = 0;
	NextLevel();
}

// ---------------------------------------------------------------------------
// -- Player input --
// ---------------------------------------------------------------------------

void AAngryDucksGameMode::LaunchDuck(FVector2D Direction, float Power)
{
	if (!bGameActive || bGamePaused || !bReadyToLaunch || DucksRemaining <= 0)
	{
		return;
	}

	if (!ActiveDuck)
	{
		PrepareDuck();
		if (!ActiveDuck)
		{
			return;
		}
	}

	// Normalise direction and scale by power
	Direction.Normalize();
	const float ClampedPower = FMath::Clamp(Power, 0.1f, 1.f) * LaunchPower;

	const FVector Velocity(Direction.X * ClampedPower, 0.f, Direction.Y * ClampedPower);
	ActiveDuck->Launch(Velocity);

	--DucksRemaining;
	bReadyToLaunch = false;

	OnDuckLaunched.Broadcast(ActiveDuck);
}

void AAngryDucksGameMode::SetAimDirection(FVector2D Direction)
{
	AimDirection = Direction;
}

// ---------------------------------------------------------------------------
// -- Level progression --
// ---------------------------------------------------------------------------

void AAngryDucksGameMode::NextLevel()
{
	++CurrentLevel;

	if (CurrentLevel > TotalLevels)
	{
		// All levels complete -- award bonus for remaining ducks and end.
		AddScore(DucksRemaining * 200);
		EndGame(/*bWon=*/ true);
		return;
	}

	ClearLevel();
	SpawnLevel(CurrentLevel);

	// Determine duck count per level (level 1: 5, level 2: 4, level 3: 3).
	DucksRemaining = FMath::Clamp(MaxDucks - (CurrentLevel - 1), 3, MaxDucks);

	bReadyToLaunch = true;
	bWaitingAfterSettle = false;
	ActiveDuck = nullptr;

	OnLevelCleared.Broadcast(CurrentLevel);
}

// ---------------------------------------------------------------------------
// -- Spawning --
// ---------------------------------------------------------------------------

void AAngryDucksGameMode::ClearLevel()
{
	for (ADestructibleBlock* Block : Blocks)
	{
		if (Block)
		{
			Block->Destroy();
		}
	}
	Blocks.Empty();

	for (APigTarget* Pig : Pigs)
	{
		if (Pig)
		{
			Pig->Destroy();
		}
	}
	Pigs.Empty();

	if (ActiveDuck)
	{
		ActiveDuck->Destroy();
		ActiveDuck = nullptr;
	}

	PigsRemaining = 0;
}

void AAngryDucksGameMode::SpawnLevel(int32 Level)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Block base position -- structures are placed to the right of the slingshot.
	const float BaseX = 800.f;
	const float BaseZ = 50.f;

	if (Level == 1)
	{
		// Level 1: Simple tower -- 3 wood blocks stacked, 1 pig on top.
		// Foundation blocks
		Blocks.Add(World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX - 30.f, 0.f, BaseZ), FRotator::ZeroRotator, SP));
		Blocks.Add(World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX + 30.f, 0.f, BaseZ), FRotator::ZeroRotator, SP));

		// Second level
		Blocks.Add(World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX, 0.f, BaseZ + 60.f), FRotator::ZeroRotator, SP));

		// Set block types -- all wood for level 1
		for (ADestructibleBlock* Block : Blocks)
		{
			if (Block)
			{
				Block->SetBlockType(EBlockType::Wood);
				Block->OnBlockDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnBlockDestroyed);
			}
		}

		// Pig on top of the structure
		APigTarget* Pig = World->SpawnActor<APigTarget>(
			APigTarget::StaticClass(), FVector(BaseX, 0.f, BaseZ + 120.f), FRotator::ZeroRotator, SP);
		if (Pig)
		{
			Pig->OnPigDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnPigKilled);
			Pigs.Add(Pig);
		}
	}
	else if (Level == 2)
	{
		// Level 2: Two towers with a bridge, 2 pigs.
		// Left tower
		Blocks.Add(World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX - 80.f, 0.f, BaseZ), FRotator::ZeroRotator, SP));
		Blocks.Add(World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX - 80.f, 0.f, BaseZ + 60.f), FRotator::ZeroRotator, SP));

		// Right tower
		Blocks.Add(World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX + 80.f, 0.f, BaseZ), FRotator::ZeroRotator, SP));
		Blocks.Add(World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX + 80.f, 0.f, BaseZ + 60.f), FRotator::ZeroRotator, SP));

		// Bridge (stone block spanning the gap)
		ADestructibleBlock* Bridge = World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX, 0.f, BaseZ + 120.f), FRotator::ZeroRotator, SP);
		if (Bridge)
		{
			Bridge->SetBlockType(EBlockType::Stone);
			Blocks.Add(Bridge);
		}

		// Glass blocks protecting the lower pigs
		ADestructibleBlock* GlassLeft = World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX - 30.f, 0.f, BaseZ), FRotator::ZeroRotator, SP);
		if (GlassLeft)
		{
			GlassLeft->SetBlockType(EBlockType::Glass);
			Blocks.Add(GlassLeft);
		}
		ADestructibleBlock* GlassRight = World->SpawnActor<ADestructibleBlock>(
			ADestructibleBlock::StaticClass(), FVector(BaseX + 30.f, 0.f, BaseZ), FRotator::ZeroRotator, SP);
		if (GlassRight)
		{
			GlassRight->SetBlockType(EBlockType::Glass);
			Blocks.Add(GlassRight);
		}

		// Set wood type for tower blocks
		for (int32 i = 0; i < 4; ++i)
		{
			if (Blocks.IsValidIndex(i) && Blocks[i])
			{
				Blocks[i]->SetBlockType(EBlockType::Wood);
			}
		}

		// Bind all block destroy delegates
		for (ADestructibleBlock* Block : Blocks)
		{
			if (Block)
			{
				Block->OnBlockDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnBlockDestroyed);
			}
		}

		// Pig under the bridge
		APigTarget* Pig1 = World->SpawnActor<APigTarget>(
			APigTarget::StaticClass(), FVector(BaseX, 0.f, BaseZ + 30.f), FRotator::ZeroRotator, SP);
		if (Pig1)
		{
			Pig1->OnPigDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnPigKilled);
			Pigs.Add(Pig1);
		}

		// Pig on top of the bridge
		APigTarget* Pig2 = World->SpawnActor<APigTarget>(
			APigTarget::StaticClass(), FVector(BaseX, 0.f, BaseZ + 160.f), FRotator::ZeroRotator, SP);
		if (Pig2)
		{
			Pig2->OnPigDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnPigKilled);
			Pigs.Add(Pig2);
		}
	}
	else if (Level == 3)
	{
		// Level 3: Fortress -- stone base, wood upper, glass accents, 3 pigs.
		// Bottom row (stone)
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = BaseX - 90.f + i * 60.f;
			ADestructibleBlock* StoneBlock = World->SpawnActor<ADestructibleBlock>(
				ADestructibleBlock::StaticClass(), FVector(X, 0.f, BaseZ), FRotator::ZeroRotator, SP);
			if (StoneBlock)
			{
				StoneBlock->SetBlockType(EBlockType::Stone);
				StoneBlock->OnBlockDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnBlockDestroyed);
				Blocks.Add(StoneBlock);
			}
		}

		// Middle row (wood)
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = BaseX - 60.f + i * 60.f;
			ADestructibleBlock* WoodBlock = World->SpawnActor<ADestructibleBlock>(
				ADestructibleBlock::StaticClass(), FVector(X, 0.f, BaseZ + 60.f), FRotator::ZeroRotator, SP);
			if (WoodBlock)
			{
				WoodBlock->SetBlockType(EBlockType::Wood);
				WoodBlock->OnBlockDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnBlockDestroyed);
				Blocks.Add(WoodBlock);
			}
		}

		// Top row (glass)
		for (int32 i = 0; i < 2; ++i)
		{
			const float X = BaseX - 30.f + i * 60.f;
			ADestructibleBlock* GlassBlock = World->SpawnActor<ADestructibleBlock>(
				ADestructibleBlock::StaticClass(), FVector(X, 0.f, BaseZ + 120.f), FRotator::ZeroRotator, SP);
			if (GlassBlock)
			{
				GlassBlock->SetBlockType(EBlockType::Glass);
				GlassBlock->OnBlockDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnBlockDestroyed);
				Blocks.Add(GlassBlock);
			}
		}

		// Pigs sheltered inside the fortress
		APigTarget* Pig1 = World->SpawnActor<APigTarget>(
			APigTarget::StaticClass(), FVector(BaseX - 30.f, 0.f, BaseZ + 30.f), FRotator::ZeroRotator, SP);
		if (Pig1)
		{
			Pig1->OnPigDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnPigKilled);
			Pigs.Add(Pig1);
		}

		APigTarget* Pig2 = World->SpawnActor<APigTarget>(
			APigTarget::StaticClass(), FVector(BaseX + 30.f, 0.f, BaseZ + 30.f), FRotator::ZeroRotator, SP);
		if (Pig2)
		{
			Pig2->OnPigDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnPigKilled);
			Pigs.Add(Pig2);
		}

		APigTarget* Pig3 = World->SpawnActor<APigTarget>(
			APigTarget::StaticClass(), FVector(BaseX, 0.f, BaseZ + 90.f), FRotator::ZeroRotator, SP);
		if (Pig3)
		{
			Pig3->OnPigDestroyed.AddDynamic(this, &AAngryDucksGameMode::OnPigKilled);
			Pigs.Add(Pig3);
		}
	}

	PigsRemaining = Pigs.Num();
}

void AAngryDucksGameMode::PrepareDuck()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ActiveDuck = World->SpawnActor<ADuckProjectile>(
		ADuckProjectile::StaticClass(), SlingPosition, FRotator::ZeroRotator, SP);

	if (ActiveDuck)
	{
		ActiveDuck->OnDuckSettled.AddDynamic(this, &AAngryDucksGameMode::OnDuckSettled);
	}
}

// ---------------------------------------------------------------------------
// -- Delegate handlers --
// ---------------------------------------------------------------------------

void AAngryDucksGameMode::OnBlockDestroyed(ADestructibleBlock* Block)
{
	AddScore(50);

	// Remove from tracked list
	Blocks.Remove(Block);
}

void AAngryDucksGameMode::OnPigKilled(APigTarget* Pig)
{
	AddScore(100);
	--PigsRemaining;

	OnPigDestroyed.Broadcast(Pig);

	// Remove from tracked list
	Pigs.Remove(Pig);
}

void AAngryDucksGameMode::OnDuckSettled(ADuckProjectile* Duck)
{
	// Start the settle delay timer before checking round state.
	bWaitingAfterSettle = true;
	SettleDelayTimer = SettleDelay;
	ActiveDuck = nullptr;
}

// ---------------------------------------------------------------------------
// -- Round logic --
// ---------------------------------------------------------------------------

void AAngryDucksGameMode::CheckRoundState()
{
	if (PigsRemaining <= 0)
	{
		// Level cleared! Bonus for remaining ducks.
		AddScore(DucksRemaining * 100);
		NextLevel();
		return;
	}

	if (DucksRemaining <= 0)
	{
		// Out of ducks with pigs remaining -- game over.
		EndGame(/*bWon=*/ false);
		return;
	}

	// Prepare the next duck for launching.
	bReadyToLaunch = true;
	PrepareDuck();
}
