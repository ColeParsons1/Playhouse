// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/BabyShark/BabySharkGameMode.h"
#include "Games/BabyShark/BabySharkHUD.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ABabySharkGameMode::ABabySharkGameMode()
{
	GameType = EPHCGameType::BabyShark;
	GameHUDClass = UBabySharkHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ABabySharkGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void ABabySharkGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused || !bGameStarted)
	{
		return;
	}

	// -- Apply gravity to the shark --
	// Gravity is negative (pulls down in screen space where Y increases downward).
	// We invert: positive SharkVelocity = moving upward (decreasing Y).
	SharkVelocity += Gravity * DeltaTime;
	SharkVelocity = FMath::Clamp(SharkVelocity, -MaxSharkVelocity, MaxSharkVelocity);
	SharkY -= SharkVelocity * DeltaTime; // Y increases downward on screen

	// -- Move pipes --
	UpdatePipes(DeltaTime);

	// -- Spawn new pipes based on distance --
	DistanceSinceLastPipe += PipeSpeed * DeltaTime;
	if (!bFirstPipeSpawned)
	{
		// Delay the first pipe to give the player a moment.
		if (DistanceSinceLastPipe >= PipeSpacing * 0.5f)
		{
			SpawnPipe();
			bFirstPipeSpawned = true;
			DistanceSinceLastPipe = 0.f;
		}
	}
	else if (DistanceSinceLastPipe >= PipeSpacing)
	{
		SpawnPipe();
		DistanceSinceLastPipe = 0.f;
	}

	// -- Collision detection --
	if (CheckCollision())
	{
		// Update best score before ending.
		if (CurrentScore > BestScore)
		{
			BestScore = CurrentScore;
		}

		OnSharkDied.Broadcast();
		EndGame(/*bWon=*/ false);
		return;
	}

	// -- Score checking --
	CheckScore();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void ABabySharkGameMode::StartGame()
{
	Super::StartGame();

	// Reset shark to center.
	SharkY = FieldHeight * 0.5f;
	SharkVelocity = 0.f;

	// Clear pipes.
	Pipes.Empty();
	DistanceSinceLastPipe = 0.f;
	bFirstPipeSpawned = false;

	// Reset difficulty settings.
	PipeSpeed = BasePipeSpeed;
	PipeGapSize = BasePipeGapSize;

	// Game has not "started" until the first tap.
	bGameStarted = false;
}

void ABabySharkGameMode::Flap()
{
	if (!bGameActive)
	{
		return;
	}

	// First flap starts the game.
	if (!bGameStarted)
	{
		bGameStarted = true;
	}

	// Apply upward velocity.
	SharkVelocity = FlapForce;
	OnSharkFlapped.Broadcast();
}

// ---------------------------------------------------------------------------
// Shark rect
// ---------------------------------------------------------------------------

FBox2D ABabySharkGameMode::GetSharkRect() const
{
	const float HalfSize = SharkSize * 0.5f;
	return FBox2D(
		FVector2D(SharkX - HalfSize, SharkY - HalfSize),
		FVector2D(SharkX + HalfSize, SharkY + HalfSize)
	);
}

void ABabySharkGameMode::GetPipeRects(const FPHCPipeObstacle& Pipe, FBox2D& OutTop, FBox2D& OutBottom) const
{
	const float HalfPipeW = PipeWidth * 0.5f;
	const float HalfGap = Pipe.GapHeight * 0.5f;
	const float GapTop = Pipe.GapCenterY - HalfGap;
	const float GapBottom = Pipe.GapCenterY + HalfGap;

	// Top pipe: from ceiling (Y=0) to the top of the gap.
	OutTop = FBox2D(
		FVector2D(Pipe.XPosition - HalfPipeW, 0.f),
		FVector2D(Pipe.XPosition + HalfPipeW, GapTop)
	);

	// Bottom pipe: from the bottom of the gap to the floor.
	OutBottom = FBox2D(
		FVector2D(Pipe.XPosition - HalfPipeW, GapBottom),
		FVector2D(Pipe.XPosition + HalfPipeW, FieldHeight)
	);
}

// ---------------------------------------------------------------------------
// Pipe management
// ---------------------------------------------------------------------------

void ABabySharkGameMode::SpawnPipe()
{
	FPHCPipeObstacle NewPipe;
	NewPipe.XPosition = FieldWidth + PipeWidth; // Start just off the right edge.
	NewPipe.GapHeight = PipeGapSize;
	NewPipe.bScored = false;

	// Random gap center within safe margins.
	const float Margin = PipeGapSize * 0.6f;
	NewPipe.GapCenterY = FMath::RandRange(Margin, FieldHeight - Margin);

	const int32 NewIndex = Pipes.Add(NewPipe);
	OnPipeSpawned.Broadcast(NewIndex);
}

void ABabySharkGameMode::UpdatePipes(float DT)
{
	const float MoveAmount = PipeSpeed * DT;

	for (int32 i = Pipes.Num() - 1; i >= 0; --i)
	{
		Pipes[i].XPosition -= MoveAmount;

		// Remove pipes that have scrolled off the left edge.
		if (Pipes[i].XPosition < -PipeWidth)
		{
			OnPipeRemoved.Broadcast(i);
			Pipes.RemoveAt(i);
		}
	}
}

// ---------------------------------------------------------------------------
// Collision detection
// ---------------------------------------------------------------------------

bool ABabySharkGameMode::CheckCollision() const
{
	const FBox2D SharkRect = GetSharkRect();

	// Check ceiling.
	if (SharkRect.Min.Y <= 0.f)
	{
		return true;
	}

	// Check floor.
	if (SharkRect.Max.Y >= FieldHeight)
	{
		return true;
	}

	// Check each pipe.
	for (const FPHCPipeObstacle& Pipe : Pipes)
	{
		FBox2D TopRect, BottomRect;
		GetPipeRects(Pipe, TopRect, BottomRect);

		// AABB overlap check with top pipe.
		if (SharkRect.Max.X > TopRect.Min.X &&
			SharkRect.Min.X < TopRect.Max.X &&
			SharkRect.Max.Y > TopRect.Min.Y &&
			SharkRect.Min.Y < TopRect.Max.Y)
		{
			return true;
		}

		// AABB overlap check with bottom pipe.
		if (SharkRect.Max.X > BottomRect.Min.X &&
			SharkRect.Min.X < BottomRect.Max.X &&
			SharkRect.Max.Y > BottomRect.Min.Y &&
			SharkRect.Min.Y < BottomRect.Max.Y)
		{
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Scoring
// ---------------------------------------------------------------------------

void ABabySharkGameMode::CheckScore()
{
	for (FPHCPipeObstacle& Pipe : Pipes)
	{
		if (!Pipe.bScored && Pipe.XPosition < SharkX)
		{
			Pipe.bScored = true;
			AddScore(1);

			// Check for difficulty increase every 10 points.
			if (CurrentScore % 10 == 0)
			{
				IncreaseDifficulty();
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Difficulty scaling
// ---------------------------------------------------------------------------

void ABabySharkGameMode::IncreaseDifficulty()
{
	// Increase pipe speed by 10%.
	PipeSpeed = FMath::Min(PipeSpeed * 1.10f, BasePipeSpeed * 3.f);

	// Shrink the gap by 8 units, with a minimum of 80.
	PipeGapSize = FMath::Max(PipeGapSize - 8.f, 80.f);
}
