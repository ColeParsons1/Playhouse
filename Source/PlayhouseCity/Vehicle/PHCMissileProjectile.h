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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Missile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Missile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Missile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Missile")
	float Damage = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Missile")
	float Speed = 8000.f;

	UPROPERTY(BlueprintReadWrite, Category = "PHC|Missile")
	TObjectPtr<AActor> OwnerVehicle;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
			   UPrimitiveComponent* OtherComp, FVector NormalImpulse,
			   const FHitResult& Hit);

	static constexpr float LifeSpan = 5.f;
};
