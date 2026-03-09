// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCDirtBikePawn.h"
#include "Vehicle/PHCNukeExplosion.h"
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

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCDirtBikePawn::APHCDirtBikePawn()
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));

	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cyl  = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	UStaticMesh* Sphere = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
	UStaticMesh* Cone = ConeFinder.Succeeded() ? ConeFinder.Object : nullptr;

	// ========================================================================
	// Dirt Bike — motocross-style off-road motorcycle with visible rider
	//
	// ~280 UU long × ~60 UU wide × ~140 UU tall (to handlebar top)
	// Cube mesh = 100×100×100 at scale 1.0 → scale = DesiredSize / 100.
	// ========================================================================

	// ---- Collision box (root) ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(140.f, 40.f, 30.f));
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

	auto MakeSphere = [&](const TCHAR* Name, const FVector& Loc,
		const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* S = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		S->SetupAttachment(VehicleCollision);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Sphere)
		{
			S->SetStaticMesh(Sphere);
			S->SetRelativeScale3D(Scale);
			S->SetRelativeLocation(Loc);
		}
		return S;
	};

	// ====================================================================
	//  FRAME — main backbone tube (top tube + down tube)
	// ====================================================================

	// Top tube (main frame beam, angled slightly downward to rear)
	FrameMesh = MakeCube(TEXT("FrameMain"),
		FVector(10.f, 0.f, 40.f), FVector(180.f, 8.f, 10.f));
	BodyColoredMeshes.Add(FrameMesh);

	// Down tube (from headstock down to engine area)
	FrameDownTube = MakeCube(TEXT("FrameDownTube"),
		FVector(65.f, 0.f, 15.f), FVector(10.f, 8.f, 50.f));
	if (FrameDownTube) FrameDownTube->SetRelativeRotation(FRotator(0.f, 0.f, 15.f));
	BodyColoredMeshes.Add(FrameDownTube);

	// Swing arm (rear suspension, from pivot to rear axle)
	SwingArm = MakeCube(TEXT("SwingArm"),
		FVector(-70.f, 0.f, 10.f), FVector(120.f, 6.f, 8.f));

	// ====================================================================
	//  ENGINE BLOCK (compact single-cylinder)
	// ====================================================================

	EngineBlock = MakeCube(TEXT("EngineBlock"),
		FVector(15.f, 0.f, 5.f), FVector(50.f, 30.f, 35.f));

	// Exhaust pipe (runs along right side, curves down)
	ExhaustPipe = MakeCyl(TEXT("ExhaustPipe"),
		FVector(-30.f, 18.f, 0.f), FVector(0.04f, 0.04f, 0.70f),
		FRotator(0.f, 0.f, 0.f));

	// Exhaust tip (slightly wider)
	ExhaustTip = MakeCyl(TEXT("ExhaustTip"),
		FVector(-90.f, 18.f, -3.f), FVector(0.06f, 0.06f, 0.12f),
		FRotator(0.f, 0.f, 0.f));

	// ====================================================================
	//  FRONT FORK (telescoping suspension)
	// ====================================================================

	ForkLegL = MakeCyl(TEXT("ForkLegL"),
		FVector(90.f, -10.f, 10.f), FVector(0.03f, 0.03f, 0.70f),
		FRotator(15.f, 0.f, 0.f));

	ForkLegR = MakeCyl(TEXT("ForkLegR"),
		FVector(90.f, 10.f, 10.f), FVector(0.03f, 0.03f, 0.70f),
		FRotator(15.f, 0.f, 0.f));

	// Fork bridge (triple clamp connecting the two fork legs)
	ForkBridge = MakeCube(TEXT("ForkBridge"),
		FVector(100.f, 0.f, 52.f), FVector(12.f, 28.f, 5.f));

	// ====================================================================
	//  HANDLEBARS
	// ====================================================================

	HandlebarMesh = MakeCyl(TEXT("HandlebarMesh"),
		FVector(100.f, 0.f, 58.f), FVector(0.025f, 0.025f, 0.40f),
		FRotator(0.f, 0.f, 90.f));

	// Rubber grips
	GripL = MakeCyl(TEXT("GripL"),
		FVector(100.f, -22.f, 58.f), FVector(0.035f, 0.035f, 0.08f),
		FRotator(0.f, 0.f, 90.f));

	GripR = MakeCyl(TEXT("GripR"),
		FVector(100.f, 22.f, 58.f), FVector(0.035f, 0.035f, 0.08f),
		FRotator(0.f, 0.f, 90.f));

	// ====================================================================
	//  WHEELS (large knobby tires)
	// ====================================================================

	// Front wheel
	WheelFront = MakeCyl(TEXT("WheelFront"),
		FVector(110.f, 0.f, -22.f), FVector(0.42f, 0.42f, 0.12f),
		FRotator(0.f, 0.f, 90.f));

	// Rear wheel
	WheelRear = MakeCyl(TEXT("WheelRear"),
		FVector(-90.f, 0.f, -22.f), FVector(0.42f, 0.42f, 0.14f),
		FRotator(0.f, 0.f, 90.f));

	// ====================================================================
	//  FENDERS
	// ====================================================================

	FrontFender = MakeCube(TEXT("FrontFender"),
		FVector(110.f, 0.f, 18.f), FVector(60.f, 22.f, 3.f));
	BodyColoredMeshes.Add(FrontFender);

	RearFender = MakeCube(TEXT("RearFender"),
		FVector(-80.f, 0.f, 18.f), FVector(55.f, 22.f, 3.f));
	BodyColoredMeshes.Add(RearFender);

	// ====================================================================
	//  SEAT (elongated, slightly raised)
	// ====================================================================

	SeatMesh = MakeCube(TEXT("SeatMesh"),
		FVector(-20.f, 0.f, 48.f), FVector(80.f, 20.f, 8.f));

	// ====================================================================
	//  FUEL TANK (between seat and handlebars)
	// ====================================================================

	FuelTankMesh = MakeCube(TEXT("FuelTankMesh"),
		FVector(40.f, 0.f, 50.f), FVector(50.f, 24.f, 16.f));
	BodyColoredMeshes.Add(FuelTankMesh);

	// ====================================================================
	//  HEADLIGHT & TAILLIGHT
	// ====================================================================

	HeadlightMesh = MakeCube(TEXT("HeadlightMesh"),
		FVector(125.f, 0.f, 38.f), FVector(6.f, 12.f, 10.f));

	TaillightMesh = MakeCube(TEXT("TaillightMesh"),
		FVector(-105.f, 0.f, 28.f), FVector(4.f, 10.f, 6.f));

	// Headlight spot
	HeadlightSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpot"));
	HeadlightSpot->SetupAttachment(VehicleCollision);
	HeadlightSpot->SetRelativeLocation(FVector(130.f, 0.f, 38.f));
	HeadlightSpot->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
	HeadlightSpot->SetIntensity(8000.f);
	HeadlightSpot->SetOuterConeAngle(45.f);
	HeadlightSpot->SetInnerConeAngle(25.f);
	HeadlightSpot->SetAttenuationRadius(3000.f);
	HeadlightSpot->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
	HeadlightSpot->SetCastShadows(false);
	HeadlightSpot->SetVisibility(false);

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

