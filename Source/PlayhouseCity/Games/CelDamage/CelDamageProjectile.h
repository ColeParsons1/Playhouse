// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CelDamageProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Projectile fired by a CelDamage vehicle weapon.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACelDamageProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACelDamageProjectile();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|CelDamage")
	float Damage = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|CelDamage")
	float Speed = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|CelDamage")
	float SplashRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|CelDamage")
	bool bIsHoming = false;

	UPROPERTY(BlueprintReadWrite, Category = "PHC|CelDamage")
	TObjectPtr<AActor> OwnerVehicle;

	UPROPERTY(BlueprintReadWrite, Category = "PHC|CelDamage")
	TObjectPtr<AActor> HomingTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|CelDamage")
	int32 SmackValue = 1;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
			   UPrimitiveComponent* OtherComp, FVector NormalImpulse,
			   const FHitResult& Hit);

	static constexpr float LifeSpan = 5.f;
};
