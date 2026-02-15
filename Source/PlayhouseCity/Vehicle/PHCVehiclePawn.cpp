// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCVehiclePawn.h"
#include "PlayhouseCity.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCVehiclePawn::APHCVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// ---- Multiplayer replication (disabled for now) ----
	// bReplicates = true;
	// SetReplicateMovement(true);
	// NetUpdateFrequency = 30.f;
	// MinNetUpdateFrequency = 15.f;
	// bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cyl  = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	// ========================================================================
	// Lamborghini Aventador S Roadster – clean block model
	//
	// Real: 4780 × 2030 × 1136 mm.  Game scale ≈ 1:1 (1 UU ≈ 1 cm).
	// ~480 long × 200 wide × ~55 tall.
	// Cube mesh = 100×100×100 at scale 1.0  →  scale = DesiredSize / 100.
	// ========================================================================

	// ---- Collision box (root) ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(240.f, 100.f, 20.f));
	VehicleCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleCollision->SetCollisionResponseToAllChannels(ECR_Block);
	VehicleCollision->SetSimulatePhysics(false);
	VehicleCollision->SetHiddenInGame(true);
	SetRootComponent(VehicleCollision);

	// Helpers (VehicleCollision is now valid for attachment) --------------------
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

	// Main body slab (the dominant shape – long, low, wide)
	BodyMesh = MakeCube(TEXT("BodyMesh"),
		FVector(-10.f, 0.f, 0.f), FVector(360.f, 190.f, 34.f));

	// Front nose wedge (lower + slightly narrower = wedge silhouette)
	FrontNoseMesh = MakeCube(TEXT("FrontNose"),
		FVector(220.f, 0.f, -4.f), FVector(100.f, 178.f, 26.f));

	// ====================================================================
	//  CABIN / ENGINE
	// ====================================================================

	// Windshield (dark glass, narrower, sits on top of body)
	CabinMesh = MakeCube(TEXT("CabinMesh"),
		FVector(35.f, 0.f, 25.f), FVector(85.f, 144.f, 18.f));

	// Engine cover hump (behind cockpit, raised)
	RearEngineMesh = MakeCube(TEXT("RearEngine"),
		FVector(-85.f, 0.f, 19.f), FVector(130.f, 168.f, 18.f));

	// Engine glass panel (tinted)
	{
		auto* G = MakeCube(TEXT("EngineGlass"),
			FVector(-85.f, 0.f, 30.f), FVector(80.f, 100.f, 3.f));
		DarkTrimMeshes.Add(G);
	}

	// ====================================================================
	//  AERO / TRIM
	// ====================================================================

	// Front splitter (carbon fiber)
	FrontSplitterMesh = MakeCube(TEXT("FrontSplitter"),
		FVector(268.f, 0.f, -18.f), FVector(30.f, 198.f, 3.f));

	// Front grille (dark intake opening)
	{
		auto* Grille = MakeCube(TEXT("FrontGrille"),
			FVector(269.f, 0.f, -8.f), FVector(4.f, 130.f, 12.f));
		DarkTrimMeshes.Add(Grille);
	}

	// Side air intakes
	SideIntakeL = MakeCube(TEXT("SideIntakeL"),
		FVector(-10.f, 96.f, 2.f), FVector(55.f, 6.f, 20.f));
	SideIntakeR = MakeCube(TEXT("SideIntakeR"),
		FVector(-10.f, -96.f, 2.f), FVector(55.f, 6.f, 20.f));

	// Side skirts (thin dark strips along bottom)
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Y = (Side == 0) ? 96.f : -96.f;
		FString N = FString::Printf(TEXT("SideSkirt_%d"), Side);
		auto* S = MakeCube(*N,
			FVector(10.f, Y, -15.f), FVector(300.f, 3.f, 5.f));
		DarkTrimMeshes.Add(S);
	}

	// Rear diffuser
	RearDiffuserMesh = MakeCube(TEXT("RearDiffuser"),
		FVector(-210.f, 0.f, -18.f), FVector(24.f, 190.f, 3.f));

	// Rear spoiler lip
	RearSpoilerMesh = MakeCube(TEXT("RearSpoiler"),
		FVector(-200.f, 0.f, 26.f), FVector(6.f, 178.f, 3.f));

	// Rear bumper (dark vertical face)
	{
		auto* RB = MakeCube(TEXT("RearBumper"),
			FVector(-205.f, 0.f, -4.f), FVector(6.f, 186.f, 20.f));
		DarkTrimMeshes.Add(RB);
	}

	// ====================================================================
	//  WHEELS
	// ====================================================================

	// Front: diameter 33, width 11
	WheelFL = MakeCyl(TEXT("WheelFL"), FVector(140.f, 92.f, -24.f), FVector(0.33f, 0.33f, 0.11f));
	WheelFR = MakeCyl(TEXT("WheelFR"), FVector(140.f, -92.f, -24.f), FVector(0.33f, 0.33f, 0.11f));

	// Rear: diameter 35, width 14 (wider stance + bigger tires)
	WheelRL = MakeCyl(TEXT("WheelRL"), FVector(-140.f, 96.f, -22.f), FVector(0.35f, 0.35f, 0.14f));
	WheelRR = MakeCyl(TEXT("WheelRR"), FVector(-140.f, -96.f, -22.f), FVector(0.35f, 0.35f, 0.14f));

	// ====================================================================
	//  LIGHTS
	// ====================================================================

	HeadlightMeshL = MakeCube(TEXT("HeadlightL"),
		FVector(268.f, 60.f, -4.f), FVector(8.f, 20.f, 4.f));
	HeadlightMeshR = MakeCube(TEXT("HeadlightR"),
		FVector(268.f, -60.f, -4.f), FVector(8.f, 20.f, 4.f));

	TaillightMeshL = MakeCube(TEXT("TaillightL"),
		FVector(-204.f, 52.f, 6.f), FVector(4.f, 28.f, 6.f));
	TaillightMeshR = MakeCube(TEXT("TaillightR"),
		FVector(-204.f, -52.f, 6.f), FVector(4.f, 28.f, 6.f));

	// Tail light connecting bar
	{
		MakeCube(TEXT("TailLightBar"),
			FVector(-206.f, 0.f, 6.f), FVector(3.f, 90.f, 3.f));
	}

	// ---- Headlight spotlights ----
	HeadlightSpotL = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotL"));
	HeadlightSpotL->SetupAttachment(VehicleCollision);
	HeadlightSpotL->SetRelativeLocation(FVector(270.f, 60.f, -4.f));
	HeadlightSpotL->SetRelativeRotation(FRotator(-3.f, 0.f, 0.f));
	HeadlightSpotL->SetIntensity(80000.f);
	HeadlightSpotL->SetAttenuationRadius(6000.f);
	HeadlightSpotL->SetOuterConeAngle(28.f);
	HeadlightSpotL->SetInnerConeAngle(16.f);
	HeadlightSpotL->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotL->SetVisibility(false);

	HeadlightSpotR = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotR"));
	HeadlightSpotR->SetupAttachment(VehicleCollision);
	HeadlightSpotR->SetRelativeLocation(FVector(270.f, -60.f, -4.f));
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
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 180.f);
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
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void APHCVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	CreateInputAssets();

	// ---- Apply materials ----

	const FLinearColor DefaultBody(0.25f, 0.25f, 0.27f);
	const FLinearColor DarkCarbon(0.025f, 0.025f, 0.03f);
	const FLinearColor DarkGlass(0.03f, 0.03f, 0.07f);
	const FLinearColor WheelBlack(0.04f, 0.04f, 0.04f);
	const FLinearColor LEDWhite(3.0f, 2.8f, 2.2f);
	const FLinearColor TailRed(2.5f, 0.02f, 0.02f);

	auto ApplyMat = [](UStaticMeshComponent* Comp, const FLinearColor& Color)
	{
		if (!Comp || !Comp->GetStaticMesh()) return;
		UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
		if (!M)
		{
			M = UMaterialInstanceDynamic::Create(Comp->GetMaterial(0), Comp);
			if (M) Comp->SetMaterial(0, M);
		}
		if (M) M->SetVectorParameterValue(TEXT("BaseColor"), Color);
	};

	// Body panels (main + nose + engine cover).
	if (BodyMesh && BodyMesh->GetStaticMesh())
	{
		BodyMaterial = UMaterialInstanceDynamic::Create(BodyMesh->GetMaterial(0), BodyMesh);
		if (BodyMaterial) { BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), DefaultBody); BodyMesh->SetMaterial(0, BodyMaterial); }
	}
	ApplyMat(FrontNoseMesh, DefaultBody);
	ApplyMat(RearEngineMesh, DefaultBody * 0.9f); // slightly darker
	ApplyMat(RearSpoilerMesh, DefaultBody);

	// All body-colored detail pieces.
	for (UStaticMeshComponent* C : BodyColoredMeshes) { ApplyMat(C, DefaultBody); }

	// Windshield.
	if (CabinMesh && CabinMesh->GetStaticMesh())
	{
		CabinMaterial = UMaterialInstanceDynamic::Create(CabinMesh->GetMaterial(0), CabinMesh);
		if (CabinMaterial) { CabinMaterial->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass); CabinMesh->SetMaterial(0, CabinMaterial); }
	}

	// Dark carbon / trim pieces.
	ApplyMat(SideIntakeL, DarkCarbon);
	ApplyMat(SideIntakeR, DarkCarbon);
	ApplyMat(FrontSplitterMesh, DarkCarbon);
	ApplyMat(RearDiffuserMesh, DarkCarbon);
	for (UStaticMeshComponent* C : DarkTrimMeshes) { ApplyMat(C, DarkCarbon); }

	// Wheels.
	ApplyMat(WheelFL, WheelBlack);
	ApplyMat(WheelFR, WheelBlack);
	ApplyMat(WheelRL, WheelBlack);
	ApplyMat(WheelRR, WheelBlack);

	// Headlights (bright white LED).
	ApplyMat(HeadlightMeshL, LEDWhite);
	ApplyMat(HeadlightMeshR, LEDWhite);

	// Tail lights (Aventador red strip).
	ApplyMat(TaillightMeshL, TailRed);
	ApplyMat(TaillightMeshR, TailRed);

	// Tail light connecting bar.
	TArray<USceneComponent*> Children;
	VehicleCollision->GetChildrenComponents(false, Children);
	for (USceneComponent* Child : Children)
	{
		if (Child->GetName().Contains(TEXT("TailLightBar")))
			ApplyMat(Cast<UStaticMeshComponent>(Child), TailRed * 0.5f);
	}
}

