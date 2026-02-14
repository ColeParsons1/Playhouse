// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "TanksGameMode.generated.h"

class ATankPawn;
class ADestructibleWall;

UCLASS(Blueprintable)
class PLAYHOUSECITY_API ATanksGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ATanksGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void StartGame() override;

	// -- Player input proxies ------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void MovePlayerForward(float Value);

	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void TurnPlayer(float Value);

	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void AimPlayerTurret(FVector2D WorldTarget);

	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void PlayerFire();

	// -- Arena config --------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Tanks")
	float ArenaWidth = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Tanks")
	float ArenaHeight = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Tanks")
	int32 WallCount = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Tanks")
	int32 NumCPUTanks = 1;

	// -- Runtime state -------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<ATankPawn> PlayerTank;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Tanks")
	TArray<TObjectPtr<ATankPawn>> CPUTanks;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Tanks")
	TArray<TObjectPtr<ADestructibleWall>> Walls;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Tanks")
	int32 CPUTanksRemaining = 0;

protected:
	void SpawnArena();
	void SpawnTanks();
	void SpawnWalls();

	UFUNCTION()
	void OnPlayerTankDestroyed(ATankPawn* Tank);

	UFUNCTION()
	void OnCPUTankDestroyed(ATankPawn* Tank);
};
