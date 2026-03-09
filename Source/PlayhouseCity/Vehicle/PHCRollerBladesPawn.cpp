// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCRollerBladesPawn.h"
#include "PlayhouseCity.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Core/PHCCitySubsystem.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCRollerBladesPawn::APHCRollerBladesPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 15.f;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cyl  = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	// ========================================================================
	// Roller Blades — inline skates worn on the character's feet
	//
	// The pawn acts as an invisible vehicle. The character attaches to it
	// and remains visible. Boot/wheel meshes replace the character's shoes.
	// ========================================================================

	// ---- Collision box (root, character-sized) ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(30.f, 30.f, 30.f));
	VehicleCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleCollision->SetCollisionResponseToAllChannels(ECR_Block);
	VehicleCollision->SetSimulatePhysics(false);
	VehicleCollision->SetHiddenInGame(true);
	SetRootComponent(VehicleCollision);

	auto MakeCube = [&](const TCHAR* Name, const FVector& Loc,
		const FVector& SizeUU) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(VehicleCollision);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cube)
		{
			C->SetStaticMesh(Cube);
			C->SetRelativeScale3D(SizeUU / 100.f);
			C->SetRelativeLocation(Loc);
		}
		return C;
	};

	auto MakeCyl = [&](const TCHAR* Name, const FVector& Loc,
		const FVector& Scale, const FRotator& Rot = FRotator(0.f, 0.f, 90.f)) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetupAttachment(VehicleCollision);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cyl)
		{
			W->SetStaticMesh(Cyl);
			W->SetRelativeScale3D(Scale);
			W->SetRelativeLocation(Loc);
			W->SetRelativeRotation(Rot);
		}
		return W;
	};

	// ====================================================================
	//  BOOT MESHES — positioned to align with character feet
	//  Character attaches at (0, 0, 75), so feet are at ~Z=7, soles ~Z=2
	// ====================================================================

	// Left boot (slightly larger than the character's shoe)
	LeftBootMesh = MakeCube(TEXT("LeftBoot"),
		FVector(4.f, -11.f, 7.f), FVector(26.f, 16.f, 14.f));

	// Right boot
	RightBootMesh = MakeCube(TEXT("RightBoot"),
		FVector(4.f, 11.f, 7.f), FVector(26.f, 16.f, 14.f));

	// ====================================================================
	//  FRAME RAILS — narrow rail under each boot holding the wheels
	// ====================================================================

	LeftFrameMesh = MakeCube(TEXT("LeftFrame"),
		FVector(4.f, -11.f, -2.f), FVector(30.f, 5.f, 4.f));

	RightFrameMesh = MakeCube(TEXT("RightFrame"),
		FVector(4.f, 11.f, -2.f), FVector(30.f, 5.f, 4.f));

	// ====================================================================
	//  WHEELS — 4 inline per foot, evenly spaced front-to-back
	// ====================================================================

	const float WheelZ = -6.f;
	const FVector WheelScale(0.035f, 0.035f, 0.04f);
	const FRotator WheelRot(0.f, 0.f, 90.f);

	// Left wheels (X positions: -5, 1, 7, 13)
	LeftWheel1 = MakeCyl(TEXT("LWheel1"), FVector(-5.f, -11.f, WheelZ), WheelScale, WheelRot);
	LeftWheel2 = MakeCyl(TEXT("LWheel2"), FVector( 1.f, -11.f, WheelZ), WheelScale, WheelRot);
	LeftWheel3 = MakeCyl(TEXT("LWheel3"), FVector( 7.f, -11.f, WheelZ), WheelScale, WheelRot);
	LeftWheel4 = MakeCyl(TEXT("LWheel4"), FVector(13.f, -11.f, WheelZ), WheelScale, WheelRot);

	// Right wheels
	RightWheel1 = MakeCyl(TEXT("RWheel1"), FVector(-5.f, 11.f, WheelZ), WheelScale, WheelRot);
	RightWheel2 = MakeCyl(TEXT("RWheel2"), FVector( 1.f, 11.f, WheelZ), WheelScale, WheelRot);
	RightWheel3 = MakeCyl(TEXT("RWheel3"), FVector( 7.f, 11.f, WheelZ), WheelScale, WheelRot);
	RightWheel4 = MakeCyl(TEXT("RWheel4"), FVector(13.f, 11.f, WheelZ), WheelScale, WheelRot);

	// ====================================================================
	//  CAMERA
	// ====================================================================

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleCollision);
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 180.f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 6.f;

	VehicleCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VehicleCamera"));
	VehicleCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	VehicleCamera->bUsePawnControlRotation = false;

	// Prevent culling
	VehicleCollision->SetBoundsScale(200.f);
	TArray<UStaticMeshComponent*> AllMeshes;
	GetComponents<UStaticMeshComponent>(AllMeshes);
	for (UStaticMeshComponent* MC : AllMeshes)
	{
		MC->bUseAttachParentBound = true;
		MC->SetBoundsScale(200.f);
		MC->bNeverDistanceCull = true;
	}
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void APHCRollerBladesPawn::BeginPlay()
{
	Super::BeginPlay();
	CreateInputAssets();

	const FLinearColor BootCyan(0.1f, 0.85f, 0.6f);
	const FLinearColor DarkMetal(0.08f, 0.08f, 0.08f);
	const FLinearColor WheelBlack(0.03f, 0.03f, 0.03f);

	auto ApplyMat = [](UStaticMeshComponent* Comp, const FLinearColor& Color,
		float Metallic = 0.f, float Roughness = 0.5f,
		const FLinearColor& Emissive = FLinearColor::Black)
	{
		if (!Comp || !Comp->GetStaticMesh()) return;
		UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
		if (!M)
		{
			M = UMaterialInstanceDynamic::Create(Comp->GetMaterial(0), Comp);
			if (M) Comp->SetMaterial(0, M);
		}
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), Color);
			if (Metallic != 0.f)
				M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			if (Roughness != 0.5f)
				M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			if (Emissive != FLinearColor::Black)
				M->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
		}
	};

	// Boots — neon cyan, slightly metallic
	ApplyMat(LeftBootMesh,  BootCyan, 0.3f, 0.4f);
	ApplyMat(RightBootMesh, BootCyan, 0.3f, 0.4f);

	// Frame rails — dark metal
	ApplyMat(LeftFrameMesh,  DarkMetal, 0.6f, 0.35f);
	ApplyMat(RightFrameMesh, DarkMetal, 0.6f, 0.35f);

	// Wheels — black rubber
	ApplyMat(LeftWheel1,  WheelBlack, 0.0f, 0.95f);
	ApplyMat(LeftWheel2,  WheelBlack, 0.0f, 0.95f);
	ApplyMat(LeftWheel3,  WheelBlack, 0.0f, 0.95f);
	ApplyMat(LeftWheel4,  WheelBlack, 0.0f, 0.95f);
	ApplyMat(RightWheel1, WheelBlack, 0.0f, 0.95f);
	ApplyMat(RightWheel2, WheelBlack, 0.0f, 0.95f);
	ApplyMat(RightWheel3, WheelBlack, 0.0f, 0.95f);
	ApplyMat(RightWheel4, WheelBlack, 0.0f, 0.95f);
}

