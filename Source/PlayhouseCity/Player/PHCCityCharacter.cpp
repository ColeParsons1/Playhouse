// Copyright PlayhouseCity. All Rights Reserved.

#include "Player/PHCCityCharacter.h"
#include "PlayhouseCity.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Core/PHCPlayerController.h"
#include "Vehicle/PHCMissileProjectile.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Constructor — EXACT same root component pattern as APHCVehiclePawn
// ---------------------------------------------------------------------------

APHCCityCharacter::APHCCityCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// ---- Root: UBoxComponent — same pattern as vehicle ----
	// Vehicle uses: QueryAndPhysics, Block all, SimulatePhysics=false, HiddenInGame
	CharacterCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CharacterCollision"));
	CharacterCollision->SetBoxExtent(FVector(20.f, 20.f, 88.f));
	CharacterCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CharacterCollision->SetCollisionResponseToAllChannels(ECR_Block);
	CharacterCollision->SetSimulatePhysics(false);
	CharacterCollision->SetHiddenInGame(true);
	SetRootComponent(CharacterCollision);

	// Force AlwaysSpawn — never let UE adjust our spawn position.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// ---- Camera boom (Fortnite-style third-person) ----------------------
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->SocketOffset = FVector(0.f, 75.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 8.f;

	// ---- Follow camera ---------------------------------------------------
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Don't rotate the pawn body to match the controller/camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// ---- Blocky Roblox R6 character mesh --------------------------------
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	auto MakeBodyPart = [&](const TCHAR* Name,
		const FVector& Loc, const FVector& SizeUU) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetSimulatePhysics(false);
		C->CastShadow = false;
		if (CubeMesh)
		{
			C->SetStaticMesh(CubeMesh);
			C->SetRelativeScale3D(SizeUU / 100.f);
			C->SetRelativeLocation(Loc);
		}
		return C;
	};

	HeadMesh     = MakeBodyPart(TEXT("HeadMesh"),     FVector(0.f,   0.f,  44.f),   FVector(28.f, 34.f, 34.f));
	TorsoMesh    = MakeBodyPart(TEXT("TorsoMesh"),    FVector(0.f,   0.f,  -4.f),   FVector(28.f, 56.f, 56.f));
	LeftArmMesh  = MakeBodyPart(TEXT("LeftArmMesh"),  FVector(0.f, -38.f,  -4.f),   FVector(28.f, 16.f, 56.f));
	RightArmMesh = MakeBodyPart(TEXT("RightArmMesh"), FVector(0.f,  38.f,  -4.f),   FVector(28.f, 16.f, 56.f));
	LeftLegMesh  = MakeBodyPart(TEXT("LeftLegMesh"),  FVector(0.f, -15.f, -60.f),   FVector(28.f, 24.f, 56.f));
	RightLegMesh = MakeBodyPart(TEXT("RightLegMesh"), FVector(0.f,  15.f, -60.f),   FVector(28.f, 24.f, 56.f));

	// Inflate bounds (same as vehicle) to prevent frustum-culling on mobile
	CharacterCollision->SetBoundsScale(10.f);
}

// ---------------------------------------------------------------------------
// Input asset creation (programmatic -- no .uasset files needed)
// ---------------------------------------------------------------------------

void APHCCityCharacter::CreateInputAssets()
{
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	LookAction = NewObject<UInputAction>(this, TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

	JumpAction = NewObject<UInputAction>(this, TEXT("IA_Jump"));
	JumpAction->ValueType = EInputActionValueType::Boolean;

	SprintAction = NewObject<UInputAction>(this, TEXT("IA_Sprint"));
	SprintAction->ValueType = EInputActionValueType::Boolean;

	InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact"));
	InteractAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_CityDefault"));

	// WASD
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this);
		S->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(S);
	}
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this);
		S->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(S);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	DefaultMappingContext->MapKey(MoveAction, EKeys::D);
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}

	// Mouse look (desktop only)
#if !PLATFORM_IOS && !PLATFORM_ANDROID
	DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
