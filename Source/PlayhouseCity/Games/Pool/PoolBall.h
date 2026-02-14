// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoolBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPhysicalMaterial;

// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EPHCPoolBallType : uint8
{
	Solid	UMETA(DisplayName = "Solid"),
	Stripe	UMETA(DisplayName = "Stripe"),
	Eight	UMETA(DisplayName = "Eight Ball"),
	Cue		UMETA(DisplayName = "Cue Ball")
};

// ---------------------------------------------------------------------------
// APoolBall
// ---------------------------------------------------------------------------

/**
 * A single pool ball with physics simulation.
 * Ball numbers 1-7 are solids, 9-15 are stripes, 8 is the eight ball,
 * and 0 is the cue ball.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APoolBall : public AActor
{
	GENERATED_BODY()

public:
	APoolBall();

	virtual void BeginPlay() override;

	// -- Configuration ----------------------------------------------------

	/** Initialize this ball with a number (0 = cue, 1-7 solid, 8, 9-15 stripe). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void InitBall(int32 InBallNumber);

	/** Standard pool ball colour mapping. */
	UFUNCTION(BlueprintPure, Category = "PHC|Pool")
	static FLinearColor GetBallColor(int32 Number);

	// -- State ------------------------------------------------------------

	/** Called when this ball enters a pocket. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void OnPotted();

	/** Is the ball moving above a tiny speed threshold? */
	UFUNCTION(BlueprintPure, Category = "PHC|Pool")
	bool IsMoving() const;

	// -- Properties -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Pool")
	int32 BallNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Pool")
	EPHCPoolBallType BallType = EPHCPoolBallType::Cue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Pool")
	FLinearColor BallColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Pool")
	bool bPotted = false;

private:
	/** Setup physics material for realistic pool-ball behaviour. */
	void ConfigurePhysics();

	/** Speed below which the ball is considered stationary (cm/s). */
	static constexpr float StationaryThreshold = 2.f;
};
