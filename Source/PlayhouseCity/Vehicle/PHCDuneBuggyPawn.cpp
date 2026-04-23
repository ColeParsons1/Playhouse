// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCDuneBuggyPawn.h"
#include "Vehicle/PHCNukeExplosion.h"
#include "Audio/PHCAudio.h"
#include "Components/AudioComponent.h"
#include "PlayhouseCity.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
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
#include "Vehicle/PHCMissileProjectile.h"
#include "Components/SphereComponent.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCDuneBuggyPawn::APHCDuneBuggyPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// ---- Multiplayer replication ----
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
	// Dune Buggy – open-frame off-road vehicle
	//
	// ~350 long × 170 wide × ~160 tall (with roll cage).
	// Higher ride, open cockpit, tubular roll cage, brush guard.
	// Cube mesh = 100×100×100 at scale 1.0  →  scale = DesiredSize / 100.
	// ========================================================================

	// ---- Collision box (root) ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(175.f, 90.f, 30.f));
	VehicleCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleCollision->SetCollisionResponseToAllChannels(ECR_Block);
	VehicleCollision->SetSimulatePhysics(false);
	VehicleCollision->SetHiddenInGame(true);
	SetRootComponent(VehicleCollision);

	// Helpers
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
		const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetupAttachment(VehicleCollision);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cyl)
		{
			W->SetStaticMesh(Cyl);
			W->SetRelativeScale3D(Scale);
			W->SetRelativeLocation(Loc);
			W->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		}
		return W;
	};

	// ====================================================================
	//  BODY
	// ====================================================================

	// Main chassis slab (flat platform)
	ChassisMesh = MakeCube(TEXT("ChassisMesh"),
		FVector(0.f, 0.f, 0.f), FVector(280.f, 160.f, 12.f));
	BodyColoredMeshes.Add(ChassisMesh);

	// Floor pan (thin plate under chassis)
	FloorPanMesh = MakeCube(TEXT("FloorPanMesh"),
		FVector(0.f, 0.f, -8.f), FVector(250.f, 150.f, 4.f));
	BodyColoredMeshes.Add(FloorPanMesh);

	// ====================================================================
	//  ROLL CAGE (6 tubular bars forming open cage)
	// ====================================================================

	// Front pillars (A-pillars going up from chassis)
	RollCageTopL = MakeCube(TEXT("RollCageTopL"),
		FVector(-10.f, 68.f, 38.f), FVector(8.f, 8.f, 64.f));
	MetalFrameMeshes.Add(RollCageTopL);

	RollCageTopR = MakeCube(TEXT("RollCageTopR"),
		FVector(-10.f, -68.f, 38.f), FVector(8.f, 8.f, 64.f));
	MetalFrameMeshes.Add(RollCageTopR);

	// Top cross bar (connects front pillars at top)
	RollCageTopCross = MakeCube(TEXT("RollCageTopCross"),
		FVector(-10.f, 0.f, 68.f), FVector(8.f, 144.f, 8.f));
	MetalFrameMeshes.Add(RollCageTopCross);

	// Rear pillars
	RollCageRearL = MakeCube(TEXT("RollCageRearL"),
		FVector(-95.f, 60.f, 33.f), FVector(8.f, 8.f, 54.f));
	MetalFrameMeshes.Add(RollCageRearL);

	RollCageRearR = MakeCube(TEXT("RollCageRearR"),
		FVector(-95.f, -60.f, 33.f), FVector(8.f, 8.f, 54.f));
	MetalFrameMeshes.Add(RollCageRearR);

	// Rear cross bar
	RollCageRearCross = MakeCube(TEXT("RollCageRearCross"),
		FVector(-95.f, 0.f, 58.f), FVector(8.f, 128.f, 8.f));
	MetalFrameMeshes.Add(RollCageRearCross);

	// ====================================================================
	//  FRONT BRUSH GUARD (3 bars)
	// ====================================================================

	BrushGuardTop = MakeCube(TEXT("BrushGuardTop"),
		FVector(145.f, 0.f, 10.f), FVector(6.f, 140.f, 6.f));
	MetalFrameMeshes.Add(BrushGuardTop);

	BrushGuardL = MakeCube(TEXT("BrushGuardL"),
		FVector(145.f, 65.f, -5.f), FVector(6.f, 6.f, 24.f));
	MetalFrameMeshes.Add(BrushGuardL);

	BrushGuardR = MakeCube(TEXT("BrushGuardR"),
		FVector(145.f, -65.f, -5.f), FVector(6.f, 6.f, 24.f));
	MetalFrameMeshes.Add(BrushGuardR);

	// ====================================================================
	//  REAR SPARE TIRE
	// ====================================================================

	SpareTireMesh = MakeCyl(TEXT("SpareTireMesh"),
		FVector(-145.f, 0.f, 22.f), FVector(0.30f, 0.30f, 0.12f));

	// ====================================================================
	//  REAR ENGINE BLOCK
	// ====================================================================

	EngineBlockMesh = MakeCube(TEXT("EngineBlockMesh"),
		FVector(-75.f, 0.f, 12.f), FVector(60.f, 80.f, 24.f));

	// ====================================================================
	//  SEAT
	// ====================================================================

	SeatMesh = MakeCube(TEXT("SeatMesh"),
		FVector(15.f, 0.f, 14.f), FVector(40.f, 40.f, 22.f));

	// ====================================================================
	//  WHEELS (off-road: bigger diameter, wider than Aventador)
	// ====================================================================

	WheelFL = MakeCyl(TEXT("WheelFL"), FVector(110.f, 85.f, -18.f), FVector(0.40f, 0.40f, 0.14f));
	WheelFR = MakeCyl(TEXT("WheelFR"), FVector(110.f, -85.f, -18.f), FVector(0.40f, 0.40f, 0.14f));

	// Rear: slightly bigger
	WheelRL = MakeCyl(TEXT("WheelRL"), FVector(-100.f, 90.f, -16.f), FVector(0.42f, 0.42f, 0.16f));
	WheelRR = MakeCyl(TEXT("WheelRR"), FVector(-100.f, -90.f, -16.f), FVector(0.42f, 0.42f, 0.16f));

	// ====================================================================
	//  LIGHTS
	// ====================================================================

	HeadlightMeshL = MakeCube(TEXT("HeadlightL"),
		FVector(142.f, 45.f, 4.f), FVector(6.f, 16.f, 6.f));
	HeadlightMeshR = MakeCube(TEXT("HeadlightR"),
		FVector(142.f, -45.f, 4.f), FVector(6.f, 16.f, 6.f));

	TaillightMeshL = MakeCube(TEXT("TaillightL"),
		FVector(-140.f, 45.f, 8.f), FVector(4.f, 16.f, 6.f));
	TaillightMeshR = MakeCube(TEXT("TaillightR"),
		FVector(-140.f, -45.f, 8.f), FVector(4.f, 16.f, 6.f));

	// ---- Headlight spotlights ----
	HeadlightSpotL = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotL"));
	HeadlightSpotL->SetupAttachment(VehicleCollision);
	HeadlightSpotL->SetRelativeLocation(FVector(144.f, 45.f, 4.f));
	HeadlightSpotL->SetRelativeRotation(FRotator(-3.f, 0.f, 0.f));
	HeadlightSpotL->SetIntensity(80000.f);
	HeadlightSpotL->SetAttenuationRadius(6000.f);
	HeadlightSpotL->SetOuterConeAngle(28.f);
	HeadlightSpotL->SetInnerConeAngle(16.f);
	HeadlightSpotL->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotL->SetVisibility(false);

	HeadlightSpotR = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotR"));
	HeadlightSpotR->SetupAttachment(VehicleCollision);
	HeadlightSpotR->SetRelativeLocation(FVector(144.f, -45.f, 4.f));
	HeadlightSpotR->SetRelativeRotation(FRotator(-3.f, 0.f, 0.f));
	HeadlightSpotR->SetIntensity(80000.f);
	HeadlightSpotR->SetAttenuationRadius(6000.f);
	HeadlightSpotR->SetOuterConeAngle(28.f);
	HeadlightSpotR->SetInnerConeAngle(16.f);
	HeadlightSpotR->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotR->SetVisibility(false);

	// ---- Camera ----
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleCollision);
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 200.f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 7.f;

	VehicleCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VehicleCamera"));
	VehicleCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	VehicleCamera->bUsePawnControlRotation = false;

	// Prevent all forms of culling — parent bounds + massive scale + no distance cull + no occlusion
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

