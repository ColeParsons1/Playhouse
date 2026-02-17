// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "GooBridgeGameMode.generated.h"

class AGooNode;
class AGooStrut;

// ---------------------------------------------------------------------------
// AGooBridgeGameMode
// ---------------------------------------------------------------------------

/**
 * Game mode for the Goo Bridge mini-game (World of Goo inspired).
 *
 * The player places goo nodes in the world and the game automatically
 * connects them to nearby nodes with struts.  Once the player is satisfied
 * with the structure they press "Simulate" to enable gravity.  If the
 * bridge holds and any node reaches the goal zone, the level is cleared.
 *
 * Each level increases the gap width and reduces available goo.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AGooBridgeGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AGooBridgeGameMode();

	virtual void Tick(float DeltaTime) override;

	// -- Game flow --------------------------------------------------------

	/** Start the Goo Bridge game and load the first level. */
	virtual void StartGame() override;

	/**
	 * Load a specific level.  Sets gap width, goo budget,
	 * and goal position, then spawns the platforms and anchor nodes.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void LoadLevel(int32 Level);

	/**
	 * Place a new goo node at the given world position.
	 * Automatically connects to nearby existing nodes.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void PlaceNode(FVector2D Position);

	/** Create a strut (physics constraint) between two nodes. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void ConnectNodes(AGooNode* A, AGooNode* B);

	/** Enable gravity / physics on all nodes to test the bridge. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void StartSimulation();

	/** Disable simulation and restore pre-sim state if the bridge failed. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void StopSimulation();

	/** Check whether any node has entered the goal zone. */
	UFUNCTION(BlueprintPure, Category = "PHC|GooBridge")
	bool CheckBridgeSuccess() const;

	/** Check whether the structure has collapsed below the death plane. */
	UFUNCTION(BlueprintPure, Category = "PHC|GooBridge")
	bool CheckBridgeCollapse() const;

	/** Remove a placed node and all struts connected to it (undo). */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	void RemoveNode(AGooNode* Node);

	/** Find all placed nodes within a given radius of Position. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GooBridge")
	TArray<AGooNode*> GetNearbyNodes(FVector2D Position, float Radius) const;

	// -- State ------------------------------------------------------------

	/** All placed goo nodes in the world. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	TArray<TObjectPtr<AGooNode>> Nodes;

	/** All struts connecting goo nodes. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	TArray<TObjectPtr<AGooStrut>> Struts;

	/** Number of goo nodes the player can still place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GooBridge")
	int32 GooAvailable = 20;

	/** World-space position of the goal zone (2D, Z is implied). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GooBridge")
	FVector2D GoalPosition = FVector2D(750.f, 0.f);

	/** Position of the starting platform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GooBridge")
	FVector2D StartPlatformPos = FVector2D(-250.f, 0.f);

	/** Position of the ending platform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GooBridge")
	FVector2D EndPlatformPos = FVector2D(750.f, 0.f);

	/** Width of the gap between platforms (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|GooBridge")
	float GapWidth = 500.f;

	/** Current puzzle level. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	int32 CurrentLevel = 1;

	/** Is the physics simulation currently running? */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	bool bSimulating = false;

	/** Currently selected/hovered node (for UI feedback). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|GooBridge")
	TObjectPtr<AGooNode> SelectedNode;

	/** Maximum distance (cm) for auto-connecting a new node to neighbours. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|GooBridge")
	float AutoConnectRadius = 200.f;

	/** Z coordinate below which nodes are considered lost/fallen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|GooBridge")
	float DeathPlaneZ = -500.f;

	/** Radius of the goal zone used for success detection (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|GooBridge")
	float GoalRadius = 80.f;

private:
	/** Spawn anchor nodes on a platform at the given position. */
	void SpawnAnchorNodes(FVector2D PlatformCenter, int32 Count);

	/** Clear all nodes, struts and reset state for a fresh level. */
	void ClearLevel();

	/** Height at which the game plays (constant Z). */
	static constexpr float PlayZ = 0.f;

	/** Store pre-simulation strut data for reset. */
	struct FStrutSnapshot
	{
		TWeakObjectPtr<AGooNode> A;
		TWeakObjectPtr<AGooNode> B;
	};
	TArray<FStrutSnapshot> PreSimStruts;
};
