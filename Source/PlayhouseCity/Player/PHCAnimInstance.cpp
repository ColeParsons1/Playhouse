// Copyright PlayhouseCity. All Rights Reserved.

#include "Player/PHCAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Player/PHCCityCharacter.h"

void UPHCAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
	if (OwnerCharacter.IsValid())
	{
		OwnerCMC = OwnerCharacter->GetCharacterMovement();
	}

	// Load UEFN Mannequin animation sequences
	IdleAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop"));

	WalkAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Walk/M_Neutral_Walk_Loop_F.M_Neutral_Walk_Loop_F"));

	RunAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F"));

	SprintAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Sprint/M_Neutral_Sprint_Loop_F.M_Neutral_Sprint_Loop_F"));

	JumpStartAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Jump/M_Neutral_Jump_F_Start_Stand_Rfoot.M_Neutral_Jump_F_Start_Stand_Rfoot"));

	JumpLoopAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Jump/M_Neutral_Jump_Loop_Fall.M_Neutral_Jump_Loop_Fall"));

	JumpLandAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Jump/M_Neutral_Jump_F_Land_Stand_Light_Rfoot.M_Neutral_Jump_F_Land_Stand_Light_Rfoot"));
}

void UPHCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter.IsValid() || !OwnerCMC.IsValid())
	{
		OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
		if (OwnerCharacter.IsValid())
			OwnerCMC = OwnerCharacter->GetCharacterMovement();
		return;
	}

	// Read CMC state
	const FVector Velocity = OwnerCMC->Velocity;
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	bIsFalling = OwnerCMC->IsFalling();
	bIsMoving = GroundSpeed > 10.f;

	// Check sprint state from our character
	if (APHCCityCharacter* PHCChar = Cast<APHCCityCharacter>(OwnerCharacter.Get()))
	{
		bIsSprinting = PHCChar->bIsSprinting;
	}

	// Smooth locomotion blend (0=idle, 1=full movement)
	const float TargetBlend = bIsMoving ? 1.f : 0.f;
	LocomotionBlend = FMath::FInterpTo(LocomotionBlend, TargetBlend, DeltaSeconds, 8.f);

	// Drive the skeleton directly — pick the right animation based on state
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh) return;

	UAnimSequence* DesiredAnim = nullptr;

	if (bIsFalling)
	{
		DesiredAnim = JumpLoopAnim;
	}
	else if (bIsMoving)
	{
		if (bIsSprinting && SprintAnim)
			DesiredAnim = SprintAnim;
		else if (GroundSpeed > 400.f && RunAnim)
			DesiredAnim = RunAnim;
		else
			DesiredAnim = WalkAnim;
	}
	else
	{
		DesiredAnim = IdleAnim;
	}

	// Only switch animation when desired changes (avoid restarting same anim)
	if (DesiredAnim && DesiredAnim != CurrentAnim)
	{
		CurrentAnim = DesiredAnim;
		{
			Mesh->PlayAnimation(DesiredAnim, true);

			// Adjust play rate based on speed for natural feel
			if (DesiredAnim == WalkAnim)
				Mesh->SetPlayRate(FMath::Clamp(GroundSpeed / 200.f, 0.5f, 1.5f));
			else if (DesiredAnim == RunAnim)
				Mesh->SetPlayRate(FMath::Clamp(GroundSpeed / 600.f, 0.8f, 1.3f));
			else if (DesiredAnim == SprintAnim)
				Mesh->SetPlayRate(FMath::Clamp(GroundSpeed / 900.f, 0.9f, 1.2f));
			else
				Mesh->SetPlayRate(1.f);
		}
	}
	else if (bIsMoving && !bIsFalling && Mesh)
	{
		// Continuously adjust play rate to match speed
		if (CurrentAnim == WalkAnim)
			Mesh->SetPlayRate(FMath::Clamp(GroundSpeed / 200.f, 0.5f, 1.5f));
		else if (CurrentAnim == RunAnim)
			Mesh->SetPlayRate(FMath::Clamp(GroundSpeed / 600.f, 0.8f, 1.3f));
		else if (CurrentAnim == SprintAnim)
			Mesh->SetPlayRate(FMath::Clamp(GroundSpeed / 900.f, 0.9f, 1.2f));
	}
}
