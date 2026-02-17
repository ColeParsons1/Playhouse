// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GooNode.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AGooStrut;

// ---------------------------------------------------------------------------
// AGooNode
// ---------------------------------------------------------------------------

/**
 * A single goo ball / node in the Goo Bridge game.
 *
 * Nodes are placed by the player to form a bridge structure.  Each node
 * maintains a list of struts that connect it to neighbours.  When simulation
 * starts, physics is enabled and the node becomes part of the physical
 * structure subject to gravity.
 *
 * Anchor nodes on platforms have bIsFixed=true and remain kinematic during
 * simulation so the bridge stays attached to the terrain.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AGooNode : public AActor
{
	GENERATED_BODY()

public:
	AGooNode();

	virtual void BeginPlay() override;

	// -- Simulation control -----------------------------------------------

	/** Toggle physics simulation on/off for this node. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void SetSimulating(bool bSim);

	/** Register a strut as connected to this node. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void AddStrut(AGooStrut* Strut);

	/** Unregister a strut from this node. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void RemoveStrut(AGooStrut* Strut);

	// -- Components -------------------------------------------------------

	/** Physics-enabled sphere collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|GooBridge")
	TObjectPtr<USphereComponent> Collision;

	/** Visual sphere mesh representing the goo ball. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|GooBridge")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// -- State ------------------------------------------------------------

	/** All struts currently attached to this node. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	TArray<TObjectPtr<AGooStrut>> ConnectedStruts;

	/** If true, this is an anchor node on a platform (remains kinematic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GooBridge")
	bool bIsFixed = false;

	/** Has this node been placed by the player? */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	bool bIsPlaced = false;

	/** Position stored before simulation starts, used for reset. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	FVector PreSimLocation = FVector::ZeroVector;

private:
	/** Configure physics material for goo-like behaviour. */
	void ConfigurePhysics();

	/** Radius of the goo ball collision sphere (cm). */
	static constexpr float NodeRadius = 20.f;
};
