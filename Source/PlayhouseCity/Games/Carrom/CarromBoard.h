// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CarromBoard.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USphereComponent;
class ACarromPiece;

// ---------------------------------------------------------------------------
// ACarromBoard
// ---------------------------------------------------------------------------

/**
 * Square carrom board with four corner pockets and boundary walls.
 * Standard board is 73.5 cm x 73.5 cm.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACarromBoard : public AActor
{
	GENERATED_BODY()

public:
	ACarromBoard();

	virtual void BeginPlay() override;

	// -- Pocket detection -------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPiecePocketed, ACarromPiece*, Piece);

	UPROPERTY(BlueprintAssignable, Category = "PHC|Carrom")
	FOnPiecePocketed OnPiecePocketed;

	// -- Components -------------------------------------------------------

	/** Flat playing surface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<UStaticMeshComponent> BoardSurface;

	/** Four boundary walls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<UStaticMeshComponent> WallTop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<UStaticMeshComponent> WallBottom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<UStaticMeshComponent> WallLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<UStaticMeshComponent> WallRight;

	/** Four corner pocket triggers. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TArray<TObjectPtr<USphereComponent>> Pockets;

	// -- Dimensions (cm) --------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Carrom")
	float BoardSize = 73.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Carrom")
	float WallThickness = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Carrom")
	float WallHeight = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Carrom")
	float PocketRadius = 3.5f;

private:
	void BuildWalls();
	void BuildPockets();
	void ConfigureWallPhysics(UStaticMeshComponent* Wall);

	UFUNCTION()
	void OnPocketOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
