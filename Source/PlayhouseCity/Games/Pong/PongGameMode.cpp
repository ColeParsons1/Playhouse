// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Pong/PongGameMode.h"
#include "Games/Pong/PongHUD.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

APongGameMode::APongGameMode()
{
	GameType = EPHCGameType::Pong;
	GameHUDClass = UPongHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void APongGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void APongGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused || !bBallActive)
	{
		return;
	}

	UpdateBall(DeltaTime);
	CheckPaddleCollision();
	CheckGoal();
	UpdateCPU(DeltaTime);
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void APongGameMode::StartGame()
{
	Super::StartGame();

	PlayerScore = 0;
	CPUScore = 0;
	CurrentBallSpeed = BallSpeed;

	// Center both paddles vertically.
	PlayerPaddleY = FieldHeight * 0.5f;
	CPUPaddleY = FieldHeight * 0.5f;

	ServeBall();
}

void APongGameMode::ServeBall()
{
	// Place ball at center of the field.
	BallPosition = FVector2D(FieldWidth * 0.5f, FieldHeight * 0.5f);

	// Reset speed to the base value for each serve.
	CurrentBallSpeed = BallSpeed;

	// Choose a random direction -- ball always moves toward one player.
	const float AngleDeg = FMath::RandRange(-45.f, 45.f);
	const float AngleRad = FMath::DegreesToRadians(AngleDeg);

	// Randomly pick left (toward player) or right (toward CPU).
	const float DirectionX = FMath::RandBool() ? 1.f : -1.f;

	BallVelocity.X = FMath::Cos(AngleRad) * CurrentBallSpeed * DirectionX;
	BallVelocity.Y = FMath::Sin(AngleRad) * CurrentBallSpeed;

	bBallActive = true;
}

void APongGameMode::MovePlayerPaddle(float Direction)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const float HalfPaddle = PaddleHeight * 0.5f;

	PlayerPaddleY += Direction * PaddleSpeed * DeltaTime;
	PlayerPaddleY = FMath::Clamp(PlayerPaddleY, HalfPaddle, FieldHeight - HalfPaddle);
}

void APongGameMode::SetPlayerPaddleY(float NewY)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	const float HalfPaddle = PaddleHeight * 0.5f;
	PlayerPaddleY = FMath::Clamp(NewY, HalfPaddle, FieldHeight - HalfPaddle);
}

// ---------------------------------------------------------------------------
// Ball movement
// ---------------------------------------------------------------------------

void APongGameMode::UpdateBall(float DT)
{
	BallPosition += BallVelocity * DT;

	// Bounce off top wall.
	const float HalfBall = BallSize * 0.5f;
	if (BallPosition.Y - HalfBall <= 0.f)
	{
		BallPosition.Y = HalfBall;
		BallVelocity.Y = FMath::Abs(BallVelocity.Y);
	}

	// Bounce off bottom wall.
	if (BallPosition.Y + HalfBall >= FieldHeight)
	{
		BallPosition.Y = FieldHeight - HalfBall;
		BallVelocity.Y = -FMath::Abs(BallVelocity.Y);
	}
}

// ---------------------------------------------------------------------------
// Paddle collisions
// ---------------------------------------------------------------------------

