// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "BabySharkGameMode.generated.h"

// ---------------------------------------------------------------------------
// Supporting types
// ---------------------------------------------------------------------------

/** A single pipe (coral/seaweed) obstacle with a gap for the shark. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCPipeObstacle
{
	GENERATED_BODY()

	/** Horizontal position of the pipe (moves leftward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|BabyShark")
	float XPosition = 0.f;

	/** Vertical center of the gap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|BabyShark")
	float GapCenterY = 0.f;

	/** Height of the gap opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|BabyShark")
	float GapHeight = 150.f;

	/** Whether the player has already been awarded a point for this pipe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|BabyShark")
	bool bScored = false;
};

// ---------------------------------------------------------------------------
// ABabySharkGameMode
// ---------------------------------------------------------------------------

/**
 * Flappy Bird-style game with a shark swimming through coral obstacles.
 *
 * Gravity constantly pulls the shark downward. Tapping/clicking causes
 * a "flap" that gives upward velocity. Pipes scroll from right to left
 * with a gap the shark must navigate through. Colliding with a pipe,
 * the ceiling, or the floor ends the game.
 *
 * Difficulty increases every 10 points by shrinking the gap and
 * increasing pipe speed.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ABabySharkGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ABabySharkGameMode();

	// -- Lifecycle ------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Game flow ------------------------------------------------------------

	virtual void StartGame() override;

	/** Apply upward velocity ("flap"). */
	UFUNCTION(BlueprintCallable, Category = "PHC|BabyShark")
	void Flap();

	/** Get the bounding box of the shark in field coordinates. */
	UFUNCTION(BlueprintPure, Category = "PHC|BabyShark")
	FBox2D GetSharkRect() const;

	/** Get the top and bottom bounding boxes for a pipe obstacle. */
	void GetPipeRects(const FPHCPipeObstacle& Pipe, FBox2D& OutTop, FBox2D& OutBottom) const;

	// -- Delegates ------------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSharkFlapped);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSharkDied);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPipeSpawned, int32, PipeIndex);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPipeRemoved, int32, PipeIndex);

	UPROPERTY(BlueprintAssignable, Category = "PHC|BabyShark")
	FOnSharkFlapped OnSharkFlapped;

	UPROPERTY(BlueprintAssignable, Category = "PHC|BabyShark")
	FOnSharkDied OnSharkDied;

	UPROPERTY(BlueprintAssignable, Category = "PHC|BabyShark")
	FOnPipeSpawned OnPipeSpawned;

	UPROPERTY(BlueprintAssignable, Category = "PHC|BabyShark")
	FOnPipeRemoved OnPipeRemoved;

	// -- Field configuration --------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float FieldWidth = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float FieldHeight = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float Gravity = -800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float FlapForce = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float SharkX = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float SharkSize = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float BasePipeSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float PipeSpacing = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float BasePipeGapSize = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float PipeWidth = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|BabyShark")
	float MaxSharkVelocity = 600.f;

	// -- Runtime state --------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|BabyShark")
	float SharkY = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|BabyShark")
	float SharkVelocity = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|BabyShark")
	TArray<FPHCPipeObstacle> Pipes;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|BabyShark")
	float PipeSpeed = 200.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|BabyShark")
	float PipeGapSize = 150.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|BabyShark")
	bool bGameStarted = false;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|BabyShark")
	int32 BestScore = 0;

protected:
	// -- Internal helpers -----------------------------------------------------

	/** Spawn a new pipe pair at the right edge. */
	void SpawnPipe();

	/** Move all pipes left and remove off-screen ones. */
	void UpdatePipes(float DT);

	/** Check shark collision with pipes, ceiling, and floor. */
	bool CheckCollision() const;

	/** Award score for pipes the shark has passed. */
	void CheckScore();

	/** Shrink gap and increase speed every N points. */
	void IncreaseDifficulty();

private:
	/** Distance tracker for spacing pipe spawns. */
	float DistanceSinceLastPipe = 0.f;

	/** Whether the first pipe has been spawned (initial delay). */
	bool bFirstPipeSpawned = false;
};
