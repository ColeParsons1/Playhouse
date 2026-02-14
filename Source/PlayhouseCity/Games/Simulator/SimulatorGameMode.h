// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "SimulatorGameMode.generated.h"

class USimulatorHUD;

/** Obstacle on the road. */
USTRUCT(BlueprintType)
struct FDrivingObstacle
{
	GENERATED_BODY()

	UPROPERTY() int32 Lane = 0;       // 0=left, 1=center, 2=right
	UPROPERTY() float PositionY = 0.f; // 0..1 from top to bottom
	UPROPERTY() int32 Type = 0;        // 0=car, 1=barrier, 2=coin
	UPROPERTY() bool bActive = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDrivingCrash);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoinCollected, int32, TotalCoins);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeedChanged, float, NewSpeed);

UCLASS(Blueprintable)
class PLAYHOUSECITY_API ASimulatorGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ASimulatorGameMode();

	virtual void Tick(float DeltaTime) override;
	virtual void StartGame() override;
	virtual void EndGame(bool bWon) override;

	/** Move the player car left or right. Direction: -1=left, +1=right */
	UFUNCTION(BlueprintCallable, Category = "PHC|Driving")
	void ChangeLane(int32 Direction);

	// -- State --
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Driving")
	int32 PlayerLane = 1; // 0, 1, 2 (center start)

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Driving")
	float CurrentSpeed = 200.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Driving")
	float DistanceTraveled = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Driving")
	int32 CoinsCollected = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Driving")
	TArray<FDrivingObstacle> Obstacles;

	// -- Config --
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Driving")
	float StartSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Driving")
	float MaxSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Driving")
	float SpeedIncreaseRate = 8.f; // Per second

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Driving")
	float ObstacleSpawnInterval = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Driving")
	float CoinSpawnChance = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Driving")
	int32 NumLanes = 3;

	// -- Delegates --
	UPROPERTY(BlueprintAssignable, Category = "PHC|Driving")
	FOnDrivingCrash OnCrash;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Driving")
	FOnCoinCollected OnCoinCollected;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Driving")
	FOnSpeedChanged OnSpeedChanged;

private:
	void SpawnObstacle();
	void UpdateObstacles(float DeltaTime);
	void CheckCollisions();

	float SpawnAccumulator = 0.f;
	float RoadOffset = 0.f; // For visual scrolling
};
