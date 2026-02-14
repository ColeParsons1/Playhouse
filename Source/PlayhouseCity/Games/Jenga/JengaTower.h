// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JengaTower.generated.h"

class AJengaBlock;

// ---------------------------------------------------------------------------
// AJengaTower
// ---------------------------------------------------------------------------

/**
 * Container / helper actor that manages the Jenga tower state.
 *
 * Responsible for building the initial stack of 54 blocks (18 layers of 3),
 * computing block positions / rotations per layer (alternating orientation),
 * and providing tower stability metrics.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AJengaTower : public AActor
{
	GENERATED_BODY()

public:
	AJengaTower();

	// -- Tower construction -----------------------------------------------

	/**
	 * Build the full tower stack of 54 blocks at TowerBasePosition.
	 * Destroys any existing blocks first.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Jenga")
	void BuildStack();

	/** Get the world position for a block at the given layer and position. */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	FVector GetBlockPosition(int32 Layer, int32 Position, bool bAlternateOrientation) const;

	/** Get the rotation for blocks in the given layer (alternates per layer). */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	FRotator GetBlockRotation(int32 Layer) const;

	// -- Tower state -------------------------------------------------------

	/** Calculate the current lean angle of the tower (degrees from vertical). */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	float GetCurrentLeanAngle() const;

	/** Get the current top layer index (accounting for removed blocks). */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	int32 GetTopLayerIndex() const;

	/** Get the total height of the tower from base to top block (cm). */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	float GetTowerHeight() const;

	// -- Data -------------------------------------------------------------

	/** All blocks in the tower (54 total when full). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	TArray<TObjectPtr<AJengaBlock>> AllBlocks;

	/** World position of the tower base center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Jenga")
	FVector TowerBasePosition = FVector::ZeroVector;

	/** Current top layer number (increases as blocks are placed on top). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	int32 TopLayer = 17; // 0-indexed, 18 layers initially

	// -- Constants --------------------------------------------------------

	/** Total number of layers in a standard Jenga tower. */
	static constexpr int32 TotalLayers = 18;

	/** Number of blocks per layer. */
	static constexpr int32 BlocksPerLayer = 3;

	/** Total blocks in a full tower. */
	static constexpr int32 TotalBlocks = TotalLayers * BlocksPerLayer;

private:
	/** Destroy all existing blocks. */
	void ClearBlocks();
};
