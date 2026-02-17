// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PHCMissileProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Missile projectile fired from the dune buggy's launcher.
 * Flies straight forward, deals damage on hit, and self-destructs after 5 seconds.
 */
UCLASS()
class PLAYHOUSECITY_API APHCMissileProjectile : public AActor
{
	GENERATED_BODY()

public:
	APHCMissileProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Enable a glowing trail of dots behind this projectile. */
	void StartTrail(const FLinearColor& Color);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Missile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Missile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Missile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Missile")
	float Damage = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Missile")
	float Speed = 15000.f;

	UPROPERTY(BlueprintReadWrite, Category = "PHC|Missile")
	TObjectPtr<AActor> OwnerVehicle;

	/** If set, spawn this actor class at impact location on hit / altitude detonation. */
	UPROPERTY()
	TSubclassOf<AActor> OnHitSpawnClass;

	/** If >= 0, detonate when altitude drops to this value (for gravity bombs). */
	float DetonateAtAltitude = -1.f;

	/** Manual gravity mode — bypasses ProjectileMovementComponent entirely. */
	bool bUseManualGravity = false;
	FVector ManualVelocity = FVector::ZeroVector;
	float ManualGravityScale = 1.f;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
			   UPrimitiveComponent* OtherComp, FVector NormalImpulse,
			   const FHitResult& Hit);

	static constexpr float LifeSpan = 1.5f;

	// -- Trail system --
	bool bHasTrail = false;
	FLinearColor TrailColor;
	float TrailTimer = 0.f;
	static constexpr float TrailInterval = 0.03f;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> TrailSphereMesh;

	void SpawnTrailDot();
};
