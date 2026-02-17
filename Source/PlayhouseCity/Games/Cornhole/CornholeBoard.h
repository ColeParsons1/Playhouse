// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CornholeBoard.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// ACornholeBoard
// ---------------------------------------------------------------------------

/**
 * The physical cornhole board.
 *
 * An angled surface with a hole trigger volume.  When a bean bag overlaps
 * the hole trigger it is notified via ABeanBag::OnEnteredHole().
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACornholeBoard : public AActor
{
	GENERATED_BODY()

public:
	ACornholeBoard();

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Cornhole")
	TObjectPtr<UBoxComponent> BoardCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Cornhole")
	TObjectPtr<UStaticMeshComponent> BoardMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Cornhole")
	TObjectPtr<UBoxComponent> HoleTrigger;

	// -- Config -----------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Cornhole")
	float BoardLength = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Cornhole")
	float BoardWidth = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Cornhole")
	float BoardAngle = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Cornhole")
	float HoleRadius = 8.f;

private:
	UFUNCTION()
	void OnHoleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