// ---------------------------------------------------------------------------
// CreateInputAssets (identical controls to dirt bike)
// ---------------------------------------------------------------------------

void APHCRollerBladesPawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_BladesThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_BladesBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_BladesSteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_BladesExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_RollerBlades"));

	// Throttle
	VehicleMappingContext->MapKey(ThrottleAction, EKeys::W);
	VehicleMappingContext->MapKey(ThrottleAction, EKeys::Up);

	// Brake
	VehicleMappingContext->MapKey(BrakeAction, EKeys::S);
	VehicleMappingContext->MapKey(BrakeAction, EKeys::Down);

	// Steer
	VehicleMappingContext->MapKey(SteerAction, EKeys::D);
	VehicleMappingContext->MapKey(SteerAction, EKeys::Right);
	{
		FEnhancedActionKeyMapping& M = VehicleMappingContext->MapKey(SteerAction, EKeys::A);
		UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(this);
		M.Modifiers.Add(Neg);
	}
	{
		FEnhancedActionKeyMapping& M = VehicleMappingContext->MapKey(SteerAction, EKeys::Left);
		UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(this);
		M.Modifiers.Add(Neg);
	}

	// Exit: F
	VehicleMappingContext->MapKey(ExitVehicleAction, EKeys::F);

	// Gamepad
	VehicleMappingContext->MapKey(ThrottleAction, EKeys::Gamepad_RightTrigger);
	VehicleMappingContext->MapKey(BrakeAction, EKeys::Gamepad_LeftTrigger);
	VehicleMappingContext->MapKey(SteerAction, EKeys::Gamepad_LeftStick_Right);
	{
		FEnhancedActionKeyMapping& M = VehicleMappingContext->MapKey(SteerAction, EKeys::Gamepad_LeftStick_Left);
		UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(this);
		M.Modifiers.Add(Neg);
	}
	VehicleMappingContext->MapKey(ExitVehicleAction, EKeys::Gamepad_FaceButton_Left);
}

