// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GooStrut.generated.h"

class UPhysicsConstraintComponent;
class UStaticMeshComponent;
class AGooNode;

// ---------------------------------------------------------------------------
// AGooStrut
// ---------------------------------------------------------------------------

/**
 * A connection (strut) between two goo nodes in the Goo Bridge game.
 *
 * Uses a UPhysicsConstraintComponent to hold two nodes together during
 * simulation.  A thin cylinder mesh is rendered between the two node
 * positions and updated each tick.  If the strut is stretched beyond
 * MaxStretch times its rest length, it breaks and destroys itself.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AGooStrut : public AActor
{
	GENERATED_BODY()

public:
	AGooStrut();

	virtual void Tick(float DeltaTime) override;

	// -- Setup ------------------------------------------------------------

	/** Initialise the strut, connecting NodeA to NodeB with a constraint. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void Initialize(AGooNode* A, AGooNode* B);

	/** Update the visual mesh to span between the two node positions. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void UpdateVisual();

	/**
	 * Check whether the strut has been stretched beyond its breaking
	 * threshold.  Returns true if the strut should break.
	 */
	UFUNCTION(BlueprintPure, Category = "PHC|GooBridge")
	bool CheckBreak() const;

	/** Detach from both nodes and destroy this actor. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void BreakStrut();

	// -- Components -------------------------------------------------------

	/** Physics constraint holding the two nodes together. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|GooBridge")
	TObjectPtr<UPhysicsConstraintComponent> Constraint;

	/** Thin cylinder mesh visualising the connection. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|GooBridge")
	TObjectPtr<UStaticMeshComponent> StrutMesh;

	// -- State ------------------------------------------------------------

	/** First connected node. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	TObjectPtr<AGooNode> NodeA;

	/** Second connected node. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	TObjectPtr<AGooNode> NodeB;

	/** Ratio at which the strut breaks (1.3 = 30% stretch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GooBridge")
	float MaxStretch = 1.3f;

	/** Resting distance between nodes when the strut was created (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	float RestLength = 0.f;

private:
	/** Radius of the visual strut cylinder (cm). */
	static constexpr float StrutRadius = 3.f;
};
