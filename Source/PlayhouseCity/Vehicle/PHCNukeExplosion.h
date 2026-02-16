// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PHCNukeExplosion.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 * Large dynamite explosion effect built from basic shapes.
 * Spawned when a Raptor bomb hits the ground.
 * Animates a fireball, flame bursts, smoke plume, debris ring,
 * shockwave, and flying ember particles.
 */
UCLASS()
class PLAYHOUSECITY_API APHCNukeExplosion : public AActor
{
	GENERATED_BODY()

public:
	APHCNukeExplosion();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	// -- Core fireball --
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FireballCore;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FireballCoreMat;

	// -- Flame burst lobes (3 offset fireballs) --
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FlameBurstA;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FlameBurstB;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FlameBurstC;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlameMatA;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlameMatB;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlameMatC;

	// -- Smoke --
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SmokePlume;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SmokeCloudA;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SmokeCloudB;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SmokePlumeMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SmokeCloudAMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SmokeCloudBMat;

	// -- Ground effects --
	UPROPERTY() TObjectPtr<UStaticMeshComponent> DebrisRing;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ShockwaveRing;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DebrisRingMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShockwaveRingMat;

	// -- Light --
	UPROPERTY() TObjectPtr<UPointLightComponent> ExplosionLight;

	// -- Ember particles (spawned at runtime) --
	static constexpr int32 NumEmbers = 10;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Embers;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> EmberMats;
	TArray<FVector> EmberVelocities;
	TArray<FVector> EmberPositions;

	float ElapsedTime = 0.f;
	static constexpr float TotalDuration = 5.f;
};
