// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "PackCatchGameMode.generated.h"

// ---------------------------------------------------------------------------
// Supporting types
// ---------------------------------------------------------------------------

/** Type of falling package, affecting score and gameplay. */
UENUM(BlueprintType)
enum class EPHCPackageType : uint8
{
	Normal	UMETA(DisplayName = "Normal"),
	Bonus	UMETA(DisplayName = "Bonus"),
	Bomb	UMETA(DisplayName = "Bomb")
};

/** A single item falling down the screen. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCFallingItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PackCatch")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PackCatch")
	float Speed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PackCatch")
	EPHCPackageType Type = EPHCPackageType::Normal;

	/** Unique id for matching visuals on the HUD side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|PackCatch")
	int32 ItemId = 0;
};

// ---------------------------------------------------------------------------
// APackCatchGameMode
// ---------------------------------------------------------------------------

/**
 * Catch falling packages in a basket/tray.
 *
 * Normal packages score +10, Bonus packages score +50, and Bombs
 * cost the player a life. Missing a Normal package also costs a life.
 * The game ends when Lives reach zero.
 *
 * Difficulty ramps over time: spawn interval shrinks and fall speed
 * increases, keeping the player challenged.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APackCatchGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	APackCatchGameMode();

	// -- Lifecycle ------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Game flow ------------------------------------------------------------

	virtual void StartGame() override;
	virtual void EndGame(bool bWon) override;

	/** Move the catcher left/right by Direction (-1 or +1). */
	UFUNCTION(BlueprintCallable, Category = "PHC|PackCatch")
	void MovePlayer(float Direction);

	/** Directly set the catcher horizontal position (touch input). */
	UFUNCTION(BlueprintCallable, Category = "PHC|PackCatch")
	void SetPlayerPosition(float X);

	/** Current difficulty multiplier for UI display. */
	UFUNCTION(BlueprintPure, Category = "PHC|PackCatch")
	float GetDifficultyMultiplier() const;

	// -- Delegates ------------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemCaught, int32, ItemId, bool, bGoodCatch);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemMissed, int32, ItemId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemSpawned, const FPHCFallingItem&, Item);

	UPROPERTY(BlueprintAssignable, Category = "PHC|PackCatch")
	FOnItemCaught OnItemCaught;

	UPROPERTY(BlueprintAssignable, Category = "PHC|PackCatch")
	FOnItemMissed OnItemMissed;

	UPROPERTY(BlueprintAssignable, Category = "PHC|PackCatch")
	FOnItemSpawned OnItemSpawned;

	// -- Field configuration --------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float FieldWidth = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float FieldHeight = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float CatcherWidth = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float CatcherHeight = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float ItemSize = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float BaseSpawnInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float MinSpawnInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float BaseFallSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	int32 StartingLives = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|PackCatch")
	float CatcherMoveSpeed = 500.f;

	// -- Runtime state --------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|PackCatch")
	float PlayerX = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|PackCatch")
	TArray<FPHCFallingItem> FallingItems;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|PackCatch")
	int32 Lives = 3;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|PackCatch")
	float GameSpeed = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|PackCatch")
	float SpawnInterval = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|PackCatch")
	int32 ConsecutiveCatches = 0;

protected:
	// -- Internal helpers -----------------------------------------------------

	/** Spawn a new falling item at a random X with a random type. */
	UFUNCTION()
	void SpawnPackage();

	/** Check if any items overlap the catcher. */
	void CheckCatch();

	/** Check if any items have fallen below the screen. */
	void CheckMiss();

	/** Ramp up difficulty: shrink interval, increase speed. */
	void IncreaseDifficulty();

private:
	/** Timer that fires SpawnPackage periodically. */
	FTimerHandle SpawnTimer;

	/** Incrementing ID for new items. */
	int32 NextItemId = 0;

	/** Elapsed time used for difficulty ramping. */
	float ElapsedGameTime = 0.f;
};
