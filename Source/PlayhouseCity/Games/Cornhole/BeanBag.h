// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BeanBag.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// ABeanBag
// ---------------------------------------------------------------------------

/**
 * A physics-enabled bean bag for the Cornhole mini-game.
 *
 * Thrown by the player or CPU, it lands on the board (1 pt) or goes
 * through the hole (3 pts).  After landing, a settle check determines
 * when the bag has stopped moving.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ABeanBag : public AActor
{
	GENERATED_BODY()

public:
	ABeanBag();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- API --------------------------------------------------------------

	/** Launch the bag with a given velocity vector. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Cornhole")
	void Launch(FVector Velocity);

	/** Called when this bag overlaps the hole trigger volume. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Cornhole")
	void OnEnteredHole();

	/** Is the bag considered settled (stopped moving)? */
	UFUNCTION(BlueprintPure, Category = "PHC|Cornhole")
	bool IsSettled() const { return bSettled; }

	/** Reset the bag for a new throw. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Cornhole")
	void ResetBag(FVector StartPosition);

	// -- Components -------------------------------------------------------

	/** Physics collision sphere (bean bags are roughly spherical). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Cornhole")
	TObjectPtr<USphereComponent> Collision;

	/** Visual mesh for the bag. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Cornhole")
	TObjectPtr<UStaticMeshComponent> BagMesh;

	// -- State ------------------------------------------------------------

	/** Bag colour: red for player, blue for CPU. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Cornhole")
	FLinearColor BagColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.f);

	/** True once the bag has come to rest. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	bool bSettled = false;

	/** True once the bag has been scored. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	bool bScored = false;

	/** Score value: 0 (off), 1 (on board), 3 (in hole). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 ScoreValue = 0;

	/** True if the bag went through the hole. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	bool bInHole = false;

	/** True if the bag is currently on the board surface. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	bool bOnBoard = false;

	// -- Delegate ---------------------------------------------------------

	/** Broadcast when this bag has settled. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBagSettled, ABeanBag*, Bag);

	UPROPERTY(BlueprintAssignable, Category = "PHC|Cornhole")
	FOnBagSettled OnBagSettled;

private:
	/** Configure physics material for bean-bag-like behaviour. */
	void ConfigurePhysics();

	/** Apply the bag colour to the mesh material. */
	void ApplyColor();

	/** Check if the bag has come to rest. */
	void CheckSettled();

	/** Timer tracking how long the bag has been below the speed threshold. */
	float SettleAccumulator = 0.f;

	/** Time (seconds) the bag must be below speed threshold to be "settled". */
	static constexpr float SettleRequiredTime = 0.8f;

	/** Speed below which the bag may be settling (cm/s). */
	static constexpr float SettleSpeedThreshold = 5.f;

	/** Radius of the bean bag collision sphere (cm). */
	static constexpr float BagRadius = 8.f;

	/** True once the bag has been launched. */
	bool bLaunched = false;
};
