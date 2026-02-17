// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "CelDamageGameMode.generated.h"

class ACelDamageVehicle;
class ACelDamageWeapon;

/**
 * Game mode for the Cel Damage vehicular combat mini-game.
 * 1v1 Smack Attack (deathmatch) mode with weapon pickups.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACelDamageGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ACelDamageGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void StartGame() override;
	virtual void EndGame(bool bWon) override;

	// -- Player input proxies -----------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void PlayerThrottle(float Value);

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void PlayerSteer(float Value);

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void PlayerFire();

	// -- Arena configuration ------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|CelDamage")
	float ArenaSize = 8000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|CelDamage")
	float WallHeight = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|CelDamage")
	int32 NumWeaponSpawns = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|CelDamage")
	int32 ScoreLimit = 10;

	// -- Runtime state ------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<ACelDamageVehicle> PlayerVehicle;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<ACelDamageVehicle> AIVehicle;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	TArray<TObjectPtr<ACelDamageWeapon>> WeaponPickups;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	int32 PlayerSmacks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	int32 AISmacks = 0;

	TArray<FVector> RespawnPoints;

protected:
	void SpawnArena();
	void SpawnVehicles();
	void SpawnWeaponPickups();
	void HandleRespawns(float DT);

	UFUNCTION()
	void OnPlayerVehicleDestroyed(ACelDamageVehicle* Vehicle);

	UFUNCTION()
	void OnAIVehicleDestroyed(ACelDamageVehicle* Vehicle);

	UFUNCTION()
	void OnVehicleHit(ACelDamageVehicle* Attacker, ACelDamageVehicle* Victim);

private:
	/** Arena offset to avoid city overlap. */
	static constexpr float ArenaOffsetX = 100000.f;
	static constexpr float ArenaOffsetY = 100000.f;

	FVector ArenaCenter() const { return FVector(ArenaOffsetX, ArenaOffsetY, 0.f); }

	UPROPERTY()
	TArray<TObjectPtr<AActor>> ArenaActors;

	AActor* SpawnArenaBlock(const FVector& Location, const FVector& Scale,
							const FRotator& Rotation, const FLinearColor& Color);

	FVector GetSafeRespawnPoint(const ACelDamageVehicle* AwayFrom) const;

	bool bPlayerNeedsRespawn = false;
	bool bAINeedsRespawn = false;
	float PlayerRespawnTimer = 0.f;
	float AIRespawnTimer = 0.f;
	static constexpr float RespawnDelay = 1.5f;
};
