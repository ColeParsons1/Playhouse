// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "PHCTouchInputComponent.generated.h"

/**
 * Mobile virtual joystick and touch-look component for the open world.
 *
 * Splits the screen into two halves:
 *  - LEFT half: virtual joystick for movement (returns FVector2D -1..1)
 *  - RIGHT half: touch-drag for camera look (returns delta per frame)
 *
 * Attach this to the player character. On non-mobile platforms the
 * component deactivates itself, so desktop input is unaffected.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PLAYHOUSECITY_API UPHCTouchInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPHCTouchInputComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/** Current movement input (-1..1 per axis) from the left virtual joystick. */
	UFUNCTION(BlueprintPure, Category = "PHC|Input")
	FVector2D GetMovementInput() const;

	/** Camera look delta this frame from the right touch area. */
	UFUNCTION(BlueprintPure, Category = "PHC|Input")
	FVector2D GetLookInput() const;

	/** True if the player is touching the move joystick. */
	UFUNCTION(BlueprintPure, Category = "PHC|Input")
	bool IsMoveActive() const { return MoveFingerIndex >= 0; }

	/** True if the player is touching the look area. */
	UFUNCTION(BlueprintPure, Category = "PHC|Input")
	bool IsLookActive() const { return LookFingerIndex >= 0; }

	// -- Tuning ---------------------------------------------------------------

	/** Dead zone radius as fraction of screen width (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Input")
	float JoystickDeadZone = 0.02f;

	/** Maximum joystick radius as fraction of screen width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Input")
	float JoystickRadius = 0.08f;

	/** Look sensitivity multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Input")
	float LookSensitivity = 0.15f;

private:
	// -- Touch state ----------------------------------------------------------

	/** Screen position where the move touch began. */
	FVector2D MoveOrigin = FVector2D::ZeroVector;

	/** Current position of the move finger. */
	FVector2D MoveCurrent = FVector2D::ZeroVector;

	/** Finger index currently controlling movement (-1 = none). */
	int32 MoveFingerIndex = -1;

	/** Previous frame's look touch position (for computing delta). */
	FVector2D LookPrevious = FVector2D::ZeroVector;

	/** Current position of the look finger. */
	FVector2D LookCurrent = FVector2D::ZeroVector;

	/** Finger index currently controlling camera look (-1 = none). */
	int32 LookFingerIndex = -1;

	/** Computed look delta this frame. */
	FVector2D LookDelta = FVector2D::ZeroVector;

	/** True on mobile platforms. */
	bool bIsMobilePlatform = false;

	/** True once touch events have been successfully bound. */
	bool bTouchBound = false;

	/** Process a touch start event. */
	void HandleTouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Process a touch move event. */
	void HandleTouchMoved(ETouchIndex::Type FingerIndex, FVector Location);

	/** Process a touch end event. */
	void HandleTouchEnded(ETouchIndex::Type FingerIndex, FVector Location);

	/** Returns the screen half (0 = left, 1 = right) for a screen position. */
	int32 GetScreenHalf(const FVector2D& ScreenPos) const;
};
