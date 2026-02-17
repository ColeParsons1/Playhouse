// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "TempleRunGameMode.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Types of obstacles the runner can encounter on the track. */
UENUM(BlueprintType)
enum class EPHCObstacleType : uint8
{
	Low			UMETA(DisplayName = "Low (Jump Over)"),
	High		UMETA(DisplayName = "High (Slide Under)"),
	BlockLeft	UMETA(DisplayName = "Block Left Lane"),
	BlockCenter	UMETA(DisplayName = "Block Center Lane"),
	BlockRight	UMETA(DisplayName = "Block Right Lane")
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** A single obstacle placed within a runner tile. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCRunnerObstacle
{
	GENERATED_BODY()

	/** Which lane (0=left, 1=center, 2=right) the obstacle occupies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TempleRun")
	int32 Lane = 1;

	/** The type of obstacle determining how the player must dodge it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TempleRun")
	EPHCObstacleType Type = EPHCObstacleType::Low;
};

/** A tile segment on the endless runner track. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCRunnerTile
{
	GENERATED_BODY()

	/** Z position (distance along the track) of this tile's front edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TempleRun")
	float ZPosition = 0.f;

	/** Obstacles placed on this tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TempleRun")
	TArray<FPHCRunnerObstacle> Obstacles;

	/** Lanes that have a coin pickup on this tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TempleRun")
	TArray<int32> CoinLanes;

	/** Whether coins on this tile have already been collected. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	TArray<bool> CoinCollected;
};

// ---------------------------------------------------------------------------
// ATempleRunGameMode
// ---------------------------------------------------------------------------

/**
 * 3-lane endless runner mini-game.
 *
 * The player runs forward automatically at an ever-increasing speed,
 * swiping left/right to change lanes, up to jump, and down to slide.
 * Obstacles require the correct action to avoid; hitting one ends the run.
 * Coins scattered across lanes award bonus score.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ATempleRunGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ATempleRunGameMode();

	// -- Lifecycle ------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Game flow ------------------------------------------------------------

	virtual void StartGame() override;

	// -- Player actions -------------------------------------------------------

	/** Move the runner one lane left (Direction = -1) or right (+1). */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRun")
	void SwipeLane(int32 Direction);

	/** Start a jump if the runner is grounded. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRun")
	void Jump();

	/** Start a slide if the runner is grounded. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRun")
	void Slide();

	// -- Tile management ------------------------------------------------------

	/** Spawn a new tile at the end of the track. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRun")
	void SpawnTile();

	// -- Configuration --------------------------------------------------------

	/** Length of each tile segment in distance units. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TempleRun")
	float TileLength = 500.f;

	/** Starting run speed in units per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TempleRun")
	float BaseRunSpeed = 600.f;

	/** Duration of the jump arc in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TempleRun")
	float JumpDuration = 0.6f;

	/** Duration of the slide in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TempleRun")
	float SlideDuration = 0.5f;

	/** Number of initial tiles spawned at game start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TempleRun")
	int32 InitialTileCount = 10;

	/** Speed multiplier applied per second of gameplay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TempleRun")
	float SpeedIncreaseRate = 5.f;

	/** Maximum run speed cap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TempleRun")
	float MaxRunSpeed = 1500.f;

	// -- Runtime state --------------------------------------------------------

	/** Current lane index: 0 = left, 1 = center, 2 = right. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	int32 CurrentLane = 1;

	/** Current forward speed in units per second. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	float RunSpeed = 600.f;

	/** Total distance traveled this run. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	float DistanceTraveled = 0.f;

	/** Currently active tiles forming the visible track. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	TArray<FPHCRunnerTile> ActiveTiles;

	/** True while the runner is in the air. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	bool bIsJumping = false;

	/** True while the runner is sliding. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	bool bIsSliding = false;

	/** Remaining time in the current jump. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	float JumpTimer = 0.f;

	/** Remaining time in the current slide. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	float SlideTimer = 0.f;

	/** Total coins collected this run. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TempleRun")
	int32 CoinsCollected = 0;

protected:
	// -- Internal helpers -----------------------------------------------------

	/** Check whether the runner has collided with any obstacle on the current tiles. */
	void CheckCollision();

	/** Collect a coin in the runner's lane on the given tile. */
	void CollectCoin(int32 TileIndex, int32 CoinIndex);

	/** Called when the runner hits an obstacle - ends the game. */
	void OnHitObstacle();

	/** Gradually ramp speed and obstacle density. */
	void IncreaseDifficulty(float DeltaTime);

	/** Remove tiles that have scrolled behind the player. */
	void CleanupPassedTiles();

	/**
	 * Validates obstacle placement so there is always at least one safe lane
	 * or a jumpable/slideable path.
	 */
	bool ValidateTilePlayability(const FPHCRunnerTile& Tile) const;

private:
	/** Tracks the distance at which the next tile should be spawned. */
	float SpawnDistance = 0.f;

	/** The runner's distance position within the current tile (for collision checks). */
	float PlayerTileOffset = 0.f;

	/** Running difficulty scalar (increases over time). */
	float DifficultyScalar = 1.f;
};
