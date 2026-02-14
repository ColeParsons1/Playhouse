// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Games/CelDamage/CelDamageVehicle.h"
#include "CelDamageWeapon.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * Floating weapon pickup for the Cel Damage mini-game.
 * Bobs up/down and rotates. Grants a weapon on collection, then respawns.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACelDamageWeapon : public AActor
{
	GENERATED_BODY()

public:
	ACelDamageWeapon();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void Collect(ACelDamageVehicle* Collector);

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void RespawnPickup();

	static FLinearColor GetColorForWeapon(ECelDamageWeaponType Type);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UBoxComponent> PickupCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	ECelDamageWeaponType WeaponType = ECelDamageWeaponType::Bat;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	bool bIsActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float RespawnTime = 10.f;

private:
	FVector OriginalLocation;
	float BobPhase = 0.f;
	float RespawnTimer = 0.f;
	void ApplyWeaponColor();
};