// ---------------------------------------------------------------------------
// Input asset creation
// ---------------------------------------------------------------------------

void APHCVehiclePawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_VehicleThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_VehicleBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_VehicleSteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_VehicleExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle"));

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

void APHCVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCVehiclePawn::HandleThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCVehiclePawn::HandleThrottleRelease);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCVehiclePawn::HandleBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCVehiclePawn::HandleBrakeRelease);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCVehiclePawn::HandleSteer);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCVehiclePawn::HandleSteerRelease);
		}
		if (ExitVehicleAction)
		{
			EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCVehiclePawn::HandleExitVehicle);
		}
	}
}

// ---------------------------------------------------------------------------
// Tick -- arcade vehicle physics
// ---------------------------------------------------------------------------

void APHCVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsOccupied) return;

	// ---- Speed update (non-linear – harder to gain speed near top) ----
	if (ThrottleInput > 0.f)
	{
		// Acceleration tapers as you approach max speed (like real drag).
		const float SpeedRatio = FMath::Clamp(CurrentSpeed / MaxSpeed, 0.f, 1.f);
		const float AccelCurve = 1.f - SpeedRatio * SpeedRatio; // quadratic falloff
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
		// At low speed (< 1000): full steering.  At max speed: 30% steering.
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
		const FRotator BaseRot(0.f, 0.f, 90.f); // cylinder rolled onto side

		// Front wheels: spin + steering yaw.
		const float SteerYaw = SteeringInput * 25.f;
		const FRotator FrontRot(WheelSpinAngle, SteerYaw, 90.f);
		if (WheelFL) WheelFL->SetRelativeRotation(FrontRot);
		if (WheelFR) WheelFR->SetRelativeRotation(FrontRot);
		// Rear wheels: spin only.
		const FRotator RearRot(WheelSpinAngle, 0.f, 90.f);
		if (WheelRL) WheelRL->SetRelativeRotation(RearRot);
		if (WheelRR) WheelRR->SetRelativeRotation(RearRot);
	}

	// ---- Movement (flat ground -- no vertical trace) ----
	if (FMath::Abs(CurrentSpeed) > 1.f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Delta = Forward * CurrentSpeed * DeltaTime;

		// Move horizontally (no sweep -- avoids collision with ground/buildings).
		AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
	}
	else
	{
		// Flatten pitch when stationary.
		FRotator Rot = GetActorRotation();
		Rot.Pitch = FMath::FInterpTo(Rot.Pitch, 0.f, DeltaTime, 5.f);
		SetActorRotation(Rot);
	}
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void APHCVehiclePawn::HandleThrottle(const FInputActionValue&)  { ThrottleInput = 1.f; }
void APHCVehiclePawn::HandleThrottleRelease(const FInputActionValue&) { ThrottleInput = 0.f; }
void APHCVehiclePawn::HandleBrake(const FInputActionValue&) { ThrottleInput = -1.f; }
void APHCVehiclePawn::HandleBrakeRelease(const FInputActionValue&)
{
	if (ThrottleInput < 0.f) ThrottleInput = 0.f;
}
void APHCVehiclePawn::HandleSteer(const FInputActionValue& Value) { SteeringInput = Value.Get<float>(); }
void APHCVehiclePawn::HandleSteerRelease(const FInputActionValue&) { SteeringInput = 0.f; }

