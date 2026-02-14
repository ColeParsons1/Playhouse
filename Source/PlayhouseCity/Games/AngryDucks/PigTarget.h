// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PigTarget.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// APigTarget
// ---------------------------------------------------------------------------

/**
 * A pig target for the Angry Ducks mini-game.
 *
 * Pigs are placed on or within block structures.  They have a small
 * amount of HP and are physics-enabled so they fall when their
 * supporting structure is destroyed.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APigTarget : public AActor
{
	GENERATED_BODY()

public:
	APigTarget();

	virtual void BeginPlay() override;

	// -- API --------------------------------------------------------------

	/** Apply damage; destroys the pig when HP reaches zero. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void ApplyDamage(float DamageAmount);

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|AngryDucks")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|AngryDucks")
	TObjectPtr<UStaticMeshComponent> PigMesh;

	// -- Properties -------------------------------------------------------

	/** Current hit points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|AngryDucks")
	float HP = 1.f;

	/** True when the pig has been destroyed. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	bool bDestroyed = false;

	// -- Delegate ---------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPigDestroyed, APigTarget*, Pig);

	UPROPERTY(BlueprintAssignable, Category = "PHC|AngryDucks")
	FOnPigDestroyed OnPigDestroyed;

private:
	/** Apply the green pig colour to the mesh. */
	void ApplyColor();

	/** Pig collision radius in cm. */
	static constexpr float PigRadius = 10.f;
};