#endif

	DefaultMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);
	DefaultMappingContext->MapKey(InteractAction, EKeys::E);
	DefaultMappingContext->MapKey(InteractAction, EKeys::F);

	// Gamepad
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Up);
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this);
		S->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(S);
	}
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Down);
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this);
		S->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(S);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Right);
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_LeftStick_Left);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Up);
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this);
		S->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(S);
	}
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Down);
		UInputModifierSwizzleAxis* S = NewObject<UInputModifierSwizzleAxis>(this);
		S->Order = EInputAxisSwizzle::YXZ;
		M.Modifiers.Add(S);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Right);
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_RightStick_Left);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	DefaultMappingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	DefaultMappingContext->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);
	DefaultMappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Left);
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void APHCCityCharacter::BeginPlay()
{
	Super::BeginPlay();

	CreateInputAssets();

	// Register the mapping context.
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Sub->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// Bind input actions.
	if (InputComponent)
	{
		UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
		if (EIC)
		{
			if (MoveAction)   EIC->BindAction(MoveAction,   ETriggerEvent::Triggered, this, &APHCCityCharacter::HandleMove);
			if (LookAction)   EIC->BindAction(LookAction,   ETriggerEvent::Triggered, this, &APHCCityCharacter::HandleLook);
			if (JumpAction)
			{
				EIC->BindAction(JumpAction, ETriggerEvent::Started,   this, &APHCCityCharacter::HandleJumpStarted);
				EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &APHCCityCharacter::HandleJumpCompleted);
			}
			if (SprintAction)
			{
				EIC->BindAction(SprintAction, ETriggerEvent::Started,   this, &APHCCityCharacter::HandleSprintStarted);
				EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &APHCCityCharacter::HandleSprintCompleted);
			}
			if (InteractAction) EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &APHCCityCharacter::HandleInteract);
		}
	}

	// ---- Apply Roblox character materials --------------------------------
	UMaterial* ColorMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));

	auto ApplyBodyColor = [&](UStaticMeshComponent* Comp,
		const FLinearColor& Color, float Roughness = 0.7f)
	{
		if (!Comp || !Comp->GetStaticMesh() || !ColorMat) return;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ColorMat, Comp);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			Comp->SetMaterial(0, MID);
		}
	};

	const FLinearColor SkinYellow(0.92f, 0.80f, 0.16f);
	const FLinearColor ShirtBlue(0.05f, 0.38f, 0.85f);
	const FLinearColor PantsGreen(0.16f, 0.50f, 0.13f);

	ApplyBodyColor(HeadMesh,     SkinYellow,  0.85f);
	ApplyBodyColor(TorsoMesh,    ShirtBlue,   0.85f);
	ApplyBodyColor(LeftArmMesh,  SkinYellow,  0.85f);
	ApplyBodyColor(RightArmMesh, SkinYellow,  0.85f);
	ApplyBodyColor(LeftLegMesh,  PantsGreen,  0.85f);
	ApplyBodyColor(RightLegMesh, PantsGreen,  0.85f);

	// Mark as grounded immediately — ground plane has no collision so the
	// downward trace won't find it.  Character is spawned at Z=0 (ground level).
	bHasBeenGrounded = true;

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityCharacter::BeginPlay -- Pawn at %s"),
		*GetActorLocation().ToString());
}

// ---------------------------------------------------------------------------
// Tick — EXACT same ground-following pattern as APHCVehiclePawn::Tick
// ---------------------------------------------------------------------------

void APHCCityCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ---- Process deferred vehicle interaction ---------------------------
	if (bPendingVehicleInteract)
	{
		bPendingVehicleInteract = false;
		if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetController()))
		{
			PC->EnterNearestVehicle();
			return;
		}
	}

	// ---- Accumulate touch movement into PendingMoveInput ----------------
	if (bHUDTouchActive && GetController())
	{
		if (!FMath::IsNearlyZero(TouchMoveX) || !FMath::IsNearlyZero(TouchMoveY))
		{
			const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
			const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector Rt  = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			PendingMoveInput += Fwd * TouchMoveY + Rt * TouchMoveX;
		}

		if (!FMath::IsNearlyZero(TouchLookX) || !FMath::IsNearlyZero(TouchLookY))
		{
			AddControllerYawInput(TouchLookX);
			AddControllerPitchInput(TouchLookY);
		}
	}

	// ---- Build horizontal movement direction ----------------------------
	FVector MoveDir = PendingMoveInput;
	MoveDir.Z = 0.f;
	if (!MoveDir.IsNearlyZero())
	{
		MoveDir = MoveDir.GetClampedToMaxSize(1.f);
	}
	PendingMoveInput = FVector::ZeroVector;

	// ---- Horizontal movement (same as vehicle: AddActorWorldOffset) -----
	const float Speed = bIsSprinting ? SprintSpeed : WalkSpeed;
	if (!MoveDir.IsNearlyZero())
	{
		const FVector Delta = MoveDir * Speed * DeltaTime;
		AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
	}

	// ---- Ground following (EXACT copy of APHCVehiclePawn ground trace) --
	{
		const FVector Pos = GetActorLocation();
		const FVector TraceStart(Pos.X, Pos.Y, Pos.Z + 5000.f);
		const FVector TraceEnd(Pos.X, Pos.Y, Pos.Z - 2000.f);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		TArray<FHitResult> Hits;
		bool bFoundSurface = false;
		if (GetWorld()->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (Hit.bStartPenetrating) continue;

				const float TargetZ = Hit.ImpactPoint.Z;

				if (TargetZ >= Pos.Z - 100.f)
				{
					bFoundSurface = true;
					bHasBeenGrounded = true;
					ManualVelocityZ = 0.f;
					const float NewZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, 12.f);
					SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
					break;
				}
			}
		}
		if (!bFoundSurface && bHasBeenGrounded)
		{
			// Airborne: apply gravity (same as vehicle)
			if (bWasGrounded)
			{
				ManualVelocityZ = 0.f;
			}
			ManualVelocityZ -= 980.f * DeltaTime;
			float NewZ = Pos.Z + ManualVelocityZ * DeltaTime;
			// Floor clamp — ground plane is at Z=0 but has no collision.
			if (NewZ <= 85.f)
			{
				NewZ = 85.f;
				ManualVelocityZ = 0.f;
			}
			SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
		}
		bWasGrounded = bFoundSurface;
	}

	// ---- Orient body to movement direction ------------------------------
	if (!MoveDir.IsNearlyZero())
	{
		const FRotator DesiredRot = MoveDir.Rotation();
		const FRotator CurrentRot = GetActorRotation();
		const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, 10.f);
		SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
	}

	// =====================================================================
	//  R6 Walk Animation (Roblox Brookhaven contralateral gait)
	//
	//  Phase relationships (standard human gait):
	//    Left Leg  + Right Arm  swing forward together
	//    Right Leg + Left Arm   swing forward together
	//  Limbs rotate as pendulums around shoulder/hip pivot points.
	//  Torso leans forward slightly and bobs vertically at 2x step freq.
	// =====================================================================
	{
		const bool bMoving = !MoveDir.IsNearlyZero();

		// Smooth blend: ramp up to 1 when moving, decay to 0 when stopped.
		const float TargetBlend = bMoving ? 1.f : 0.f;
		WalkAnimBlend = FMath::FInterpTo(WalkAnimBlend, TargetBlend, DeltaTime, 8.f);

		// Accumulate cycle phase only while moving (speed-proportional).
		// Base speed is now 25 mph (a run), so use run-level cycle speeds.
		if (bMoving)
		{
			const float CycleSpeed = bIsSprinting ? 16.f : 13.f; // rad/s
			WalkCyclePhase += CycleSpeed * DeltaTime;
		}

		// Swing amplitudes (degrees) — running gait at 25 mph base.
		const float LegAmp  = bIsSprinting ? 60.f : 50.f;
		const float ArmAmp  = bIsSprinting ? 50.f : 40.f;
		const float LeanAmp = bIsSprinting ? 12.f :  8.f; // torso pitch
		const float BobAmp  = bIsSprinting ?  4.f :  3.f; // vertical UU

		const float Phase = WalkCyclePhase;
		const float Blend = WalkAnimBlend;

		// Per-limb swing angles (contralateral: left leg ↔ right arm).
		const float LeftLegAngle  =  FMath::Sin(Phase) * LegAmp * Blend;
		const float RightLegAngle = -LeftLegAngle;
		const float RightArmAngle =  FMath::Sin(Phase) * ArmAmp * Blend;
		const float LeftArmAngle  = -RightArmAngle;

		// Torso: forward lean + vertical bounce (2× step frequency).
		const float TorsoLean  = LeanAmp * Blend;
		const float VertBounce = FMath::Cos(Phase * 2.f) * BobAmp * Blend;

		// Pivot geometry (must match constructor rest positions).
		// Shoulder pivot Z = torso center (-4) + half torso height (28) = 24
		// Hip pivot Z      = torso center (-4) - half torso height (28) = -32
		const float ShoulderZ   = 24.f;
		const float HipZ        = -32.f;
		const float LimbHalfLen = 28.f; // half of 56 UU limb

		// Rotate a limb around its pivot and apply position + rotation.
		auto AnimateLimb = [&](UStaticMeshComponent* Mesh,
			float RestY, float PivotZ, float SwingDeg)
		{
			if (!Mesh) return;
			const FVector RestOffset(0.f, 0.f, -LimbHalfLen);
			const FRotator SwingRot(SwingDeg, 0.f, 0.f);
			const FVector RotatedOffset = SwingRot.RotateVector(RestOffset);
			Mesh->SetRelativeLocation(FVector(RotatedOffset.X, RestY, PivotZ + RotatedOffset.Z));
			Mesh->SetRelativeRotation(FRotator(SwingDeg, 0.f, 0.f));
		};

		AnimateLimb(LeftArmMesh,  -38.f, ShoulderZ, LeftArmAngle);
		AnimateLimb(RightArmMesh,  38.f, ShoulderZ, RightArmAngle);
		AnimateLimb(LeftLegMesh,  -15.f, HipZ,      LeftLegAngle);
		AnimateLimb(RightLegMesh,  15.f, HipZ,      RightLegAngle);

		// Torso: forward lean + bounce.
		if (TorsoMesh)
		{
			TorsoMesh->SetRelativeLocation(FVector(0.f, 0.f, -4.f + VertBounce));
			TorsoMesh->SetRelativeRotation(FRotator(TorsoLean, 0.f, 0.f));
		}

		// Head: half bounce (dampened), stays upright.
		if (HeadMesh)
		{
			HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 44.f + VertBounce * 0.5f));
		}
	}
}

