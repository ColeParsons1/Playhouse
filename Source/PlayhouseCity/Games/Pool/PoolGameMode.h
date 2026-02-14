// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "Games/Pool/PoolBall.h"
#include "PoolGameMode.generated.h"

class APoolBall;
class APoolCue;
class APoolTable;

// ---------------------------------------------------------------------------
// APoolGameMode
// ---------------------------------------------------------------------------

/**
 * 8-ball pool game mode. Player vs CPU.
 *
 * Ball-type assignment follows real 8-ball rules: the first legally potted
 * ball after the break determines which player shoots solids and which
 * shoots stripes. The eight ball must be potted last; potting it before
 * clearing all assigned balls is a loss.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APoolGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	APoolGameMode();

	// -- Lifecycle --------------------------------------------------------

	virtual void StartGame() override;
	virtual void Tick(float DeltaTime) override;

	// -- Player actions ---------------------------------------------------

	/** Show the aiming guide line from the cue ball toward Direction. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void AimShot(FVector2D Direction, float Power);

	/** Fire the cue ball with the given world-space direction and power. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void TakeShot(FVector Direction, float Power);

	// -- Query ------------------------------------------------------------

	/** True if any ball on the table is still in motion. */
	UFUNCTION(BlueprintPure, Category = "PHC|Pool")
	bool AreBallsMoving() const;

	// -- State (read-only in Blueprint) -----------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	TArray<TObjectPtr<APoolBall>> Balls;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<APoolBall> CueBall;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<APoolCue> Cue;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<APoolTable> Table;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	bool bPlayerTurn = true;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	EPHCPoolBallType PlayerBallType = EPHCPoolBallType::Solid;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	EPHCPoolBallType CPUBallType = EPHCPoolBallType::Stripe;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	bool bBallTypeAssigned = false;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	bool bAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	TArray<int32> PlayerPotted;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	TArray<int32> CPUPotted;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	bool bBallsMoving = false;

private:
	// -- Internal ---------------------------------------------------------

	/** Spawn table, rack balls in triangle, place cue ball. */
	void SpawnTableAndBalls();

	/** Standard triangle rack positions centred at FootSpot. */
	TArray<FVector> GetRackPositions(FVector FootSpot, float BallDiameter) const;

	/** Called when all balls have stopped after a shot. */
	void OnAllBallsStopped();

	/** Evaluate what happened during this turn (scoring, fouls). */
	void EvaluateTurnResult();

	/** Check if a pocket event should assign ball types. */
	void CheckPocket(APoolBall* Ball);

	/** Check various foul conditions. */
	bool CheckFoul() const;

	/** Replace the cue ball after a scratch. */
	void ReplaceCueBall();

	/** Check if the game is over (8-ball potted). */
	void CheckGameOver();

	/** Simple CPU AI: pick a target ball, calculate angle, shoot. */
	void CPUTurn();

	/** Find the best target ball for the CPU. */
	APoolBall* FindBestCPUTarget() const;

	// Pocket callback
	UFUNCTION()
	void HandleBallPocketed(APoolBall* Ball);

	/** Balls potted during the current shot (reset each shot). */
	TArray<TObjectPtr<APoolBall>> ShotPottedBalls;

	/** Was the cue ball potted this shot? */
	bool bCueBallPottedThisShot = false;

	/** Did the shooter pot at least one of their own balls this shot? */
	bool bPottedOwnBallThisShot = false;

	/** Timer to allow CPU "thinking" delay. */
	float CPUThinkTimer = 0.f;
	bool bCPUThinking = false;
	static constexpr float CPUThinkDelay = 1.2f;

	/** Ball diameter for positioning. */
	static constexpr float BallDiameter = 5.7f;
};
