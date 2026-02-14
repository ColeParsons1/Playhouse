// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleBlock.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// EBlockType
// ---------------------------------------------------------------------------

/** Material types for destructible blocks in Angry Ducks. */
UENUM(BlueprintType)
enum class EBlockType : uint8
{
	Wood   UMETA(DisplayName = "Wood"),   // 1 HP, brown
	Stone  UMETA(DisplayName = "Stone"),  // 2 HP, gray
	Glass  UMETA(DisplayName = "Glass")   // 1 HP, light blue (fragile)
};

// ---------------------------------------------------------------------------
// ADestructibleBlock
// ---------------------------------------------------------------------------

/**
 * A destructible building block for the Angry Ducks mini-game.
 *
 * Blocks form structures that the player knocks down with duck
 * projectiles.  Each block has HP based on its material type and
 * enables physics simulation when struck so that structures topple
 * realistically.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ADestructibleBlock : public AActor
{
	GENERATED_BODY()

public:
	ADestructibleBlock();

	// -- API --------------------------------------------------------------

	/** Apply damage; destroys the block when HP reaches zero. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void ApplyDamage(float DamageAmount);

	/** Set the block material type (updates HP and colour). */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void SetBlockType(EBlockType NewType);

	/** Return the colour corresponding to the current block type. */
	UFUNCTION(BlueprintPure, Category = "PHC|AngryDucks")
	FLinearColor GetBlockColor() const;

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|AngryDucks")
	TObjectPtr<UBoxComponent> BoxCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|AngryDucks")
	TObjectPtr<UStaticMeshComponent> BlockMesh;

	// -- Properties -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|AngryDucks")
	EBlockType BlockType = EBlockType::Wood;

	/** Current hit points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|AngryDucks")
	float HP = 1.f;

	/** Maximum hit points (set by block type). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|AngryDucks")
	float MaxHP = 1.f;

	/** Block half-extent in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|AngryDucks")
	float BlockHalfExtent = 25.f;

	// -- Delegate ---------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBlockDestroyed, ADestructibleBlock*, Block);

	UPROPERTY(BlueprintAssignable, Category = "PHC|AngryDucks")
	FOnBlockDestroyed OnBlockDestroyed;

private:
	/** Apply the block colour to the mesh material. */
	void ApplyColor();
};