void APHCDirtBikePawn::BeginPlay()
{
	Super::BeginPlay();
	CreateInputAssets();

	const FLinearColor BikeRed(0.75f, 0.12f, 0.05f);
	const FLinearColor DarkMetal(0.08f, 0.08f, 0.08f);
	const FLinearColor Chrome(0.7f, 0.7f, 0.72f);
	const FLinearColor RubberBlack(0.03f, 0.03f, 0.03f);
	const FLinearColor SeatBrown(0.15f, 0.10f, 0.05f);
	const FLinearColor TailRed(2.0f, 0.05f, 0.02f);
	const FLinearColor HeadlightWhite(3.0f, 3.0f, 2.8f);

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

	// Load ClearCoat material
	ClearCoatMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCClearCoat.M_PHCClearCoat"));

	auto ApplyClearCoat = [&](UStaticMeshComponent* Comp, const FLinearColor& Color)
	{
		if (!Comp || !Comp->GetStaticMesh()) return;
		UMaterialInterface* BaseMat = ClearCoatMaterial
			? (UMaterialInterface*)ClearCoatMaterial
			: Comp->GetMaterial(0);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BaseMat, Comp);
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), Color);
			M->SetScalarParameterValue(TEXT("Metallic"), 0.7f);
			M->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
			Comp->SetMaterial(0, M);
		}
	};

	// Body panels — ClearCoat red
	if (FrameMesh && FrameMesh->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = ClearCoatMaterial
			? (UMaterialInterface*)ClearCoatMaterial
			: FrameMesh->GetMaterial(0);
		BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, FrameMesh);
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), BikeRed);
			BodyMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.7f);
			BodyMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
			FrameMesh->SetMaterial(0, BodyMaterial);
		}
	}
	for (UStaticMeshComponent* C : BodyColoredMeshes)
	{
		if (C != FrameMesh) ApplyClearCoat(C, BikeRed);
	}

	// Frame parts — dark metal
	ApplyMat(FrameDownTube, DarkMetal, 0.6f, 0.35f);
	ApplyMat(SwingArm, DarkMetal, 0.5f, 0.4f);
	ApplyMat(ForkBridge, DarkMetal, 0.6f, 0.3f);

	// Fork legs — chrome
	ApplyMat(ForkLegL, Chrome, 0.8f, 0.15f);
	ApplyMat(ForkLegR, Chrome, 0.8f, 0.15f);

	// Handlebars — chrome
	ApplyMat(HandlebarMesh, Chrome, 0.8f, 0.15f);
	ApplyMat(GripL, RubberBlack, 0.0f, 0.9f);
	ApplyMat(GripR, RubberBlack, 0.0f, 0.9f);

	// Engine — dark metallic
	ApplyMat(EngineBlock, DarkMetal, 0.7f, 0.35f);
	ApplyMat(ExhaustPipe, Chrome, 0.8f, 0.2f);
	ApplyMat(ExhaustTip, DarkMetal, 0.6f, 0.4f);

	// Wheels — black rubber
	ApplyMat(WheelFront, RubberBlack, 0.0f, 0.95f);
	ApplyMat(WheelRear, RubberBlack, 0.0f, 0.95f);

	// Seat — brown leather
	ApplyMat(SeatMesh, SeatBrown, 0.0f, 0.8f);

	// Headlight — bright emissive
	ApplyMat(HeadlightMesh, FLinearColor(0.9f, 0.9f, 0.85f), 0.0f, 0.1f, HeadlightWhite);

	// Taillight — red emissive
	ApplyMat(TaillightMesh, FLinearColor(0.6f, 0.02f, 0.02f), 0.0f, 0.1f, TailRed);

}

