// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Pool/PoolGameMode.h"
#include "Games/Pool/PoolHUD.h"
#include "Games/Pool/PoolBall.h"
#include "Games/Pool/PoolCue.h"
#include "Games/Pool/PoolTable.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------

APoolGameMode::APoolGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameType = EPHCGameType::Pool;
	GameHUDClass = UPoolHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void APoolGameMode::StartGame()
{
	Super::StartGame();

	// Reset state
	Balls.Empty();
	PlayerPotted.Empty();
	CPUPotted.Empty();
	ShotPottedBalls.Empty();
	bPlayerTurn = true;
	bBallTypeAssigned = false;
	bBallsMoving = false;
	bAiming = false;
	bCueBallPottedThisShot = false;
	bPottedOwnBallThisShot = false;
	bCPUThinking = false;

	SpawnTableAndBalls();
}

// ---------------------------------------------------------------------------

void APoolGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive)
	{
		return;
	}

	// CPU "thinking" delay
	if (bCPUThinking)
	{
		CPUThinkTimer += DeltaTime;
		if (CPUThinkTimer >= CPUThinkDelay)
		{
			bCPUThinking = false;
			CPUThinkTimer = 0.f;
			CPUTurn();
		}
		return;
	}

	// Check if balls have settled
	const bool bMovingNow = AreBallsMoving();
	if (bBallsMoving && !bMovingNow)
	{
		bBallsMoving = false;
		OnAllBallsStopped();
	}
	bBallsMoving = bMovingNow;
}

// ---------------------------------------------------------------------------
// Player actions
// ---------------------------------------------------------------------------

void APoolGameMode::AimShot(FVector2D Direction, float Power)
{
	if (!bPlayerTurn || bBallsMoving || !CueBall || !Cue)
	{
		return;
	}

	bAiming = true;
	const FVector Dir3D = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();

	Cue->SetActorLocation(CueBall->GetActorLocation());
	Cue->SetAimDirection(-Dir3D);
	Cue->SetPowerLevel(Power);
	Cue->ShowAimLine(CueBall->GetActorLocation(), Dir3D);
}

// ---------------------------------------------------------------------------