// ---------------------------------------------------------------------------
// SetupPlayerInputComponent
// ---------------------------------------------------------------------------

void APHCRollerBladesPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (VehicleMappingContext)
			{
				Sub->AddMappingContext(VehicleMappingContext, 1);
			}
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EIC)
	{
		if (ThrottleAction)
		{
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCRollerBladesPawn::HandleThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCRollerBladesPawn::HandleThrottleRelease);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCRollerBladesPawn::HandleBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCRollerBladesPawn::HandleBrakeRelease);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCRollerBladesPawn::HandleSteer);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCRollerBladesPawn::HandleSteerRelease);
		}
		if (ExitVehicleAction)
		{
			EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCRollerBladesPawn::HandleExitVehicle);
		}
	}
}

// ---------------------------------------------------------------------------
// Tick (arcade physics + terrain following, like dirt bike)
// ---------------------------------------------------------------------------

void APHCRollerBladesPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsOccupied) return;

	// ---- Speed update ----
	if (ThrottleInput > 0.f)
	{
		const float SpeedRatio = FMath::Clamp(CurrentSpeed / MaxSpeed, 0.f, 1.f);
		const float AccelCurve = 1.f - SpeedRatio * SpeedRatio;
		CurrentSpeed += Acceleration * AccelCurve * DeltaTime;
	}
	else if (ThrottleInput < 0.f)
	{
		CurrentSpeed -= BrakeDeceleration * DeltaTime;
	}
	else
	{
		if (CurrentSpeed > 0.f)
		{
			CurrentSpeed -= NaturalDeceleration * DeltaTime;
			if (CurrentSpeed < 0.f) CurrentSpeed = 0.f;
		}
		else if (CurrentSpeed < 0.f)
		{
			CurrentSpeed += NaturalDeceleration * DeltaTime;
			if (CurrentSpeed > 0.f) CurrentSpeed = 0.f;
		}
	}
	CurrentSpeed = FMath::Clamp(CurrentSpeed, -MaxSpeed * 0.15f, MaxSpeed);

	// ---- Steering / Yaw (no lean — character stays upright) ----
	if (FMath::Abs(CurrentSpeed) > 10.f)
	{
		const float NormSpeed = FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxSpeed, 0.f, 1.f);
		const float SpeedFactor = FMath::Lerp(1.0f, 0.3f, NormSpeed);
		const float YawDelta = SteeringInput * SteeringSpeed * SpeedFactor * DeltaTime;
		FRotator Rot = GetActorRotation();
		Rot.Yaw += YawDelta;
		Rot.Roll = 0.f;  // Stay upright
		SetActorRotation(Rot);
	}

	// ---- Movement ----
	if (FMath::Abs(CurrentSpeed) > 1.f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Delta = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal() * CurrentSpeed * DeltaTime;
		AddActorWorldOffset(Delta, false);
	}

	// ---- Wall detection (line trace at chest height, every other frame) ----
	WallTraceFrameCounter++;
	if ((WallTraceFrameCounter & 1) == 0)
	{
		const FVector Pos = GetActorLocation();
		const FVector MoveDir = GetActorForwardVector();
		const float MoveLen = FMath::Abs(CurrentSpeed) * DeltaTime;
		const FVector TraceStart = Pos + FVector(0.f, 0.f, 400.f);
		const FVector TraceEnd = TraceStart + FVector(MoveDir.X, MoveDir.Y, 0.f).GetSafeNormal() * (MoveLen + 120.f);

		FCollisionQueryParams WallParams;
		WallParams.AddIgnoredActor(this);
		FHitResult WallHit;
		if (GetWorld()->LineTraceSingleByChannel(WallHit, TraceStart, TraceEnd, ECC_WorldStatic, WallParams))
		{
			if (!WallHit.bStartPenetrating && WallHit.Distance < MoveLen + 60.f
				&& WallHit.ImpactNormal.Z < 0.5f)
			{
				CurrentSpeed *= 0.1f;
			}
		}
	}

	// ---- Ground following (line trace + terrain height) ----
	{
		const FVector Pos = GetActorLocation();
		const float FootOffset = 8.f; // Roller blades sit low to ground
		float BestZ = -1e9f;
		bool bFoundSurface = false;

		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(this);
		FHitResult GroundHit;
		const FVector Start(Pos.X, Pos.Y, Pos.Z + 500.f);
		const FVector End(Pos.X, Pos.Y, Pos.Z - 1000.f);
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, Start, End, ECC_WorldStatic, TraceParams))
		{
			if (!GroundHit.bStartPenetrating)
			{
				BestZ = GroundHit.ImpactPoint.Z + FootOffset;
				bFoundSurface = true;
			}
		}

		// Cached terrain height (every 3 frames or when moved >200 UU)
		TerrainCacheFrameCounter++;
		const FVector2D CurrentPos2D(Pos.X, Pos.Y);
		const float DistFromCache = FVector2D::Distance(CurrentPos2D, CachedTerrainPos);
		if (TerrainCacheFrameCounter >= 3 || DistFromCache > 200.f)
		{
			TerrainCacheFrameCounter = 0;
			CachedTerrainPos = CurrentPos2D;
			if (UPHCCitySubsystem* CitySub = GetWorld()->GetSubsystem<UPHCCitySubsystem>())
			{
				CachedTerrainZ = CitySub->GetTerrainHeight(Pos.X, Pos.Y);
			}
		}

		const float TerrainZ = CachedTerrainZ + FootOffset;
		if (TerrainZ > BestZ)
		{
			BestZ = TerrainZ;
			bFoundSurface = true;
		}

		if (bFoundSurface)
		{
			const float TargetZ = BestZ;
			if (Pos.Z > TargetZ + 2.f)
			{
				// Falling — apply gravity
				VerticalVelocity -= 980.f * DeltaTime;
				const float NewZ = Pos.Z + VerticalVelocity * DeltaTime;
				if (NewZ <= TargetZ)
				{
					SetActorLocation(FVector(Pos.X, Pos.Y, TargetZ));
					VerticalVelocity = 0.f;
				}
				else
				{
					SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
				}
			}
			else
			{
				// On or below ground — smooth interpolation (slightly faster than bike for grace)
				const float SmoothZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, 25.f);
				SetActorLocation(FVector(Pos.X, Pos.Y, SmoothZ));
				VerticalVelocity = 0.f;
			}
		}
	}

	// ---- Wheel spin animation ----
	{
		const float WheelRadius = 3.5f; // Small inline wheels
		const float AngularSpeed = (WheelRadius > 0.f)
			? FMath::RadiansToDegrees(CurrentSpeed / WheelRadius)
			: 0.f;
		WheelSpinAngle += AngularSpeed * DeltaTime;

		const FRotator SpinRot(WheelSpinAngle, 0.f, 90.f);
		if (LeftWheel1)  LeftWheel1->SetRelativeRotation(SpinRot);
		if (LeftWheel2)  LeftWheel2->SetRelativeRotation(SpinRot);
		if (LeftWheel3)  LeftWheel3->SetRelativeRotation(SpinRot);
		if (LeftWheel4)  LeftWheel4->SetRelativeRotation(SpinRot);
		if (RightWheel1) RightWheel1->SetRelativeRotation(SpinRot);
		if (RightWheel2) RightWheel2->SetRelativeRotation(SpinRot);
		if (RightWheel3) RightWheel3->SetRelativeRotation(SpinRot);
		if (RightWheel4) RightWheel4->SetRelativeRotation(SpinRot);
	}
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void APHCRollerBladesPawn::HandleThrottle(const FInputActionValue&)
{
	ThrottleInput = 1.f;
}
void APHCRollerBladesPawn::HandleThrottleRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
}
void APHCRollerBladesPawn::HandleBrake(const FInputActionValue&)
{
	ThrottleInput = -1.f;
}
void APHCRollerBladesPawn::HandleBrakeRelease(const FInputActionValue&)
{
	if (ThrottleInput < 0.f)
	{
		ThrottleInput = 0.f;
	}
}
void APHCRollerBladesPawn::HandleSteer(const FInputActionValue& Value)
{
	SteeringInput = Value.Get<float>();
}
void APHCRollerBladesPawn::HandleSteerRelease(const FInputActionValue&)
{
	SteeringInput = 0.f;
}
void APHCRollerBladesPawn::HandleExitVehicle(const FInputActionValue&)
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (VehicleMappingContext)
				Sub->RemoveMappingContext(VehicleMappingContext);
		}
		static const FName ExitFuncName(TEXT("OnVehicleExitRequested"));
		UFunction* Func = PC->FindFunction(ExitFuncName);
		if (Func) PC->ProcessEvent(Func, nullptr);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FVector APHCRollerBladesPawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -200.f + FVector(0.f, 0.f, 50.f);
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCRollerBladesPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCRollerBladesPawn, bIsOccupied);
}