// ---------------------------------------------------------------------------
// CreateInputAssets (identical to car controls)
// ---------------------------------------------------------------------------

void APHCDirtBikePawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_BikeThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_BikeBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_BikeSteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_BikeExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_DirtBike"));

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

void APHCDirtBikePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCDirtBikePawn::HandleThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCDirtBikePawn::HandleThrottleRelease);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCDirtBikePawn::HandleBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCDirtBikePawn::HandleBrakeRelease);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCDirtBikePawn::HandleSteer);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCDirtBikePawn::HandleSteerRelease);
		}
		if (ExitVehicleAction)
		{
			EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCDirtBikePawn::HandleExitVehicle);
		}
	}
}

// ---------------------------------------------------------------------------
// Tick (same arcade physics as car vehicles)
// ---------------------------------------------------------------------------

void APHCDirtBikePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

	if (!bIsOccupied) return;

	// ---- Speed update (identical quadratic curve to cars) ----
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

	// ---- Steering / Yaw ----
	if (FMath::Abs(CurrentSpeed) > 10.f)
	{
		const float NormSpeed = FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxSpeed, 0.f, 1.f);
		const float SpeedFactor = FMath::Lerp(1.0f, 0.3f, NormSpeed);
		const float YawDelta = SteeringInput * SteeringSpeed * SpeedFactor * DeltaTime;
		FRotator Rot = GetActorRotation();
		Rot.Yaw += YawDelta;

		// Cosmetic lean: tilt the bike into turns
		const float TargetRoll = -SteeringInput * 25.f * FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxSpeed, 0.f, 1.f);
		Rot.Roll = FMath::FInterpTo(Rot.Roll, TargetRoll, DeltaTime, 6.f);

		SetActorRotation(Rot);
	}
	else
	{
		// Auto-level roll when slow
		FRotator Rot = GetActorRotation();
		if (FMath::Abs(Rot.Roll) > 0.5f)
		{
			Rot.Roll = FMath::FInterpTo(Rot.Roll, 0.f, DeltaTime, 8.f);
			SetActorRotation(Rot);
		}
	}

	// ---- Movement ----
	if (FMath::Abs(CurrentSpeed) > 1.f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Delta = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal() * CurrentSpeed * DeltaTime;
		AddActorWorldOffset(Delta, false);
	}

	// ---- Wall detection (line trace at chest height) ----
	// PERF-OPT: Only run wall trace every other frame to halve line trace cost.
	// To rollback: remove the frame counter check (WallTraceFrameCounter).
	// Skipped inside the motocross park where the bike needs to freely
	// approach hills at full speed for satisfying jumps.
	WallTraceFrameCounter++;
	if ((WallTraceFrameCounter & 1) == 0) // every other frame
	{
		const FVector Pos = GetActorLocation();
		const bool bInMotocrossPark = (Pos.X > -175000.f && Pos.X < -65000.f &&
			Pos.Y > -35000.f && Pos.Y < 65000.f);
		if (!bInMotocrossPark)
		{
			const FVector MoveDir = GetActorForwardVector();
			const float MoveLen = FMath::Abs(CurrentSpeed) * DeltaTime;
			// Trace at 400 UU above bike to avoid hitting terrain slopes
			const FVector TraceStart = Pos + FVector(0.f, 0.f, 400.f);
			const FVector TraceEnd = TraceStart + FVector(MoveDir.X, MoveDir.Y, 0.f).GetSafeNormal() * (MoveLen + 120.f);

			FCollisionQueryParams WallParams;
			WallParams.AddIgnoredActor(this);
			FHitResult WallHit;
			if (GetWorld()->LineTraceSingleByChannel(WallHit, TraceStart, TraceEnd, ECC_WorldStatic, WallParams))
			{
				// Only count as wall if the hit surface is mostly vertical (not terrain slope)
				if (!WallHit.bStartPenetrating && WallHit.Distance < MoveLen + 60.f
					&& WallHit.ImpactNormal.Z < 0.5f)
				{
					CurrentSpeed *= 0.1f;
				}
			}
		}
	}

	// ---- Ground following (line trace + terrain height) ----
	// PERF-OPT: Cache GetTerrainHeight — only recalculate every 3 frames
	// or when the bike has moved >200 UU from the cached sample position.
	// This reduces worst-case 15 Perlin noise calls from every frame to every 3rd frame.
	// To rollback: replace this block with the original direct GetTerrainHeight call.
	{
		const FVector Pos = GetActorLocation();
		const float FootOffset = 22.f;
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

		// PERF-OPT: Only call GetTerrainHeight every 3 frames or when position changed significantly
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
				// On or below ground — smoothly interpolate up to avoid jerky snapping
				const float SmoothZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, 20.f);
				SetActorLocation(FVector(Pos.X, Pos.Y, SmoothZ));
				VerticalVelocity = 0.f;
			}
		}
	}

	// ---- Wheel spin animation ----
	{
		const float WheelRadius = 42.f;
		const float AngularSpeed = (WheelRadius > 0.f)
			? FMath::RadiansToDegrees(CurrentSpeed / WheelRadius)
			: 0.f;
		WheelSpinAngle += AngularSpeed * DeltaTime;

		const FRotator SpinRot(WheelSpinAngle, 0.f, 90.f);
		if (WheelFront) WheelFront->SetRelativeRotation(SpinRot);
		if (WheelRear)  WheelRear->SetRelativeRotation(SpinRot);
	}
}