void APoolGameMode::TakeShot(FVector Direction, float Power)
{
	if (!CueBall || bBallsMoving)
	{
		return;
	}

	// Reset per-shot tracking
	ShotPottedBalls.Empty();
	bCueBallPottedThisShot = false;
	bPottedOwnBallThisShot = false;

	const FVector Impulse = Direction.GetSafeNormal() * FMath::Clamp(Power, 0.f, 1.f) * 600.f;
	if (CueBall->CollisionSphere)
	{
		CueBall->CollisionSphere->AddImpulse(Impulse, NAME_None, true);
	}

	bBallsMoving = true;
	bAiming = false;

	if (Cue)
	{
		Cue->PlayStrikeAnimation();
	}
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

bool APoolGameMode::AreBallsMoving() const
{
	for (const TObjectPtr<APoolBall>& Ball : Balls)
	{
		if (Ball && Ball->IsMoving())
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void APoolGameMode::SpawnTableAndBalls()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn table
	const FVector TableOrigin = FVector::ZeroVector;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Table = World->SpawnActor<APoolTable>(APoolTable::StaticClass(), TableOrigin, FRotator::ZeroRotator, SpawnParams);

	if (Table)
	{
		Table->OnBallPocketed.AddDynamic(this, &APoolGameMode::HandleBallPocketed);
	}

	// Foot spot: one quarter of the way from the right end
	const float FootSpotX = Table ? Table->TableLength * 0.25f : 63.5f;
	const FVector FootSpot = TableOrigin + FVector(FootSpotX, 0.f, BallDiameter * 0.5f + 1.f);

	// Rack the 15 object balls
	TArray<FVector> RackPos = GetRackPositions(FootSpot, BallDiameter);

	// Standard 8-ball rack order (front to back, left to right per row):
	// Row1: 1, Row2: 9,2, Row3: 3,8,10, Row4: 11,4,5,14, Row5: 6,13,12,15,7
	const TArray<int32> RackOrder = { 1, 9, 2, 3, 8, 10, 11, 4, 5, 14, 6, 13, 12, 15, 7 };

	for (int32 i = 0; i < RackOrder.Num() && i < RackPos.Num(); ++i)
	{
		APoolBall* Ball = World->SpawnActor<APoolBall>(APoolBall::StaticClass(), RackPos[i], FRotator::ZeroRotator, SpawnParams);
		if (Ball)
		{
			Ball->InitBall(RackOrder[i]);
			Balls.Add(Ball);
		}
	}

	// Cue ball: one quarter from the left end
	const float CueBallX = Table ? -Table->TableLength * 0.25f : -63.5f;
	const FVector CuePos = TableOrigin + FVector(CueBallX, 0.f, BallDiameter * 0.5f + 1.f);
	CueBall = World->SpawnActor<APoolBall>(APoolBall::StaticClass(), CuePos, FRotator::ZeroRotator, SpawnParams);
	if (CueBall)
	{
		CueBall->InitBall(0);
		Balls.Add(CueBall);
	}

	// Cue stick
	Cue = World->SpawnActor<APoolCue>(APoolCue::StaticClass(), CuePos + FVector(-20.f, 0.f, 0.f), FRotator::ZeroRotator, SpawnParams);
}

// ---------------------------------------------------------------------------

TArray<FVector> APoolGameMode::GetRackPositions(FVector FootSpot, float Diam) const
{
	TArray<FVector> Positions;
	const float RowOffset = Diam * 0.866f; // sqrt(3)/2
	int32 Index = 0;

	for (int32 Row = 0; Row < 5; ++Row)
	{
		const int32 BallsInRow = Row + 1;
		const float StartY = -(BallsInRow - 1) * Diam * 0.5f;

		for (int32 Col = 0; Col < BallsInRow; ++Col)
		{
			FVector Pos = FootSpot;
			Pos.X += Row * RowOffset;
			Pos.Y += StartY + Col * Diam;
			// Small random jitter to make break more natural
			Pos.Y += FMath::FRandRange(-0.05f, 0.05f);
			Positions.Add(Pos);
			++Index;
		}
	}

	return Positions;
}

// ---------------------------------------------------------------------------
// Pocket handling
// ---------------------------------------------------------------------------

void APoolGameMode::HandleBallPocketed(APoolBall* Ball)
{
	if (!Ball || Ball->bPotted)
	{
		return;
	}

	ShotPottedBalls.Add(Ball);
	CheckPocket(Ball);
}

// ---------------------------------------------------------------------------

void APoolGameMode::CheckPocket(APoolBall* Ball)
{
	if (!Ball)
	{
		return;
	}

	// Cue ball scratched
	if (Ball->BallType == EPHCPoolBallType::Cue)
	{
		bCueBallPottedThisShot = true;
		Ball->OnPotted();
		return;
	}

	// Assign ball types on first legal pot
	if (!bBallTypeAssigned && Ball->BallType != EPHCPoolBallType::Eight)
	{
		if (bPlayerTurn)
		{
			PlayerBallType = Ball->BallType;
			CPUBallType = (Ball->BallType == EPHCPoolBallType::Solid)
				? EPHCPoolBallType::Stripe
				: EPHCPoolBallType::Solid;
		}
		else
		{
			CPUBallType = Ball->BallType;
			PlayerBallType = (Ball->BallType == EPHCPoolBallType::Solid)
				? EPHCPoolBallType::Stripe
				: EPHCPoolBallType::Solid;
		}
		bBallTypeAssigned = true;
	}

	Ball->OnPotted();

	// Track which side potted
	const EPHCPoolBallType CurrentBallType = bPlayerTurn ? PlayerBallType : CPUBallType;
	if (Ball->BallType == CurrentBallType)
	{
		bPottedOwnBallThisShot = true;
		if (bPlayerTurn)
		{
			PlayerPotted.Add(Ball->BallNumber);
		}
		else
		{
			CPUPotted.Add(Ball->BallNumber);
		}
	}
	else if (Ball->BallType == EPHCPoolBallType::Eight)
	{
		// 8-ball potted: handled in CheckGameOver
	}
	else
	{
		// Opponent's ball potted (counts for opponent)
		if (bPlayerTurn)
		{
			CPUPotted.Add(Ball->BallNumber);
		}
		else
		{
			PlayerPotted.Add(Ball->BallNumber);
		}
	}

	AddScore(10);
}

// ---------------------------------------------------------------------------
// Turn evaluation
// ---------------------------------------------------------------------------

void APoolGameMode::OnAllBallsStopped()
{
	// Check for game-over first (8-ball potted)
	CheckGameOver();
	if (!bGameActive)
	{
		return;
	}

	EvaluateTurnResult();
}

// ---------------------------------------------------------------------------

void APoolGameMode::EvaluateTurnResult()
{
	// Handle cue-ball scratch
	if (bCueBallPottedThisShot)
	{
		ReplaceCueBall();
		// Turn passes to the other player
		bPlayerTurn = !bPlayerTurn;
	}
	else if (!bPottedOwnBallThisShot)
	{
		// Didn't pot any of own balls: switch turns
		bPlayerTurn = !bPlayerTurn;
	}
	// else: potted own ball without scratching -> same player continues

	// If it is now CPU's turn, start the think timer
	if (!bPlayerTurn && bGameActive)
	{
		bCPUThinking = true;
		CPUThinkTimer = 0.f;
	}
}

// ---------------------------------------------------------------------------

bool APoolGameMode::CheckFoul() const
{
	// Cue ball potted
	if (bCueBallPottedThisShot)
	{
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------

void APoolGameMode::ReplaceCueBall()
{
	if (!CueBall)
	{
		return;
	}

	// Restore the cue ball behind the head string
	const float CueBallX = Table ? -Table->TableLength * 0.25f : -63.5f;
	const FVector CuePos = FVector(CueBallX, 0.f, BallDiameter * 0.5f + 1.f);

	CueBall->bPotted = false;
	CueBall->SetActorHiddenInGame(false);
	CueBall->SetActorEnableCollision(true);
	CueBall->SetActorLocation(CuePos);

	if (CueBall->CollisionSphere)
	{
		CueBall->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CueBall->CollisionSphere->SetSimulatePhysics(true);
		CueBall->CollisionSphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
		CueBall->CollisionSphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

// ---------------------------------------------------------------------------

void APoolGameMode::CheckGameOver()
{
	// Find if the 8-ball was potted this shot
	bool bEightPotted = false;
	for (const TObjectPtr<APoolBall>& PB : ShotPottedBalls)
	{
		if (PB && PB->BallType == EPHCPoolBallType::Eight)
		{
			bEightPotted = true;
			break;
		}
	}

	if (!bEightPotted)
	{
		return;
	}

	// Determine if the shooter cleared all their own balls first
	const int32 RequiredCount = 7; // 7 solids or 7 stripes
	bool bShooterCleared = false;

	if (bPlayerTurn)
	{
		bShooterCleared = (PlayerPotted.Num() >= RequiredCount);
	}
	else
	{
		bShooterCleared = (CPUPotted.Num() >= RequiredCount);
	}

	// Also a loss if the cue ball was potted alongside the 8-ball
	if (bCueBallPottedThisShot)
	{
		bShooterCleared = false;
	}

	const bool bPlayerWon = bPlayerTurn ? bShooterCleared : !bShooterCleared;
	EndGame(bPlayerWon);
}

// ---------------------------------------------------------------------------
// CPU AI
// ---------------------------------------------------------------------------

void APoolGameMode::CPUTurn()
{
	if (!CueBall || CueBall->bPotted)
	{
		// Should not happen, but guard
		bPlayerTurn = true;
		return;
	}

	APoolBall* Target = FindBestCPUTarget();
	if (!Target)
	{
		// No valid target: just hit the cue ball weakly forward
		TakeShot(FVector(1.f, 0.f, 0.f), 0.3f);
		return;
	}

	// Direction: cue ball toward target
	FVector ToTarget = (Target->GetActorLocation() - CueBall->GetActorLocation());
	ToTarget.Z = 0.f;
	FVector Dir = ToTarget.GetSafeNormal();

	// Add some inaccuracy
	const float Inaccuracy = FMath::FRandRange(-0.08f, 0.08f);
	Dir = Dir.RotateAngleAxis(FMath::RadiansToDegrees(Inaccuracy), FVector::UpVector);

	// Power proportional to distance, with some randomness
	const float Dist = ToTarget.Size();
	const float BasePower = FMath::Clamp(Dist / 200.f, 0.25f, 0.85f);
	const float Power = BasePower + FMath::FRandRange(-0.05f, 0.1f);

	TakeShot(Dir, FMath::Clamp(Power, 0.2f, 1.f));
}

// ---------------------------------------------------------------------------

APoolBall* APoolGameMode::FindBestCPUTarget() const
{
	if (!CueBall)
	{
		return nullptr;
	}

	APoolBall* BestBall = nullptr;
	float BestScore = -1.f;

	const FVector CuePos = CueBall->GetActorLocation();

	for (const TObjectPtr<APoolBall>& Ball : Balls)
	{
		if (!Ball || Ball->bPotted || Ball == CueBall)
		{
			continue;
		}

		// Only consider CPU's assigned balls (or any if not yet assigned)
		if (bBallTypeAssigned && Ball->BallType != CPUBallType)
		{
			// Allow 8-ball if all CPU balls are potted
			if (Ball->BallType == EPHCPoolBallType::Eight && CPUPotted.Num() >= 7)
			{
				// OK
			}
			else
			{
				continue;
			}
		}

		const float Dist = FVector::Dist(CuePos, Ball->GetActorLocation());
		// Prefer closer balls (inverse distance scoring)
		const float Score = 1000.f / FMath::Max(Dist, 1.f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestBall = Ball.Get();
		}
	}

	return BestBall;
}
