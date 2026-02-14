// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CarromPiece.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EPHCCarromPieceType : uint8
{
	White	UMETA(DisplayName = "White"),
	Black	UMETA(DisplayName = "Black"),
	Queen	UMETA(DisplayName = "Queen"),
	Striker	UMETA(DisplayName = "Striker")
};

// ---------------------------------------------------------------------------
// ACarromPiece
// ---------------------------------------------------------------------------

/**
 * A single carrom disc (coin or striker).
 * Uses flat-disc physics: constrained to XY plane, with appropriate
 * friction and restitution values for each piece type.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACarromPiece : public AActor
{
	GENERATED_BODY()

public:
	ACarromPiece();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Setup ------------------------------------------------------------

	/** Initialise this piece with the given type. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Carrom")
	void InitPiece(EPHCCarromPieceType InType);

	// -- State ------------------------------------------------------------

	/** True if the piece is moving above the stationary threshold. */
	UFUNCTION(BlueprintPure, Category = "PHC|Carrom")
	bool IsMoving() const;

	/** Called when pocketed. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Carrom")
	void OnPocketed();

	/** Reset position for striker replacement. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Carrom")
	void ResetToPosition(FVector Pos);

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<USphereComponent> CollisionDisc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<UStaticMeshComponent> DiscMesh;

	// -- Properties -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Carrom")
	EPHCCarromPieceType PieceType = EPHCCarromPieceType::White;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	bool bPocketed = false;

private:
	/** Configure mass, friction, damping based on piece type. */
	void ConfigurePhysics();

	/** Constrain Z velocity to keep pieces on the board surface. */
	void ConstrainToBoard();

	static constexpr float StationaryThreshold = 1.5f;
	static constexpr float CoinRadius = 1.6f;   // ~3.2 cm diameter coins
	static constexpr float StrikerRadius = 2.1f; // ~4.2 cm diameter striker
};