void APongGameMode::CheckPaddleCollision()
{
	const float HalfBall = BallSize * 0.5f;
	const float HalfPaddleH = PaddleHeight * 0.5f;

	// --- Player paddle (left side, X near PaddleWidth) ---
	const float PlayerPaddleRight = PaddleWidth;
	if (BallVelocity.X < 0.f &&
		BallPosition.X - HalfBall <= PlayerPaddleRight &&
		BallPosition.X + HalfBall >= 0.f)
	{
		const float DistFromCenter = BallPosition.Y - PlayerPaddleY;
		if (FMath::Abs(DistFromCenter) <= HalfPaddleH + HalfBall)
		{
			// Reflect horizontally.
			BallPosition.X = PlayerPaddleRight + HalfBall;
			BallVelocity.X = FMath::Abs(BallVelocity.X);

			// Add vertical angle based on where the ball hit the paddle.
			// Normalized offset: -1 at top, +1 at bottom.
			const float NormalizedOffset = FMath::Clamp(DistFromCenter / HalfPaddleH, -1.f, 1.f);
			const float MaxBounceAngle = 60.f;
			const float BounceAngleRad = FMath::DegreesToRadians(NormalizedOffset * MaxBounceAngle);

			BallVelocity.X = FMath::Cos(BounceAngleRad) * CurrentBallSpeed;
			BallVelocity.Y = FMath::Sin(BounceAngleRad) * CurrentBallSpeed;

			IncreaseSpeed();
		}
	}

	// --- CPU paddle (right side, X near FieldWidth - PaddleWidth) ---
	const float CPUPaddleLeft = FieldWidth - PaddleWidth;
	if (BallVelocity.X > 0.f &&
		BallPosition.X + HalfBall >= CPUPaddleLeft &&
		BallPosition.X - HalfBall <= FieldWidth)
	{
		const float DistFromCenter = BallPosition.Y - CPUPaddleY;
		if (FMath::Abs(DistFromCenter) <= HalfPaddleH + HalfBall)
		{
			// Reflect horizontally.
			BallPosition.X = CPUPaddleLeft - HalfBall;
			BallVelocity.X = -FMath::Abs(BallVelocity.X);

			const float NormalizedOffset = FMath::Clamp(DistFromCenter / HalfPaddleH, -1.f, 1.f);
			const float MaxBounceAngle = 60.f;
			const float BounceAngleRad = FMath::DegreesToRadians(NormalizedOffset * MaxBounceAngle);

			BallVelocity.X = -FMath::Cos(BounceAngleRad) * CurrentBallSpeed;
			BallVelocity.Y = FMath::Sin(BounceAngleRad) * CurrentBallSpeed;

			IncreaseSpeed();
		}
	}
}

// ---------------------------------------------------------------------------
// Goal detection
// ---------------------------------------------------------------------------

void APongGameMode::CheckGoal()
{
	const float HalfBall = BallSize * 0.5f;

	// Ball passed the left edge -- CPU scores.
	if (BallPosition.X - HalfBall <= 0.f)
	{
		CPUScore++;
		bBallActive = false;

		if (CPUScore >= WinScore)
		{
			EndGame(/*bWon=*/ false);
		}
		else
		{
			ResetRound();
		}
		return;
	}

	// Ball passed the right edge -- Player scores.
	if (BallPosition.X + HalfBall >= FieldWidth)
	{
		PlayerScore++;
		AddScore(10); // Add to the base-class CurrentScore for reward calculation.
		bBallActive = false;

		if (PlayerScore >= WinScore)
		{
			EndGame(/*bWon=*/ true);
		}
		else
		{
			ResetRound();
		}
	}
}

// ---------------------------------------------------------------------------
// CPU AI
// ---------------------------------------------------------------------------

void APongGameMode::UpdateCPU(float DT)
{
	// The CPU tries to track the ball's Y position with imperfect reaction.
	const float TargetY = BallPosition.Y;
	const float Diff = TargetY - CPUPaddleY;

	// Apply reaction speed as a blend factor.
	const float EffectiveSpeed = PaddleSpeed * CPUReactionSpeed;
	const float MaxMove = EffectiveSpeed * DT;

	if (FMath::Abs(Diff) > 2.f) // small dead zone to avoid jittering
	{
		const float MoveAmount = FMath::Clamp(Diff, -MaxMove, MaxMove);
		CPUPaddleY += MoveAmount;
	}

	// Clamp within field bounds.
	const float HalfPaddle = PaddleHeight * 0.5f;
	CPUPaddleY = FMath::Clamp(CPUPaddleY, HalfPaddle, FieldHeight - HalfPaddle);
}

// ---------------------------------------------------------------------------
// Speed / round management
// ---------------------------------------------------------------------------

void APongGameMode::IncreaseSpeed()
{
	// Each rally exchange increases ball speed by 3%.
	CurrentBallSpeed *= 1.03f;

	// Re-normalize velocity to the new speed while keeping direction.
	const float CurrentSpeed = BallVelocity.Size();
	if (CurrentSpeed > KINDA_SMALL_NUMBER)
	{
		BallVelocity = (BallVelocity / CurrentSpeed) * CurrentBallSpeed;
	}
}

void APongGameMode::ResetRound()
{
	// Brief pause before re-serving (0.8 seconds).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ServeDelayHandle,
			this,
			&APongGameMode::ServeBall,
			0.8f,
			/*bLoop=*/ false
		);
	}
}