// ---------------------------------------------------------------------------
// Input handlers (identical pattern to car vehicles)
// ---------------------------------------------------------------------------

void APHCDirtBikePawn::HandleThrottle(const FInputActionValue&)
{
	ThrottleInput = 1.f;
	if (!HasAuthority()) ServerSetThrottle(1.f);
}
void APHCDirtBikePawn::HandleThrottleRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
	if (!HasAuthority()) ServerSetThrottle(0.f);
}
void APHCDirtBikePawn::HandleBrake(const FInputActionValue&)
{
	ThrottleInput = -1.f;
	if (!HasAuthority()) ServerSetThrottle(-1.f);
}
void APHCDirtBikePawn::HandleBrakeRelease(const FInputActionValue&)
{
	if (ThrottleInput < 0.f)
	{
		ThrottleInput = 0.f;
		if (!HasAuthority()) ServerSetThrottle(0.f);
	}
}
void APHCDirtBikePawn::HandleSteer(const FInputActionValue& Value)
{
	SteeringInput = Value.Get<float>();
	if (!HasAuthority()) ServerSetSteering(SteeringInput);
}
void APHCDirtBikePawn::HandleSteerRelease(const FInputActionValue&)
{
	SteeringInput = 0.f;
	if (!HasAuthority()) ServerSetSteering(0.f);
}
void APHCDirtBikePawn::HandleExitVehicle(const FInputActionValue&)
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

void APHCDirtBikePawn::SetBodyColor(const FLinearColor& Color)
{
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
	}
	for (UStaticMeshComponent* Comp : BodyColoredMeshes)
	{
		if (!Comp) continue;
		UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}
}

void APHCDirtBikePawn::SetHeadlightsEnabled(bool bEnabled)
{
	if (HeadlightSpot) HeadlightSpot->SetVisibility(bEnabled);
}

FVector APHCDirtBikePawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -200.f + FVector(0.f, 0.f, 50.f);
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCDirtBikePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCDirtBikePawn, bIsOccupied);
}

void APHCDirtBikePawn::ServerSetThrottle_Implementation(float Value) { ThrottleInput = Value; }
void APHCDirtBikePawn::ServerSetSteering_Implementation(float Value) { SteeringInput = Value; }