void APHCDuneBuggyPawn::BeginPlay()
{
	Super::BeginPlay();

	EngineAC = PHCAudio::SpawnAttached3DLoop(VehicleCollision,
		TEXT("/Game/Audio/SFX/Vehicles/CarEngine_Loop.CarEngine_Loop"),
		0.f, 0.85f);

	bHasBeenGrounded = true;

	CreateInputAssets();

	// ---- Apply materials ----

	const FLinearColor DefaultBody(0.35f, 0.42f, 0.18f); // olive/army green
	const FLinearColor DarkMetal(0.08f, 0.08f, 0.08f);
	const FLinearColor WheelBlack(0.04f, 0.04f, 0.04f);
	const FLinearColor EngineGray(0.06f, 0.06f, 0.06f);
	const FLinearColor SeatBrown(0.05f, 0.04f, 0.03f);
	const FLinearColor LEDWhite(3.0f, 2.8f, 2.2f);
	const FLinearColor TailRed(2.5f, 0.02f, 0.02f);

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

	// Load ClearCoat material for automotive paint (GTA V style)
	ClearCoatMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCClearCoat.M_PHCClearCoat"));

	// Body panels (chassis + floor pan) -- ClearCoat for glossy automotive paint.
	auto ApplyClearCoat = [&](UStaticMeshComponent* Comp, const FLinearColor& Color)
	{
		if (!Comp || !Comp->GetStaticMesh()) return;
		UMaterialInterface* BaseMat = ClearCoatMaterial ? (UMaterialInterface*)ClearCoatMaterial : Comp->GetMaterial(0);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BaseMat, Comp);
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), Color);
			M->SetScalarParameterValue(TEXT("Metallic"), 0.9f);
			M->SetScalarParameterValue(TEXT("Roughness"), 0.35f);
			Comp->SetMaterial(0, M);
		}
	};

	if (ChassisMesh && ChassisMesh->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = ClearCoatMaterial ? (UMaterialInterface*)ClearCoatMaterial : ChassisMesh->GetMaterial(0);
		BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, ChassisMesh);
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), DefaultBody);
			BodyMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.9f);
			BodyMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.35f);
			ChassisMesh->SetMaterial(0, BodyMaterial);
		}
	}
	for (UStaticMeshComponent* C : BodyColoredMeshes) { ApplyClearCoat(C, DefaultBody); }

	// Metal frame (roll cage + brush guard).
	for (UStaticMeshComponent* C : MetalFrameMeshes) { ApplyMat(C, DarkMetal, 0.85f, 0.3f); }

	// Engine block.
	ApplyMat(EngineBlockMesh, EngineGray, 0.8f, 0.35f);

	// Seat.
	ApplyMat(SeatMesh, SeatBrown, 0.0f, 0.65f);

	// Wheels.
	ApplyMat(WheelFL, WheelBlack, 0.0f, 0.9f);
	ApplyMat(WheelFR, WheelBlack, 0.0f, 0.9f);
	ApplyMat(WheelRL, WheelBlack, 0.0f, 0.9f);
	ApplyMat(WheelRR, WheelBlack, 0.0f, 0.9f);

	// Spare tire.
	ApplyMat(SpareTireMesh, WheelBlack, 0.0f, 0.9f);

	// Headlights (bright white LED).
	ApplyMat(HeadlightMeshL, LEDWhite, 0.0f, 0.1f, LEDWhite);
	ApplyMat(HeadlightMeshR, LEDWhite, 0.0f, 0.1f, LEDWhite);

	// Taillights (red).
	ApplyMat(TaillightMeshL, TailRed, 0.0f, 0.1f, TailRed);
	ApplyMat(TaillightMeshR, TailRed, 0.0f, 0.1f, TailRed);

}

