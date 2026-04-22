// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PHCNukeExplosion.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UPostProcessComponent;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	// -- Niagara flame jets (F_FreeFlameFx_Pack) — added on top of mesh effects --
	UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> FlameJets;

	// -- Heat distortion "melting" effect --
	UPROPERTY() TObjectPtr<UPostProcessComponent> HeatPostProcess;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeatHazeDome;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeatHazeMat;

	// -- Time dilation state (real-seconds tracked to survive self-slowdown) --
	double RealStartTime = 0.0;
	bool bTimeDilationRestored = true;
	static constexpr float TimeDilationPeak = 0.55f;   // 45% slowdown at peak
	static constexpr float TimeDilationHold = 0.6f;    // real seconds held at peak
	static constexpr float TimeDilationRamp = 0.9f;    // real seconds to ramp back

	float ElapsedTime = 0.f;
	static constexpr float TotalDuration = 8.f;
};
