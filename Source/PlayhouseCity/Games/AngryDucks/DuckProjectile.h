// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DuckProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// ADuckProjectile
// ---------------------------------------------------------------------------

/**
 * Physics-enabled duck projectile for the Angry Ducks mini-game.
 *
 * Launched from the slingshot, the duck travels in an arc under gravity,
 * bouncing off surfaces and dealing damage to blocks and pigs on impact.
 * After the duck comes to rest it broadcasts OnDuckSettled.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ADuckProjectile : public AActor
{
	GENERATED_BODY()

public:
	ADuckProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- API --------------------------------------------------------------

	/** Launch the duck with the given world-space velocity. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void Launch(FVector Velocity);

	/** Has the duck been launched? */
	UFUNCTION(BlueprintPure, Category = "PHC|AngryDucks")
	bool IsLaunched() const { return bLaunched; }

	/** Has the duck come to rest? */
	UFUNCTION(BlueprintPure, Category = "PHC|AngryDucks")
	bool IsSettled() const { return bSettled; }

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|AngryDucks")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|AngryDucks")
	TObjectPtr<UStaticMeshComponent> DuckMesh;

	// -- Properties -------------------------------------------------------

	/** Damage dealt to blocks/pigs on impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|AngryDucks")
	float ImpactDamage = 1.f;

	/** True once the duck has been launched from the slingshot. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	bool bLaunched = false;

	/** True once the duck has come to rest. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|AngryDucks")
	bool bSettled = false;

	// -- Delegate ---------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDuckSettled, ADuckProjectile*, Duck);

	UPROPERTY(BlueprintAssignable, Category = "PHC|AngryDucks")
	FOnDuckSettled OnDuckSettled;

private:
	UFUNCTION()
	void OnHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	/** Check if the duck has come to rest. */
	void CheckSettled();

	/** Apply the yellow duck colour to the mesh. */
	void ApplyColor();

	/** Timer tracking how long the duck has been below the speed threshold. */
	float SettleAccumulator = 0.f;

	/** Time (seconds) the duck must be below speed threshold to be "settled". */
	static constexpr float SettleRequiredTime = 0.8f;

	/** Speed below which the duck may be settling (cm/s). */
	static constexpr float SettleSpeedThreshold = 8.f;

	/** Radius of the duck collision sphere (cm). */
	static constexpr float DuckRadius = 12.f;

	/** Auto-destroy after this many seconds once settled. */
	static constexpr float PostSettleLifeSpan = 2.f;
};
