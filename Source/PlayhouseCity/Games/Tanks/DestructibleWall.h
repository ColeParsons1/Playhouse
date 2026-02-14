// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleWall.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// ADestructibleWall
// ---------------------------------------------------------------------------

/**
 * A destructible terrain block for the Tanks mini-game.
 * Has HP and is removed from the scene when destroyed.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ADestructibleWall : public AActor
{
	GENERATED_BODY()

public:
	ADestructibleWall();

	// -- Damage -----------------------------------------------------------

	/** Apply damage; destroys the wall when HP reaches zero. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void ApplyDamage(int32 DamageAmount);

	/** Delegate broadcast when this wall is destroyed. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallDestroyed, ADestructibleWall*, Wall);

	UPROPERTY(BlueprintAssignable, Category = "PHC|Tanks")
	FOnWallDestroyed OnWallDestroyed;

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<UBoxComponent> BoxCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<UStaticMeshComponent> WallMesh;

	// -- Properties -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	int32 HP = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	int32 MaxHP = 1;

	/** Wall block size in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Tanks")
	float BlockSize = 50.f;
};
