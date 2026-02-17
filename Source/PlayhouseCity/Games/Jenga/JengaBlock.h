// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "JengaBlock.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// AJengaBlock
// ---------------------------------------------------------------------------

/**
 * A single Jenga block with physics simulation.
 *
 * Standard Jenga proportions: length:width:height = 7.5 : 2.5 : 1.5 cm.
 * Scaled up for the game world.  Physics material is configured for
 * wood-like friction to create realistic stacking and sliding behaviour.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AJengaBlock : public AActor
{
	GENERATED_BODY()

public:
	AJengaBlock();

	virtual void BeginPlay() override;

	// -- API --------------------------------------------------------------

	/** Highlight this block (e.g. when hovered or selected). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Jenga")
	void SetHighlighted(bool bHighlight);

	/** Is the block currently moving above a tiny speed threshold? */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	bool IsMoving() const;

	/** Get the current linear velocity magnitude (cm/s). */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	float GetSpeed() const;

	// -- Components -------------------------------------------------------

	/** Physics-enabled box collision matching Jenga block proportions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Jenga")
	TObjectPtr<UBoxComponent> BlockCollision;

	/** Visual mesh for the block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Jenga")
	TObjectPtr<UStaticMeshComponent> BlockMesh;

	// -- State ------------------------------------------------------------

	/** The layer this block belongs to (0 = bottom layer). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Jenga")
	int32 LayerIndex = 0;

	/** Position within the layer (0, 1, or 2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Jenga")
	int32 PositionInLayer = 0;

	/** True once the block has been fully removed from the tower. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	bool bIsRemoved = false;

	/** Slight wood-tone colour variation per block. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Jenga")
	FLinearColor BlockColor = FLinearColor(0.76f, 0.6f, 0.42f, 1.f);

	// -- Dimensions (cm) - half-extents for the box component ------------

	/** Full block length (cm). */
	static constexpr float BlockLength = 75.f;

	/** Full block width (cm). */
	static constexpr float BlockWidth = 25.f;

	/** Full block height (cm). */
	static constexpr float BlockHeight = 15.f;

private:
	/** Apply wood-like physics material. */
	void ConfigurePhysics();

	/** Dynamic material instance for highlight toggling. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynMaterial;

	/** Speed below which the block is considered stationary (cm/s). */
	static constexpr float StationaryThreshold = 3.f;
};
