// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "JengaGameMode.generated.h"

class AJengaBlock;
class AJengaTower;

// ---------------------------------------------------------------------------
// AJengaGameMode
// ---------------------------------------------------------------------------

/**
 * Game mode for the Jenga mini-game.
 *
 * Players take turns removing blocks from a tower of 54 blocks (18 layers
 * of 3) without causing the tower to topple.  Physics simulation provides
 * realistic stacking behaviour.  Score is the number of blocks successfully
 * removed before collapse.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AJengaGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AJengaGameMode();

	virtual void Tick(float DeltaTime) override;

	// -- Game flow --------------------------------------------------------

	/** Start a new Jenga game: build tower, reset counters. */
	virtual void StartGame() override;

	/** Build the 54-block tower (delegates to AJengaTower). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Jenga")
	void BuildTower();

	/** Select a block for potential removal. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Jenga")
	void SelectBlock(AJengaBlock* Block);

	/**
	 * Slide the selected block in a direction by a given amount.
	 * @param Direction  World-space direction to slide.
	 * @param Amount     Distance to move this frame (cm).
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Jenga")
	void PullBlock(FVector Direction, float Amount);

	/**
	 * Release the block.  If the block has been fully extracted from the
	 * tower it is placed on top; otherwise it slides back to its original
	 * position.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Jenga")
	void ReleaseBlock();

	/** Move a fully-removed block to the top of the tower. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Jenga")
	void PlaceOnTop(AJengaBlock* Block);

	/** Check the current lean angle and determine if the tower is stable. */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	bool CheckTowerStability() const;

	/** Test whether a given block can safely be removed from its position. */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	bool IsBlockRemovable(AJengaBlock* Block) const;

	/** Get the tower height (cm) from the tower helper. */
	UFUNCTION(BlueprintPure, Category = "PHC|Jenga")
	float GetTowerHeight() const;

	// -- State ------------------------------------------------------------

	/** All blocks in the game (alias for Tower->AllBlocks). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	TArray<TObjectPtr<AJengaBlock>> Blocks;

	/** The currently selected block (nullptr if none). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	TObjectPtr<AJengaBlock> SelectedBlock;

	/** Is the player currently dragging / pulling a block? */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	bool bIsPulling = false;

	/** Number of blocks successfully removed and placed on top. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	int32 BlocksRemoved = 0;

	/** Maximum lean angle (degrees) before the tower is considered fallen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Jenga")
	float TowerStabilityThreshold = 15.f;

	/** True when the tower has collapsed. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	bool bTowerFallen = false;

	/** Timer used to wait for blocks to settle after a removal. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	float SettleTimer = 0.f;

	/** True while waiting for the tower to stabilise after a move. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	bool bWaitingForSettle = false;

	/** The tower helper actor. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Jenga")
	TObjectPtr<AJengaTower> Tower;

private:
	/** Called when the tower collapses. Calculates final score. */
	void OnTowerCollapse();

	/** Check if any blocks are still moving significantly. */
	bool AreBlocksSettled() const;

	/** Location of the selected block before pulling began. */
	FVector PullStartLocation = FVector::ZeroVector;

	/** Cumulative distance the selected block has been pulled (cm). */
	float PullDistance = 0.f;

	/** Distance a block must be pulled to count as fully removed (cm). */
	static constexpr float FullRemovalDistance = 80.f;

	/** Time (seconds) to wait for blocks to settle. */
	static constexpr float SettleWaitTime = 2.f;

	/** Speed threshold below which all blocks are considered settled. */
	static constexpr float SettleSpeedThreshold = 5.f;
};