// ---------------------------------------------------------------------------
// Input asset creation (identical to PHCVehiclePawn)
// ---------------------------------------------------------------------------

void APHCDuneBuggyPawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_BuggyThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_BuggyBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_BuggySteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_BuggyExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Buggy"));

	VehicleMappingContext->MapKey(ThrottleAction, EKeys::W);
	VehicleMappingContext->MapKey(ThrottleAction, EKeys::Up);
	VehicleMappingContext->MapKey(BrakeAction, EKeys::S);
	VehicleMappingContext->MapKey(BrakeAction, EKeys::Down);
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
	VehicleMappingContext->MapKey(ExitVehicleAction, EKeys::F);
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

void APHCDuneBuggyPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCDuneBuggyPawn::HandleThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCDuneBuggyPawn::HandleThrottleRelease);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCDuneBuggyPawn::HandleBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCDuneBuggyPawn::HandleBrakeRelease);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCDuneBuggyPawn::HandleSteer);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCDuneBuggyPawn::HandleSteerRelease);
		}
		if (ExitVehicleAction)
		{
			EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCDuneBuggyPawn::HandleExitVehicle);
		}
	}
}

// ---------------------------------------------------------------------------
// Tick -- arcade vehicle physics (same as Aventador, uses member tuning)
// ---------------------------------------------------------------------------

void APHCDuneBuggyPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (EngineAC)
	{
		const float SpeedRatio = FMath::Clamp(FMath::Abs(CurrentSpeed) / FMath::Max(MaxSpeed, 1.f), 0.f, 1.f);
		EngineAC->SetPitchMultiplier(FMath::Lerp(0.7f, 1.4f, SpeedRatio));
		EngineAC->SetVolumeMultiplier(bIsOccupied ? FMath::Lerp(0.4f, 1.0f, SpeedRatio) : 0.f);
	}

	// Damage flash
	if (DamageFlashTimer > 0.f)
	{
		DamageFlashTimer -= DeltaTime;
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("EmissiveColor"),
				DamageFlashTimer > 0.f ? FLinearColor(0.4f, 0.02f, 0.0f) : FLinearColor::Black);
		}
	}

	// Destruction
	if (bIsDestroyed && !IsActorBeingDestroyed())
	{
		UWorld* W = GetWorld();
		if (W)
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			W->SpawnActor<APHCNukeExplosion>(APHCNukeExplosion::StaticClass(),
				GetActorLocation(), FRotator::ZeroRotator, SP);
		}
		SetActorHiddenInGame(true);
		Destroy();
		return;
	}

	// Tick missile cooldown regardless of occupancy.
	FireCooldownRemaining = FMath::Max(0.f, FireCooldownRemaining - DeltaTime);

	if (!bIsOccupied) return;

	// ---- Speed update (non-linear – harder to gain speed near top) ----
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
	CurrentSpeed = FMath::Clamp(CurrentSpeed, -MaxSpeed * 0.3f, MaxSpeed);

	// ---- Steering (inverse speed: responsive at low speed, stable at high) ----
	if (FMath::Abs(CurrentSpeed) > 10.f)
	{
		const float NormSpeed = FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxSpeed, 0.f, 1.f);
		const float SpeedFactor = FMath::Lerp(1.0f, 0.3f, NormSpeed);
		const float YawDelta = SteeringInput * SteeringSpeed * SpeedFactor * DeltaTime;
		FRotator NewRotation = GetActorRotation();
		NewRotation.Yaw += YawDelta;
		SetActorRotation(NewRotation);
	}

	// ---- Wheel spin visual ----
	{
		WheelSpinAngle += CurrentSpeed * DeltaTime * 0.5f;
		const float SteerYaw = SteeringInput * 25.f;
		const FRotator FrontRot(WheelSpinAngle, SteerYaw, 90.f);
		if (WheelFL) WheelFL->SetRelativeRotation(FrontRot);
		if (WheelFR) WheelFR->SetRelativeRotation(FrontRot);
		const FRotator RearRot(WheelSpinAngle, 0.f, 90.f);
		if (WheelRL) WheelRL->SetRelativeRotation(RearRot);
		if (WheelRR) WheelRR->SetRelativeRotation(RearRot);
	}

	// ---- Movement (with wall collision line trace) ----
	if (FMath::Abs(CurrentSpeed) > 1.f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Delta = Forward * CurrentSpeed * DeltaTime;
		const FVector Pos = GetActorLocation();
		const float MoveLen = FVector(Delta.X, Delta.Y, 0.f).Size();
		const FVector MoveNorm = FVector(Delta.X, Delta.Y, 0.f).GetSafeNormal();

		// Line trace well above vehicle to detect building walls (not terrain slopes)
		const FVector TraceStart(Pos.X, Pos.Y, Pos.Z + 200.f);
		const FVector TraceEnd = TraceStart + MoveNorm * (MoveLen + 60.f);
		FCollisionQueryParams WallParams;
		WallParams.AddIgnoredActor(this);
		FHitResult WallHit;

		bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			WallHit, TraceStart, TraceEnd, ECC_WorldStatic, WallParams);

		if (bBlocked && !WallHit.bStartPenetrating && WallHit.Distance < MoveLen + 40.f)
		{
			const float SafeDist = FMath::Max(0.f, WallHit.Distance - 40.f);
			if (SafeDist > 1.f)
				AddActorWorldOffset(MoveNorm * FMath::Min(SafeDist, MoveLen), false);
			CurrentSpeed *= 0.1f;
		}
		else
		{
			AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
		}
	}

	// ---- Ground following (downward trace to ride slopes, ramps, ocean floor) ----
	{
		const FVector Pos = GetActorLocation();

		// --- Underground tunnel/airport zone override ---
		const bool bInTunnel = (Pos.X >= 102000.f && Pos.X <= 116000.f &&
			Pos.Y >= 28000.f && Pos.Y <= 32000.f);
		const bool bInAirport = (Pos.X >= 100000.f && Pos.X <= 200000.f &&
			Pos.Y >= -10000.f && Pos.Y <= 70000.f && Pos.Z < 60.f);

		const float FootOffset = 50.f; // vehicle pivot above wheels

		if (bInTunnel || bInAirport)
		{
			float TargetZ;
			if (bInTunnel && Pos.X >= 102000.f && Pos.X <= 116000.f)
			{
				const float Alpha = (Pos.X - 102000.f) / (116000.f - 102000.f);
				TargetZ = FMath::Lerp(-8000.f + FootOffset, 60.f, Alpha);
				if (Pos.Z < TargetZ - 200.f) TargetZ = -8000.f + FootOffset;
			}
			else { TargetZ = -8000.f + FootOffset; }
			bHasBeenGrounded = true;
			bWasGrounded = true;
			VerticalVelocity = 0.f;
			const float NewZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, 12.f);
			SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
		}
		else
		{
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

				if (TargetZ >= Pos.Z - 200.f)
				{
					bFoundSurface = true;
					bHasBeenGrounded = true;
					VerticalVelocity = 0.f;
					const float NewZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, 12.f);
					SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));

					const FVector Forward2D = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();
					const float ForwardSlope = -(Hit.ImpactNormal.X * Forward2D.X + Hit.ImpactNormal.Y * Forward2D.Y) / FMath::Max(Hit.ImpactNormal.Z, 0.01f);
					const float TargetPitch = FMath::RadiansToDegrees(FMath::Atan(ForwardSlope));
					FRotator Rot = GetActorRotation();
					Rot.Pitch = FMath::FInterpTo(Rot.Pitch, TargetPitch, DeltaTime, 5.f);
					SetActorRotation(Rot);
					break;
				}
			}
		}
		if (!bFoundSurface && bHasBeenGrounded)
		{
			if (bWasGrounded)
			{
				const float PitchRad = FMath::DegreesToRadians(GetActorRotation().Pitch);
				VerticalVelocity = FMath::Abs(CurrentSpeed) * FMath::Sin(PitchRad);
			}
			VerticalVelocity -= 980.f * DeltaTime;
			float NewZ = Pos.Z + VerticalVelocity * DeltaTime;
			if (NewZ <= 50.f) { NewZ = 50.f; VerticalVelocity = 0.f; }
			SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));

			FRotator Rot = GetActorRotation();
			Rot.Pitch = FMath::FInterpTo(Rot.Pitch, 0.f, DeltaTime, 3.f);
			SetActorRotation(Rot);
		}
		bWasGrounded = bFoundSurface;
		// Terrain height supplement — ensure vehicle rides terrain hills
		if (UPHCCitySubsystem* CitySub = GetWorld()->GetSubsystem<UPHCCitySubsystem>())
		{
			const FVector CurPos = GetActorLocation();
			const float TerrainZ = CitySub->GetTerrainHeight(CurPos.X, CurPos.Y) + FootOffset;
			if (CurPos.Z < TerrainZ - 2.f)
			{
				const float NewZ = FMath::FInterpTo(CurPos.Z, TerrainZ, DeltaTime, 12.f);
				SetActorLocation(FVector(CurPos.X, CurPos.Y, NewZ));
				VerticalVelocity = 0.f;
				bHasBeenGrounded = true;
			}
		}
		} // end else (not in tunnel/airport)
	}

}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void APHCDuneBuggyPawn::HandleThrottle(const FInputActionValue&)
{
	ThrottleInput = 1.f;
	if (!HasAuthority()) ServerSetThrottle(1.f);
}
void APHCDuneBuggyPawn::HandleThrottleRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
	if (!HasAuthority()) ServerSetThrottle(0.f);
}
void APHCDuneBuggyPawn::HandleBrake(const FInputActionValue&)
{
	ThrottleInput = -1.f;
	if (!HasAuthority()) ServerSetThrottle(-1.f);
}
void APHCDuneBuggyPawn::HandleBrakeRelease(const FInputActionValue&)
{
	if (ThrottleInput < 0.f)
	{
		ThrottleInput = 0.f;
		if (!HasAuthority()) ServerSetThrottle(0.f);
	}
}
void APHCDuneBuggyPawn::HandleSteer(const FInputActionValue& Value)
{
	SteeringInput = Value.Get<float>();
	if (!HasAuthority()) ServerSetSteering(SteeringInput);
}
void APHCDuneBuggyPawn::HandleSteerRelease(const FInputActionValue&)
{
	SteeringInput = 0.f;
	if (!HasAuthority()) ServerSetSteering(0.f);
}

