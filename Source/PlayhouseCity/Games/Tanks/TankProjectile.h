// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

// ---------------------------------------------------------------------------
// ATankProjectile
// ---------------------------------------------------------------------------

/**
 * Projectile fired from a tank turret.
 * Travels in a straight line, dealing damage to tanks and destroying
 * terrain on impact.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ATankProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATankProjectile();

	virtual void BeginPlay() override;

	// -- Properties -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Damage dealt on hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Tanks")
	float Damage = 1.f;

	/** Projectile speed in cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Tanks")
	float Speed = 800.f;

	/** Owning tank (to avoid self-damage). */
	UPROPERTY(BlueprintReadWrite, Category = "PHC|Tanks")
	TObjectPtr<AActor> OwnerTank;

private:
	UFUNCTION()
	void OnHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	/** Auto-destroy after this many seconds. */
	static constexpr float LifeSpan = 4.f;
};