void APHCVehiclePawn::HandleExitVehicle(const FInputActionValue&)
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

void APHCVehiclePawn::SetBodyColor(const FLinearColor& Color)
{
	auto SetColor = [](UStaticMeshComponent* Comp, const FLinearColor& C)
	{
		if (!Comp) return;
		UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
		if (!Mat)
		{
			Mat = UMaterialInstanceDynamic::Create(Comp->GetMaterial(0), Comp);
			if (Mat) Comp->SetMaterial(0, Mat);
		}
		if (Mat) Mat->SetVectorParameterValue(TEXT("BaseColor"), C);
	};

	// Main body.
	if (!BodyMaterial && BodyMesh)
		BodyMaterial = UMaterialInstanceDynamic::Create(BodyMesh->GetMaterial(0), BodyMesh);
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
		BodyMesh->SetMaterial(0, BodyMaterial);
	}

	// Front nose, spoiler: match body.
	SetColor(FrontNoseMesh, Color);
	SetColor(RearSpoilerMesh, Color);

	// Engine cover: slightly darker.
	FLinearColor Darker = Color * 0.85f; Darker.A = 1.f;
	SetColor(RearEngineMesh, Darker);

	// All body-colored detail meshes (fenders, A-pillars, cockpit walls, etc.).
	for (UStaticMeshComponent* C : BodyColoredMeshes)
	{
		SetColor(C, Color);
	}

	// Windshield: stays dark tinted glass.
	if (!CabinMaterial && CabinMesh)
		CabinMaterial = UMaterialInstanceDynamic::Create(CabinMesh->GetMaterial(0), CabinMesh);
	if (CabinMaterial)
	{
		CabinMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.03f, 0.03f, 0.07f));
		CabinMesh->SetMaterial(0, CabinMaterial);
	}
}

void APHCVehiclePawn::SetHeadlightsEnabled(bool bEnabled)
{
	if (HeadlightSpotL) HeadlightSpotL->SetVisibility(bEnabled);
	if (HeadlightSpotR) HeadlightSpotR->SetVisibility(bEnabled);
}

FVector APHCVehiclePawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -300.f + FVector(0.f, 0.f, 100.f);
}

void APHCVehiclePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCVehiclePawn, bIsOccupied);
}
