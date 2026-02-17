// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoolTable.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class APoolBall;

// ---------------------------------------------------------------------------
// APoolTable
// ---------------------------------------------------------------------------

/**
 * Pool table actor containing the playing surface, cushions (rails),
 * and six pocket trigger volumes.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APoolTable : public AActor
{
	GENERATED_BODY()

public:
	APoolTable();

	virtual void BeginPlay() override;

	// -- Pocket detection delegate ----------------------------------------

	/** Broadcast when a ball overlaps a pocket trigger. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBallPocketed, APoolBall*, Ball);

	UPROPERTY(BlueprintAssignable, Category = "PHC|Pool")
	FOnBallPocketed OnBallPocketed;

	// -- Components -------------------------------------------------------

	/** The flat playing surface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<UStaticMeshComponent> TableSurface;

	/** Four rail cushions surrounding the playing area. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<UStaticMeshComponent> CushionTop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<UStaticMeshComponent> CushionBottom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<UStaticMeshComponent> CushionLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<UStaticMeshComponent> CushionRight;

	/** Six pocket trigger volumes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TArray<TObjectPtr<UBoxComponent>> Pockets;

	// -- Dimensions (cm) --------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float TableLength = 254.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float TableWidth = 127.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float CushionHeight = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float CushionThickness = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float PocketRadius = 7.f;

private:
	/** Create and attach cushion components. */
	void BuildCushions();

	/** Create pocket trigger boxes at the six standard positions. */
	void BuildPockets();

	/** Configure physics material for cushions. */
	void ConfigureCushionPhysics(UStaticMeshComponent* Cushion);

	/** Pocket overlap callback. */
	UFUNCTION()
	void OnPocketOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
