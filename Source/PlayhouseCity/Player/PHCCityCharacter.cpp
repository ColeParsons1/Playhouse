// Copyright PlayhouseCity. All Rights Reserved.

#include "Player/PHCCityCharacter.h"
#include "Input/PHCTouchInputComponent.h"
#include "PlayhouseCity.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Core/PHCPlayerController.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCCityCharacter::APHCCityCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// ---- Camera boom (spring arm) ----------------------------------------
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.f;
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 80.f); // over-right-shoulder
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.f;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 8.f;

	// ---- Follow camera ---------------------------------------------------
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// ---- Character movement defaults -------------------------------------
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->JumpZVelocity = 420.f;
	Movement->AirControl = 0.2f;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;

	// Don't rotate the character mesh to match the controller/camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// ---- Touch input component -------------------------------------------
	TouchInput = CreateDefaultSubobject<UPHCTouchInputComponent>(TEXT("TouchInput"));
}

// ---------------------------------------------------------------------------
// Input asset creation (programmatic -- no .uasset files needed)
// ---------------------------------------------------------------------------

void APHCCityCharacter::CreateInputAssets()
{
	// ---- Input Actions ---------------------------------------------------

	// Move: 2D axis (WASD / left joystick)
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	// Look: 2D axis (mouse delta / right stick)
	LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

	// Jump: digital (space / button)
	JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
	JumpAction->ValueType = EInputActionValueType::Boolean;

	// Sprint: digital (shift / button)
	SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;

	// Interact: digital (E/F key -- enter vehicle, interact with objects)
	InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact"));
	InteractAction->ValueType = EInputActionValueType::Boolean;

	// ---- Mapping Context -------------------------------------------------

	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_CityDefault"));

	// -- WASD Movement --
	// W = forward (+Y). W is 1D, so swizzle X->Y to push it into the Y axis.
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
	// S = backward (-Y). Swizzle + negate.
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(this);
		Mapping.Modifiers.Add(Negate);
	}
	// D = right (+X). 1D key naturally maps to X axis -- no modifiers needed.
	{
		DefaultMappingContext->MapKey(MoveAction, EKeys::D);
	}
	// A = left (-X). Negate.
	{
		FEnhancedActionKeyMapping& Mapping = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(this);
		Mapping.Modifiers.Add(Negate);
	}

	// -- Mouse Look --
	DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);

	// -- Jump --
	DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);

	// -- Sprint --
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);

	// -- Interact (enter vehicle / interact) --
	DefaultMappingContext->MapKey(InteractAction, EKeys::E);
	DefaultMappingContext->MapKey(InteractAction, EKeys::F);

	// -- Gamepad bindings (for controllers / future use) --
	// Left stick: 4 directional bindings for movement
	{
		FEnhancedActionKeyMapping& Up = DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Up);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Up.Modifiers.Add(Swizzle);
	}
	{
		FEnhancedActionKeyMapping& Down = DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Down);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Down.Modifiers.Add(Swizzle);
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(this);
		Down.Modifiers.Add(Negate);
	}
	DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Right);
	{
		FEnhancedActionKeyMapping& Left = DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Left);
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(this);
		Left.Modifiers.Add(Negate);
	}
	// Right stick: camera look
	{
		FEnhancedActionKeyMapping& RUp = DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Up);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		RUp.Modifiers.Add(Swizzle);
	}
	{
		FEnhancedActionKeyMapping& RDown = DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Down);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(this);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		RDown.Modifiers.Add(Swizzle);
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(this);
		RDown.Modifiers.Add(Negate);
	}
	DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Right);
	{
		FEnhancedActionKeyMapping& RLeft = DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Left);
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(this);
		RLeft.Modifiers.Add(Negate);
	}
	DefaultMappingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom); // A button
	DefaultMappingContext->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);  // L3 click
	DefaultMappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Left); // Y button

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityCharacter: Enhanced Input assets created programmatically."));
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void APHCCityCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Create input assets at runtime (cannot use NewObject in constructor/CDO).
	CreateInputAssets();

	// Register the mapping context with the Enhanced Input subsystem.
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// Bind input actions here (they didn't exist yet when SetupPlayerInputComponent ran).
	if (InputComponent)
	{
		UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
		if (EIC)
		{
			if (MoveAction)
			{
				EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APHCCityCharacter::HandleMove);
			}
			if (LookAction)
			{
				EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &APHCCityCharacter::HandleLook);
			}
			if (JumpAction)
			{
				EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &APHCCityCharacter::HandleJumpStarted);
				EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &APHCCityCharacter::HandleJumpCompleted);
			}
			if (SprintAction)
			{
				EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &APHCCityCharacter::HandleSprintStarted);
				EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &APHCCityCharacter::HandleSprintCompleted);
			}
			if (InteractAction)
			{
				EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &APHCCityCharacter::HandleInteract);
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityCharacter::BeginPlay -- Character spawned at %s"),
		*GetActorLocation().ToString());
}

// ---------------------------------------------------------------------------
// Tick -- apply mobile touch input
// ---------------------------------------------------------------------------

void APHCCityCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!TouchInput || !TouchInput->IsActive() || !Controller)
	{
		return;
	}

	// Apply movement from the virtual joystick.
	const FVector2D MoveInput = TouchInput->GetMovementInput();
	if (!MoveInput.IsNearlyZero())
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDir, MoveInput.Y);
		AddMovementInput(RightDir, MoveInput.X);
	}

	// Apply camera look from the right-side touch drag.
	const FVector2D LookInput = TouchInput->GetLookInput();
	if (!LookInput.IsNearlyZero())
	{
		AddControllerYawInput(LookInput.X);
		AddControllerPitchInput(LookInput.Y);
	}
}

// ---------------------------------------------------------------------------
// Input binding
// ---------------------------------------------------------------------------

void APHCCityCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Input actions are created in BeginPlay (NewObject can't run in CDO constructor).
	// The actual bindings happen in BeginPlay after CreateInputAssets().
	// This override is kept for subclass extensibility.
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void APHCCityCharacter::HandleMove(const FInputActionValue& Value)
{
	if (!Controller) return;

	const FVector2D MoveVector = Value.Get<FVector2D>();
	if (MoveVector.IsNearlyZero())
	{
		return;
	}

	// Build camera-relative movement directions.
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDir, MoveVector.Y);
	AddMovementInput(RightDir, MoveVector.X);
}

void APHCCityCharacter::HandleLook(const FInputActionValue& Value)
{
	if (!Controller) return;

	const FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X);
	AddControllerPitchInput(LookVector.Y);
}

void APHCCityCharacter::HandleJumpStarted(const FInputActionValue& /*Value*/)
{
	Jump();
}

void APHCCityCharacter::HandleJumpCompleted(const FInputActionValue& /*Value*/)
{
	StopJumping();
}

void APHCCityCharacter::HandleSprintStarted(const FInputActionValue& /*Value*/)
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void APHCCityCharacter::HandleSprintCompleted(const FInputActionValue& /*Value*/)
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void APHCCityCharacter::HandleInteract(const FInputActionValue& /*Value*/)
{
	// Try to enter a nearby vehicle.
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(Controller))
	{
		PC->EnterNearestVehicle();
	}
}
