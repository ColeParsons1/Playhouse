// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "PongGameMode.generated.h"

// ---------------------------------------------------------------------------
// APongGameMode
// ---------------------------------------------------------------------------

/**
 * Classic 2D Pong implementation driven entirely via tick-based movement.
 *
 * The game field, paddles, and ball are positioned using 2D coordinates
 * that the paired UPongHUD widget maps onto a UCanvasPanel. A simple
 * CPU opponent tracks the ball with a configurable reaction speed.
 *
 * Win condition: first to reach WinScore points.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APongGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	APongGameMode();

	// -- Lifecycle ------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Game flow ------------------------------------------------------------

	virtual void StartGame() override;

	/** Serve the ball from the center toward a random player. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pong")
	void ServeBall();

	/** Move the player paddle by Direction (-1 = up, +1 = down). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pong")
	void MovePlayerPaddle(float Direction);

	/** Directly set the player paddle Y position (e.g. from touch). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pong")
	void SetPlayerPaddleY(float NewY);

	// -- Field configuration --------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	float FieldWidth = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	float FieldHeight = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	float PaddleHeight = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	float PaddleWidth = 15.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	float BallSize = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	float PaddleSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	float BallSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong")
	int32 WinScore = 7;

	/** 0-1 scalar: how well the CPU tracks the ball (1 = perfect). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pong", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CPUReactionSpeed = 0.8f;

	// -- Runtime state (readable from HUD) ------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pong")
	FVector2D BallPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pong")
	FVector2D BallVelocity = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pong")
	float PlayerPaddleY = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pong")
	float CPUPaddleY = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pong")
	int32 PlayerScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pong")
	int32 CPUScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pong")
	bool bBallActive = false;

protected:
	// -- Internal helpers -----------------------------------------------------

	/** Move the ball and bounce off top/bottom walls. */
	void UpdateBall(float DT);

	/** Check and handle paddle collisions; add angle based on hit offset. */
	void CheckPaddleCollision();

	/** Check if the ball has passed the left or right edge (goal). */
	void CheckGoal();

	/** CPU paddle AI: tracks ball with imperfection and delay. */
	void UpdateCPU(float DT);

	/** Slightly increase ball speed after each rally exchange. */
	void IncreaseSpeed();

	/** After a goal, pause briefly then re-serve. */
	void ResetRound();

private:
	/** Current base speed of the ball (increases during rallies). */
	float CurrentBallSpeed = 0.f;

	/** Timer handle for the brief pause between goals and serves. */
	FTimerHandle ServeDelayHandle;
};