// ---------------------------------------------------------------------------
// Input binding
// ---------------------------------------------------------------------------

void APHCCityCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void APHCCityCharacter::HandleMove(const FInputActionValue& Value)
{
	AController* MyController = GetController();
	if (!MyController) return;

	const FVector2D MoveVector = Value.Get<FVector2D>();
	if (MoveVector.IsNearlyZero()) return;

	const FRotator YawRotation(0.f, MyController->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	PendingMoveInput += ForwardDir * MoveVector.Y + RightDir * MoveVector.X;
}

void APHCCityCharacter::HandleLook(const FInputActionValue& Value)
{
	if (!GetController()) return;
	const FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X);
	AddControllerPitchInput(LookVector.Y);
}

void APHCCityCharacter::HandleJumpStarted(const FInputActionValue& /*Value*/)
{
	if (bHasBeenGrounded)
	{
		ManualVelocityZ = 420.f;
		bHasBeenGrounded = true; // stays true
		bWasGrounded = false;    // force airborne next frame
	}
}

void APHCCityCharacter::HandleJumpCompleted(const FInputActionValue& /*Value*/)
{
}

void APHCCityCharacter::HandleSprintStarted(const FInputActionValue& /*Value*/)
{
	bIsSprinting = true;
}

void APHCCityCharacter::HandleSprintCompleted(const FInputActionValue& /*Value*/)
{
	bIsSprinting = false;
}

void APHCCityCharacter::HandleInteract(const FInputActionValue& /*Value*/)
{
	bPendingVehicleInteract = true;
}

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------

void APHCCityCharacter::FireWeapon()
{
	if (CurrentWeaponType < 0) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Spawn point: slightly in front of character at hand height
	const FVector SpawnLoc = GetActorLocation()
		+ GetActorForwardVector() * 120.f
		+ FVector(0.f, 0.f, 20.f);
	const FRotator SpawnRot = GetControlRotation();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (CurrentWeaponType == 0) // Gun
	{
		APHCMissileProjectile* Bullet = World->SpawnActor<APHCMissileProjectile>(
			APHCMissileProjectile::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Bullet)
		{
			Bullet->Speed = 25000.f;
			Bullet->SetLifeSpan(0.8f);
			if (Bullet->ProjectileMesh)
			{
				Bullet->ProjectileMesh->SetRelativeScale3D(FVector(0.6f, 0.06f, 0.06f));
			}
			Bullet->StartTrail(FLinearColor(1.f, 0.85f, 0.2f));
		}
	}
	else if (CurrentWeaponType == 1) // RPG
	{
		APHCMissileProjectile* Rocket = World->SpawnActor<APHCMissileProjectile>(
			APHCMissileProjectile::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Rocket)
		{
			Rocket->Speed = 12000.f;
			Rocket->bUseManualGravity = true;
			Rocket->ManualGravityScale = 0.4f;
			Rocket->SetLifeSpan(3.f);
			if (Rocket->ProjectileMesh)
			{
				Rocket->ProjectileMesh->SetRelativeScale3D(FVector(2.5f, 0.25f, 0.25f));
			}
			Rocket->StartTrail(FLinearColor(1.f, 0.3f, 0.05f));
		}
	}
	else if (CurrentWeaponType == 2) // Sword
	{
		// Melee: sphere sweep 250 UU in front of character
		const FVector SweepStart = GetActorLocation() + FVector(0.f, 0.f, 20.f);
		const FVector SweepEnd = SweepStart + GetActorForwardVector() * 250.f;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, SweepStart, SweepEnd, FQuat::Identity,
			ECC_Pawn, FCollisionShape::MakeSphere(60.f), Params);

		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor())
			{
				UE_LOG(LogPlayhouseCity, Log, TEXT("Sword hit: %s"), *Hit.GetActor()->GetName());
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCCityCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCCityCharacter, bIsSprinting);
}
