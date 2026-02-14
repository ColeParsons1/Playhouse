// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/PackCatch/PackCatchGameMode.h"
#include "Games/PackCatch/PackCatchHUD.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

APackCatchGameMode::APackCatchGameMode()
{
	GameType = EPHCGameType::PackCatch;
	GameHUDClass = UPackCatchHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void APackCatchGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void APackCatchGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	ElapsedGameTime += DeltaTime;

	// Move all falling items downward.
	for (FPHCFallingItem& Item : FallingItems)
	{
		Item.Position.Y += Item.Speed * GameSpeed * DeltaTime;
	}

	// Check catches and misses (iterate in reverse for safe removal).
	CheckCatch();
	CheckMiss();

	// Gradually ramp difficulty.
	IncreaseDifficulty();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void APackCatchGameMode::StartGame()
{
	Super::StartGame();

	// Reset state.
	Lives = StartingLives;
	PlayerX = FieldWidth * 0.5f;
	FallingItems.Empty();
	NextItemId = 0;
	GameSpeed = 1.0f;
	SpawnInterval = BaseSpawnInterval;
	ElapsedGameTime = 0.f;
	ConsecutiveCatches = 0;

	// Start the periodic spawn timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpawnTimer,
			this,
			&APackCatchGameMode::SpawnPackage,
			SpawnInterval,
			/*bLoop=*/ true
		);
	}
}

void APackCatchGameMode::EndGame(bool bWon)
{
	// Stop spawning.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimer);
	}

	FallingItems.Empty();

	Super::EndGame(bWon);
}

// ---------------------------------------------------------------------------
// Player movement
// ---------------------------------------------------------------------------

void APackCatchGameMode::MovePlayer(float Direction)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const float HalfCatcher = CatcherWidth * 0.5f;

	PlayerX += Direction * CatcherMoveSpeed * DeltaTime;
	PlayerX = FMath::Clamp(PlayerX, HalfCatcher, FieldWidth - HalfCatcher);
}

void APackCatchGameMode::SetPlayerPosition(float X)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	const float HalfCatcher = CatcherWidth * 0.5f;
	PlayerX = FMath::Clamp(X, HalfCatcher, FieldWidth - HalfCatcher);
}

float APackCatchGameMode::GetDifficultyMultiplier() const
{
	return GameSpeed;
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void APackCatchGameMode::SpawnPackage()
{
	if (!bGameActive)
	{
		return;
	}

	FPHCFallingItem NewItem;
	NewItem.ItemId = NextItemId++;

	// Random horizontal position with margin for item size.
	const float HalfItem = ItemSize * 0.5f;
	NewItem.Position.X = FMath::RandRange(HalfItem, FieldWidth - HalfItem);
	NewItem.Position.Y = -ItemSize; // Start above the visible field.
	NewItem.Speed = BaseFallSpeed;

	// Determine package type with weighted random.
	const float Roll = FMath::FRand();
	if (Roll < 0.10f) // 10% chance of bomb
	{
		NewItem.Type = EPHCPackageType::Bomb;
	}
	else if (Roll < 0.20f) // 10% chance of bonus
	{
		NewItem.Type = EPHCPackageType::Bonus;
	}
	else // 80% normal
	{
		NewItem.Type = EPHCPackageType::Normal;
	}

	FallingItems.Add(NewItem);
	OnItemSpawned.Broadcast(NewItem);
}

// ---------------------------------------------------------------------------
// Catch / miss detection
// ---------------------------------------------------------------------------

void APackCatchGameMode::CheckCatch()
{
	const float HalfCatcher = CatcherWidth * 0.5f;
	const float CatcherLeft = PlayerX - HalfCatcher;
	const float CatcherRight = PlayerX + HalfCatcher;
	const float CatcherTop = FieldHeight - CatcherHeight;
	const float HalfItem = ItemSize * 0.5f;

	for (int32 i = FallingItems.Num() - 1; i >= 0; --i)
	{
		const FPHCFallingItem& Item = FallingItems[i];

		// Check if the item is within the catcher zone vertically.
		if (Item.Position.Y + HalfItem < CatcherTop || Item.Position.Y - HalfItem > FieldHeight)
		{
			continue;
		}

		// Check horizontal overlap.
		if (Item.Position.X + HalfItem < CatcherLeft || Item.Position.X - HalfItem > CatcherRight)
		{
			continue;
		}

		// Item caught!
		const int32 CaughtId = Item.ItemId;
		const EPHCPackageType CaughtType = Item.Type;

		switch (CaughtType)
		{
		case EPHCPackageType::Normal:
			AddScore(10);
			ConsecutiveCatches++;
			OnItemCaught.Broadcast(CaughtId, /*bGoodCatch=*/ true);
			break;

		case EPHCPackageType::Bonus:
		{
			// Bonus score: 50 base, plus combo bonus.
			const int32 ComboBonus = FMath::Min(ConsecutiveCatches * 5, 50);
			AddScore(50 + ComboBonus);
			ConsecutiveCatches++;
			OnItemCaught.Broadcast(CaughtId, /*bGoodCatch=*/ true);
			break;
		}

		case EPHCPackageType::Bomb:
			Lives--;
			ConsecutiveCatches = 0;
			OnItemCaught.Broadcast(CaughtId, /*bGoodCatch=*/ false);
			if (Lives <= 0)
			{
				Lives = 0;
				EndGame(/*bWon=*/ false);
				return;
			}
			break;
		}

		FallingItems.RemoveAt(i);
	}
}

void APackCatchGameMode::CheckMiss()
{
	for (int32 i = FallingItems.Num() - 1; i >= 0; --i)
	{
		const FPHCFallingItem& Item = FallingItems[i];

		// Item has fallen below the field.
		if (Item.Position.Y > FieldHeight + ItemSize)
		{
			const int32 MissedId = Item.ItemId;
			const EPHCPackageType MissedType = Item.Type;

			// Only lose a life for missed Normal packages.
			if (MissedType == EPHCPackageType::Normal)
			{
				Lives--;
				ConsecutiveCatches = 0;
				OnItemMissed.Broadcast(MissedId);

				if (Lives <= 0)
				{
					Lives = 0;
					EndGame(/*bWon=*/ false);
					return;
				}
			}
			else
			{
				// Missed bombs / bonus items just disappear.
				OnItemMissed.Broadcast(MissedId);
			}

			FallingItems.RemoveAt(i);
		}
	}
}

// ---------------------------------------------------------------------------
// Difficulty ramping
// ---------------------------------------------------------------------------

void APackCatchGameMode::IncreaseDifficulty()
{
	// Speed increases by 2% every 5 seconds, capped at 3x.
	GameSpeed = FMath::Min(1.0f + (ElapsedGameTime / 5.f) * 0.02f, 3.0f);

	// Spawn interval decreases logarithmically, capped at minimum.
	const float NewInterval = FMath::Max(
		BaseSpawnInterval / GameSpeed,
		MinSpawnInterval
	);

	// Only reset the timer if the interval has changed noticeably.
	if (FMath::Abs(NewInterval - SpawnInterval) > 0.05f)
	{
		SpawnInterval = NewInterval;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SpawnTimer);
			World->GetTimerManager().SetTimer(
				SpawnTimer,
				this,
				&APackCatchGameMode::SpawnPackage,
				SpawnInterval,
				/*bLoop=*/ true
			);
		}
	}
}
