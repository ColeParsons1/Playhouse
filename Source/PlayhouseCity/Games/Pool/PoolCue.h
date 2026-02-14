// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoolCue.generated.h"

class UStaticMeshComponent;
class ULineBatchComponent;

// ---------------------------------------------------------------------------
// APoolCue
// ---------------------------------------------------------------------------

/**
 * Visual cue stick used for aiming and striking the cue ball.
 * Provides aim-line visualisation, power pull-back feedback, and
 * a simple strike animation.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APoolCue : public AActor
{
	GENERATED_BODY()

public:
	APoolCue();

	virtual void Tick(float DeltaTime) override;

	// -- Aiming -----------------------------------------------------------

	/** Orient the cue to face the given world-space direction. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void SetAimDirection(FVector Direction);

	/** Visual pull-back distance proportional to shot power (0..1). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void SetPowerLevel(float Power);

	/** Render a dotted aim line from Start along Direction. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void ShowAimLine(FVector Start, FVector Direction);

	/** Remove the aim line visualisation. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void HideAimLine();

	/** Play the forward-strike animation (cosmetic only). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void PlayStrikeAnimation();

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Pool")
	TObjectPtr<UStaticMeshComponent> CueMesh;

	// -- Tuning -----------------------------------------------------------

	/** Maximum pull-back distance in cm when power is 1.0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float MaxPullBack = 30.f;

	/** Length of the dotted aim line in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float AimLineLength = 200.f;

	/** Gap between dots on the aim line in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Pool")
	float AimLineDotSpacing = 8.f;

private:
	/** Current aim direction (normalised). */
	FVector AimDir = FVector::ForwardVector;

	/** Current power level 0..1. */
	float CurrentPower = 0.f;

	/** Whether the aim line is visible. */
	bool bAimLineVisible = false;

	/** Cached aim-line start and direction. */
	FVector AimLineStart = FVector::ZeroVector;
	FVector AimLineDir = FVector::ForwardVector;

	/** Strike animation state. */
	bool bStriking = false;
	float StrikeTimer = 0.f;
	float StrikeDuration = 0.12f;

	/** Draw the dotted aim line via debug drawing. */
	void DrawAimLine() const;
};