void APHCDuneBuggyPawn::HandleExitVehicle(const FInputActionValue&)
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (VehicleMappingContext)
			{
				Sub->RemoveMappingContext(VehicleMappingContext);
			}
		}

		static const FName ExitFuncName(TEXT("OnVehicleExitRequested"));
		UFunction* Func = PC->FindFunction(ExitFuncName);
		if (Func)
		{
			PC->ProcessEvent(Func, nullptr);
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void APHCDuneBuggyPawn::SetBodyColor(const FLinearColor& Color)
{
	auto SetColor = [](UStaticMeshComponent* Comp, const FLinearColor& C,
		float Metallic = 0.f, float Roughness = 0.5f)
	{
		if (!Comp) return;
		UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
		if (!Mat)
		{
			Mat = UMaterialInstanceDynamic::Create(Comp->GetMaterial(0), Comp);
			if (Mat) Comp->SetMaterial(0, Mat);
		}
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), C);
			if (Metallic != 0.f)
				Mat->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			if (Roughness != 0.5f)
				Mat->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		}
	};

	// Main chassis (ClearCoat automotive paint).
	if (!BodyMaterial && ChassisMesh)
	{
		UMaterialInterface* BaseMat = ClearCoatMaterial ? (UMaterialInterface*)ClearCoatMaterial : ChassisMesh->GetMaterial(0);
		BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, ChassisMesh);
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
		BodyMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.9f);
		BodyMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.35f);
		ChassisMesh->SetMaterial(0, BodyMaterial);
	}

	// All body-colored parts (chassis is in BodyColoredMeshes too).
	for (UStaticMeshComponent* C : BodyColoredMeshes)
	{
		SetColor(C, Color, 0.9f, 0.35f);
	}
}

