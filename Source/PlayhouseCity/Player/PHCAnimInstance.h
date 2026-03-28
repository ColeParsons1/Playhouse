// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PHCAnimInstance.generated.h"

/**
 * Custom AnimInstance for PlayhouseCity character.
 * Reads velocity/state from CharacterMovementComponent and blends
 * between locomotion animations. Supports traversal montages.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// -- Locomotion state (read by AnimGraph) --------------------------------

	/** Ground speed (horizontal velocity magnitude). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Locomotion")
	float GroundSpeed = 0.f;

	/** True when CMC reports character is falling/jumping. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Locomotion")
	bool bIsFalling = false;

	/** True when character has horizontal input (is trying to move). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Locomotion")
	bool bIsMoving = false;

	/** True when sprinting. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Locomotion")
	bool bIsSprinting = false;

	/** Blend weight: 0=idle, ramps up with speed. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Locomotion")
	float LocomotionBlend = 0.f;

	// -- Animation sequences (loaded in NativeInitializeAnimation) -----------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Anims")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Anims")
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Anims")
	TObjectPtr<UAnimSequence> RunAnim;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Anims")
	TObjectPtr<UAnimSequence> SprintAnim;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Anims")
	TObjectPtr<UAnimSequence> JumpStartAnim;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Anims")
	TObjectPtr<UAnimSequence> JumpLoopAnim;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Anims")
	TObjectPtr<UAnimSequence> JumpLandAnim;

private:
	TWeakObjectPtr<class ACharacter> OwnerCharacter;
	TWeakObjectPtr<class UCharacterMovementComponent> OwnerCMC;
	TObjectPtr<UAnimSequence> CurrentAnim;
};
