// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "AngryDucksGameMode.generated.h"

class ADuckProjectile;
class ADestructibleBlock;
class APigTarget;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDuckLaunched, ADuckProjectile*, Duck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPigDestroyed, APigTarget*, Pig);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelCleared, int32, Level);

// ---------------------------------------------------------------------------
// AAngryDucksGameMode
// ---------------------------------------------------------------------------

/**
 * Game mode for the Angry Ducks mini-game.
 *
 * The player uses a slingshot to launch duck projectiles at structures
 * containing pig targets. Three levels of increasing difficulty, each
 * with a limited number of ducks.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AAngryDucksGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AAngryDucksGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void StartGame() override;

	// -- Player input -----------------------------------------------------

	/** Launch a duck from the slingshot in the given direction with power. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void LaunchDuck(FVector2D Direction, float Power);

	/** Set the current aim direction (normalised) from HUD input. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void SetAimDirection(FVector2D Direction);

	/** Return the current aim direction. */
	UFUNCTION(BlueprintPure, Category = "PHC|AngryDucks")
	FVector2D GetAimDirection() const { return AimDirection; }

	/** Advance to the next level (called when all pigs are destroyed). */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void NextLevel();

	// -- Arena config -----------------------------------------------------

	/** World-space position of the slingshot anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|AngryDucks")
	FVector SlingPosition = FVector(200.f, 0.f, 200.f);

	/** Maximum launch power scalar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|AngryDucks")
	float LaunchPower = 1500.f;

	/** Maximum number of ducks the player can have per level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|AngryDucks")
	int32 MaxDucks = 5;

	/** Total number of levels in a single round. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|AngryDucks")
	int32 TotalLevels = 3;

	// -- Runtime state ----------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	int32 CurrentLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	int32 DucksRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	int32 PigsRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	bool bReadyToLaunch = true;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	TArray<TObjectPtr<ADestructibleBlock>> Blocks;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	TArray<TObjectPtr<APigTarget>> Pigs;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	TObjectPtr<ADuckProjectile> ActiveDuck;

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|AngryDucks")
	FOnDuckLaunched OnDuckLaunched;

	UPROPERTY(BlueprintAssignable, Category = "PHC|AngryDucks")
	FOnPigDestroyed OnPigDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "PHC|AngryDucks")
	FOnLevelCleared OnLevelCleared;

protected:
	/** Spawn the block/pig layout for the given level index (1-based). */
	void SpawnLevel(int32 Level);

	/** Clear all blocks and pigs from the scene. */
	void ClearLevel();

	/** Spawn a duck on the slingshot, ready to launch. */
	void PrepareDuck();

	/** Check if the round is over (all pigs dead or out of ducks). */
	void CheckRoundState();

	UFUNCTION()
	void OnBlockDestroyed(ADestructibleBlock* Block);

	UFUNCTION()
	void OnPigKilled(APigTarget* Pig);

	UFUNCTION()
	void OnDuckSettled(ADuckProjectile* Duck);

private:
	/** Current aim direction set by the HUD. */
	FVector2D AimDirection = FVector2D(1.f, 1.f);

	/** Delay timer after a duck settles before allowing the next launch. */
	float SettleDelayTimer = 0.f;

	/** True while waiting for the settle delay to expire. */
	bool bWaitingAfterSettle = false;

	/** Time to wait after a duck settles before preparing the next one. */
	static constexpr float SettleDelay = 1.2f;
};