void APHCDuneBuggyPawn::SetHeadlightsEnabled(bool bEnabled)
{
	if (HeadlightSpotL) HeadlightSpotL->SetVisibility(bEnabled);
	if (HeadlightSpotR) HeadlightSpotR->SetVisibility(bEnabled);
}

FVector APHCDuneBuggyPawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -300.f + FVector(0.f, 0.f, 100.f);
}

void APHCDuneBuggyPawn::FireMissile()
{
	if (FireCooldownRemaining > 0.f)
	{
		return;
	}

	FireCooldownRemaining = FireCooldown;

	// Spawn locally — missiles are non-replicated visual tracers, no RPC needed.
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector MuzzleLoc = GetActorLocation()
		+ GetActorForwardVector() * 250.f
		+ FVector(0.f, 0.f, 50.f);
	const FRotator MuzzleRot = GetActorForwardVector().Rotation();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APHCMissileProjectile* Proj = World->SpawnActor<APHCMissileProjectile>(
		APHCMissileProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
	if (Proj)
	{
		Proj->OwnerVehicle = this;
	}
}

void APHCDuneBuggyPawn::ServerFireMissile_Implementation()
{
	// Kept for RPC signature — firing now handled locally in FireMissile().
}

void APHCDuneBuggyPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCDuneBuggyPawn, bIsOccupied);
}

void APHCDuneBuggyPawn::ServerSetThrottle_Implementation(float Value)
{
	ThrottleInput = Value;
}

void APHCDuneBuggyPawn::ServerSetSteering_Implementation(float Value)
{
	SteeringInput = Value;
}
