// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "TDGameMode.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Types of towers the player can build. */
UENUM(BlueprintType)
enum class EPHCTowerType : uint8
{
	Arrow		UMETA(DisplayName = "Arrow Tower"),
	Cannon		UMETA(DisplayName = "Cannon Tower"),
	Ice			UMETA(DisplayName = "Ice Tower"),
	Lightning	UMETA(DisplayName = "Lightning Tower")
};

/** Types of enemies that march along the path. */
UENUM(BlueprintType)
enum class EPHCEnemyType : uint8
{
	Basic	UMETA(DisplayName = "Basic"),
	Fast	UMETA(DisplayName = "Fast"),
	Tank	UMETA(DisplayName = "Tank"),
	Boss	UMETA(DisplayName = "Boss")
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** A single placed tower on the grid. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCTower
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	EPHCTowerType Type = EPHCTowerType::Arrow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	FIntPoint GridPos = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float Range = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float Damage = 10.f;

	/** Shots per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float FireRate = 1.f;

	/** Accumulator for fire timing. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	float FireTimer = 0.f;

	/** Index of the enemy currently being targeted (-1 = none). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	int32 TargetEnemyIndex = -1;
};

/** A single enemy marching along the path. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCEnemy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	EPHCEnemyType Type = EPHCEnemyType::Basic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float HP = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float MaxHP = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float Speed = 40.f;

	/** How far this enemy has traveled along the path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float DistanceTraveled = 0.f;

	/** Index into the EnemyPath waypoints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	int32 PathIndex = 0;

	/** Whether this enemy is currently slowed by an Ice tower. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	bool bSlowed = false;

	/** Remaining duration of the slow effect. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	float SlowTimer = 0.f;

	/** World position interpolated between waypoints for rendering. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	FVector2D WorldPosition = FVector2D::ZeroVector;

	/** Unique ID for tracking across frames. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	int32 UniqueId = 0;
};

/** Configuration for a wave of enemies. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCWaveConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	int32 EnemyCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	TArray<EPHCEnemyType> EnemyTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float SpawnInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float HPMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|TD")
	float SpeedMultiplier = 1.f;
};

// ---------------------------------------------------------------------------
// ATDGameMode
// ---------------------------------------------------------------------------

/**
 * Grid-based tower defense mini-game.
 *
 * The player places towers on a grid to defend against waves of enemies
 * that march along a predetermined path. Each tower type has unique
 * behaviour: Arrow (fast/low damage), Cannon (slow/splash), Ice (slows),
 * and Lightning (chain damage).
 *
 * Win: survive all waves. Lose: lives reach zero.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ATDGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ATDGameMode();

	// -- Lifecycle ------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Game flow ------------------------------------------------------------

	virtual void StartGame() override;

	/** Begin the next wave of enemies. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TD")
	void StartWave();

	// -- Tower management -----------------------------------------------------

	/**
	 * Place a tower at the given grid position.
	 * Returns true if placement was valid and gold was deducted.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|TD")
	bool PlaceTower(FIntPoint Pos, EPHCTowerType Type);

	/** Upgrade the tower at the given index. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TD")
	bool UpgradeTower(int32 TowerIndex);

	/** Sell the tower at the given index, refunding partial gold. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TD")
	void SellTower(int32 TowerIndex);

	/** Get the gold cost for a new tower of the specified type. */
	UFUNCTION(BlueprintPure, Category = "PHC|TD")
	int32 GetTowerCost(EPHCTowerType Type) const;

	/** Get the upgrade cost for a tower at the given index. */
	UFUNCTION(BlueprintPure, Category = "PHC|TD")
	int32 GetUpgradeCost(int32 TowerIndex) const;

	/** Get the sell refund for a tower at the given index. */
	UFUNCTION(BlueprintPure, Category = "PHC|TD")
	int32 GetSellRefund(int32 TowerIndex) const;

	/** Get the wave configuration for a given wave number. */
	UFUNCTION(BlueprintPure, Category = "PHC|TD")
	FPHCWaveConfig GetWaveConfig(int32 WaveNumber) const;

	/** Get the cell size used for world-space conversion. */
	UFUNCTION(BlueprintPure, Category = "PHC|TD")
	float GetCellSize() const { return CellSize; }

	// -- Grid -----------------------------------------------------------------

	/** Grid dimensions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TD")
	int32 GridWidth = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TD")
	int32 GridHeight = 8;

	/**
	 * 2D grid: 0 = empty (buildable), 1 = path, 2 = tower.
	 * Stored as a flat array accessed via [Y * GridWidth + X].
	 */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	TArray<int32> Grid;

	/** Ordered waypoints the enemies follow. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	TArray<FIntPoint> EnemyPath;

	// -- Towers & Enemies -----------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	TArray<FPHCTower> Towers;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	TArray<FPHCEnemy> Enemies;

	// -- Wave state ------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	int32 Wave = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TD")
	int32 MaxWaves = 20;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	bool bWaveActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	int32 EnemiesSpawned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	int32 EnemiesInWave = 0;

	// -- Economy & Lives ------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	int32 Gold = 100;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|TD")
	int32 Lives = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TD")
	int32 StartingGold = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TD")
	int32 StartingLives = 20;

protected:
	// -- Internal helpers -----------------------------------------------------

	/** Spawn a single enemy for the current wave. */
	void SpawnEnemy();

	/** Move all living enemies along the path. */
	void MoveEnemies(float DT);

	/** Update all towers: acquire targets and fire. */
	void UpdateTowers(float DT);

	/** Apply damage to an enemy. Handles death and splash. */
	void DamageEnemy(int32 EnemyIndex, float Damage, EPHCTowerType SourceType);

	/** Called when an enemy is killed. */
	void OnEnemyDeath(int32 Index);

	/** Called when an enemy reaches the end of the path. */
	void OnEnemyReachedEnd(int32 Index);

	/** Initialize the grid layout and path. */
	void SetupGrid();

	/** Check if the game is lost (lives <= 0). */
	void CheckGameOver();

	/** Find the nearest enemy within range of a tower. */
	int32 FindTargetForTower(const FPHCTower& Tower) const;

	/** Get the world position of a grid cell center. */
	FVector2D GridToWorld(FIntPoint Cell) const;

	/** Cell size for world-space conversion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|TD")
	float CellSize = 50.f;

private:
	/** Timer for spawning enemies during a wave. */
	FTimerHandle SpawnTimerHandle;

	/** Current wave config cached during active wave. */
	FPHCWaveConfig CurrentWaveConfig;

	/** Counter for unique enemy IDs. */
	int32 NextEnemyId = 1;

	/** Number of enemies killed in the current wave. */
	int32 EnemiesKilledInWave = 0;

	/** Splash damage radius for Cannon towers (in world units). */
	float CannonSplashRadius = 75.f;

	/** Number of chain targets for Lightning towers. */
	int32 LightningChainCount = 3;

	/** Slow effect duration from Ice towers. */
	float IceSlowDuration = 2.f;

	/** Slow effect speed multiplier. */
	float IceSlowMultiplier = 0.5f;
};
