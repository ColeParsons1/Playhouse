// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCRaptorPawn.h"
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
#include "Vehicle/PHCMissileProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Vehicle/PHCNukeExplosion.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCRaptorPawn::APHCRaptorPawn()
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(
		TEXT("/Engine/BasicShapes/Cone.Cone"));

	UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* Cyl  = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	UStaticMesh* Cone = ConeFinder.Succeeded() ? ConeFinder.Object : nullptr;

	// ========================================================================
	// F-22 Raptor – stealth fighter jet from basic shapes
	//
	// ~900 UU long × ~670 UU wingspan × ~80 UU tall.
	// Cube mesh = 100×100×100 at scale 1.0  →  scale = DesiredSize / 100.
	// ========================================================================

	// ---- Collision box (root) ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(500.f, 400.f, 25.f));
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
		}
		return W;
	};

	auto MakeCone = [&](const TCHAR* Name, const FVector& Loc,
		const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Co = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Co->SetupAttachment(VehicleCollision);
		Co->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cone)
		{
			Co->SetStaticMesh(Cone);
			Co->SetRelativeScale3D(Scale);
			Co->SetRelativeLocation(Loc);
		}
		return Co;
	};

	// ====================================================================
	//  FUSELAGE — long, flat diamond cross-section for stealth shaping
	// ====================================================================

	// Main fuselage body (elongated, narrow, very flat)
	FuselageMain = MakeCube(TEXT("FuselageMain"),
		FVector(0.f, 0.f, 0.f), FVector(750.f, 180.f, 26.f));
	BodyColoredMeshes.Add(FuselageMain);

	// Upper fuselage spine (subtle raised ridge along centerline)
	UStaticMeshComponent* DorsalSpine = MakeCube(TEXT("DorsalSpine"),
		FVector(-20.f, 0.f, 14.f), FVector(500.f, 40.f, 6.f));
	BodyColoredMeshes.Add(DorsalSpine);

	// Belly fairing (flat underbody panel, slightly wider)
	UStaticMeshComponent* BellyFairing = MakeCube(TEXT("BellyFairing"),
		FVector(-30.f, 0.f, -14.f), FVector(550.f, 160.f, 5.f));
	BodyColoredMeshes.Add(BellyFairing);

	// Nose section (long tapered wedge)
	FuselageNose = MakeCube(TEXT("FuselageNose"),
		FVector(380.f, 0.f, -1.f), FVector(340.f, 100.f, 16.f));
	BodyColoredMeshes.Add(FuselageNose);

	// Nose tip (sharp pointed cone radome)
	NoseTip = MakeCone(TEXT("NoseTip"),
		FVector(570.f, 0.f, -2.f), FVector(0.35f, 0.20f, 0.50f));
	if (NoseTip) NoseTip->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	BodyColoredMeshes.Add(NoseTip);

	// Rear fuselage (tapers between engines)
	FuselageRear = MakeCube(TEXT("FuselageRear"),
		FVector(-300.f, 0.f, 0.f), FVector(260.f, 155.f, 22.f));
	BodyColoredMeshes.Add(FuselageRear);

	// Engine nacelle fairings (elongated pods running aft from wing root)
	UStaticMeshComponent* NacelleL = MakeCube(TEXT("NacelleL"),
		FVector(-200.f, 65.f, -3.f), FVector(350.f, 55.f, 24.f));
	BodyColoredMeshes.Add(NacelleL);

	UStaticMeshComponent* NacelleR = MakeCube(TEXT("NacelleR"),
		FVector(-200.f, -65.f, -3.f), FVector(350.f, 55.f, 24.f));
	BodyColoredMeshes.Add(NacelleR);

	// ====================================================================
	//  COCKPIT CANOPY (glass) — low-profile bubble
	// ====================================================================

	CockpitCanopy = MakeCube(TEXT("CockpitCanopy"),
		FVector(200.f, 0.f, 22.f), FVector(160.f, 50.f, 16.f));
	// Gets M_PHCGlass in BeginPlay – NOT in BodyColoredMeshes

	// Canopy frame (thin outline around glass)
	UStaticMeshComponent* CanopyFrame = MakeCube(TEXT("CanopyFrame"),
		FVector(200.f, 0.f, 20.f), FVector(170.f, 54.f, 3.f));
	BodyColoredMeshes.Add(CanopyFrame);

	// ====================================================================
	//  WINGS — swept trapezoidal with LERX blends
	// ====================================================================

	// Main wing panels (thin, wide, slightly swept back)
	WingL = MakeCube(TEXT("WingL"),
		FVector(-50.f, 280.f, -3.f), FVector(320.f, 230.f, 5.f));
	if (WingL) WingL->SetRelativeRotation(FRotator(0.f, -8.f, -1.5f));
	BodyColoredMeshes.Add(WingL);

	WingR = MakeCube(TEXT("WingR"),
		FVector(-50.f, -280.f, -3.f), FVector(320.f, 230.f, 5.f));
	if (WingR) WingR->SetRelativeRotation(FRotator(0.f, 8.f, 1.5f));
	BodyColoredMeshes.Add(WingR);

	// LERX — Leading Edge Root Extensions (swept strakes blending fuselage to wings)
	UStaticMeshComponent* LerxL = MakeCube(TEXT("LerxL"),
		FVector(100.f, 140.f, -2.f), FVector(250.f, 90.f, 6.f));
	if (LerxL) LerxL->SetRelativeRotation(FRotator(0.f, -18.f, 0.f));
	BodyColoredMeshes.Add(LerxL);

	UStaticMeshComponent* LerxR = MakeCube(TEXT("LerxR"),
		FVector(100.f, -140.f, -2.f), FVector(250.f, 90.f, 6.f));
	if (LerxR) LerxR->SetRelativeRotation(FRotator(0.f, 18.f, 0.f));
	BodyColoredMeshes.Add(LerxR);

	// Wing trailing edge flaps (thin strips at rear of each wing)
	UStaticMeshComponent* FlapL = MakeCube(TEXT("FlapL"),
		FVector(-180.f, 240.f, -4.f), FVector(80.f, 200.f, 3.f));
	if (FlapL) FlapL->SetRelativeRotation(FRotator(0.f, -5.f, 0.f));
	BodyColoredMeshes.Add(FlapL);

	UStaticMeshComponent* FlapR = MakeCube(TEXT("FlapR"),
		FVector(-180.f, -240.f, -4.f), FVector(80.f, 200.f, 3.f));
	if (FlapR) FlapR->SetRelativeRotation(FRotator(0.f, 5.f, 0.f));
	BodyColoredMeshes.Add(FlapR);

	// ====================================================================
	//  HORIZONTAL STABILIZERS (all-moving tailerons)
	// ====================================================================

	HorizStabL = MakeCube(TEXT("HorizStabL"),
		FVector(-400.f, 150.f, 0.f), FVector(160.f, 115.f, 4.f));
	if (HorizStabL) HorizStabL->SetRelativeRotation(FRotator(0.f, -6.f, 0.f));
	BodyColoredMeshes.Add(HorizStabL);

	HorizStabR = MakeCube(TEXT("HorizStabR"),
		FVector(-400.f, -150.f, 0.f), FVector(160.f, 115.f, 4.f));
	if (HorizStabR) HorizStabR->SetRelativeRotation(FRotator(0.f, 6.f, 0.f));
	BodyColoredMeshes.Add(HorizStabR);

	// ====================================================================
	//  VERTICAL STABILIZERS (canted outward ~28°, F-22 signature)
	// ====================================================================

	VertStabL = MakeCube(TEXT("VertStabL"),
		FVector(-340.f, 75.f, 45.f), FVector(140.f, 10.f, 100.f));
	if (VertStabL) VertStabL->SetRelativeRotation(FRotator(0.f, 0.f, -28.f));
	BodyColoredMeshes.Add(VertStabL);

	VertStabR = MakeCube(TEXT("VertStabR"),
		FVector(-340.f, -75.f, 45.f), FVector(140.f, 10.f, 100.f));
	if (VertStabR) VertStabR->SetRelativeRotation(FRotator(0.f, 0.f, 28.f));
	BodyColoredMeshes.Add(VertStabR);

	// ====================================================================
	//  ENGINE INTAKES (trapezoidal stealth inlets on fuselage sides)
	// ====================================================================

	IntakeL = MakeCube(TEXT("IntakeL"),
		FVector(-30.f, 110.f, -10.f), FVector(140.f, 45.f, 28.f));
	if (IntakeL) IntakeL->SetRelativeRotation(FRotator(3.f, -5.f, 0.f));

	IntakeR = MakeCube(TEXT("IntakeR"),
		FVector(-30.f, -110.f, -10.f), FVector(140.f, 45.f, 28.f));
	if (IntakeR) IntakeR->SetRelativeRotation(FRotator(3.f, 5.f, 0.f));

	// Intake lip/ramp (angular panel above each intake)
	UStaticMeshComponent* IntakeLipL = MakeCube(TEXT("IntakeLipL"),
		FVector(-10.f, 110.f, 2.f), FVector(100.f, 42.f, 4.f));
	if (IntakeLipL) IntakeLipL->SetRelativeRotation(FRotator(-6.f, -5.f, 0.f));
	BodyColoredMeshes.Add(IntakeLipL);

	UStaticMeshComponent* IntakeLipR = MakeCube(TEXT("IntakeLipR"),
		FVector(-10.f, -110.f, 2.f), FVector(100.f, 42.f, 4.f));
	if (IntakeLipR) IntakeLipR->SetRelativeRotation(FRotator(-6.f, 5.f, 0.f));
	BodyColoredMeshes.Add(IntakeLipR);

	// ====================================================================
	//  ENGINE NOZZLES (2D thrust-vectoring rectangular nozzles)
	// ====================================================================

	NozzleL = MakeCube(TEXT("NozzleL"),
		FVector(-440.f, 65.f, 0.f), FVector(30.f, 22.f, 18.f));

	NozzleR = MakeCube(TEXT("NozzleR"),
		FVector(-440.f, -65.f, 0.f), FVector(30.f, 22.f, 18.f));

	// ====================================================================
	//  AFTERBURNER CONES (glowing exhaust plumes)
	// ====================================================================

	AfterburnerL = MakeCone(TEXT("AfterburnerL"),
		FVector(-480.f, 65.f, 0.f), FVector(0.14f, 0.10f, 0.35f));
	if (AfterburnerL) AfterburnerL->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));

	AfterburnerR = MakeCone(TEXT("AfterburnerR"),
		FVector(-480.f, -65.f, 0.f), FVector(0.14f, 0.10f, 0.35f));
	if (AfterburnerR) AfterburnerR->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));

	// ====================================================================
	//  NAVIGATION LIGHTS
	// ====================================================================

	// Left wingtip: red
	NavLightL = MakeCube(TEXT("NavLightL"),
		FVector(-80.f, 392.f, -3.f), FVector(6.f, 4.f, 4.f));

	// Right wingtip: green
	NavLightR = MakeCube(TEXT("NavLightR"),
		FVector(-80.f, -392.f, -3.f), FVector(6.f, 4.f, 4.f));

	// Tail: white
	TailLight = MakeCube(TEXT("TailLight"),
		FVector(-450.f, 0.f, 12.f), FVector(6.f, 6.f, 4.f));

	// ====================================================================
	//  CAMERA
	// ====================================================================

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleCollision);
	CameraBoom->TargetArmLength = 1000.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 300.f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 6.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 5.f;

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

void APHCRaptorPawn::BeginPlay()
{
	Super::BeginPlay();
	CreateInputAssets();

	// -- Colors --
	const FLinearColor StealthGray(0.35f, 0.37f, 0.38f);
	const FLinearColor DarkIntake(0.04f, 0.04f, 0.05f);
	const FLinearColor DarkNozzle(0.06f, 0.06f, 0.06f);
	const FLinearColor CanopyTint(0.02f, 0.03f, 0.06f);
	const FLinearColor NavRed(2.5f, 0.02f, 0.02f);
	const FLinearColor NavGreen(0.02f, 2.5f, 0.02f);
	const FLinearColor LightWhite(3.0f, 3.0f, 3.0f);

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

	// Load materials
	ClearCoatMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCClearCoat.M_PHCClearCoat"));
	GlassMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCGlass.M_PHCGlass"));

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

	// Body panels – ClearCoat stealth gray
	if (FuselageMain && FuselageMain->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = ClearCoatMaterial
			? (UMaterialInterface*)ClearCoatMaterial
			: FuselageMain->GetMaterial(0);
		BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, FuselageMain);
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), StealthGray);
			BodyMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.7f);
			BodyMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
			FuselageMain->SetMaterial(0, BodyMaterial);
		}
	}
	for (UStaticMeshComponent* C : BodyColoredMeshes)
	{
		if (C != FuselageMain)
		{
			ApplyClearCoat(C, StealthGray);
		}
	}

	// Cockpit canopy – Glass
	if (CockpitCanopy && CockpitCanopy->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = GlassMaterial
			? (UMaterialInterface*)GlassMaterial
			: CockpitCanopy->GetMaterial(0);
		UMaterialInstanceDynamic* GlassMat = UMaterialInstanceDynamic::Create(BaseMat, CockpitCanopy);
		if (GlassMat)
		{
			GlassMat->SetVectorParameterValue(TEXT("BaseColor"), CanopyTint);
			GlassMat->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
			GlassMat->SetScalarParameterValue(TEXT("Metallic"), 0.3f);
			GlassMat->SetScalarParameterValue(TEXT("Roughness"), 0.02f);
			CockpitCanopy->SetMaterial(0, GlassMat);
		}
	}

	// Engine intakes – dark
	ApplyMat(IntakeL, DarkIntake, 0.3f, 0.4f);
	ApplyMat(IntakeR, DarkIntake, 0.3f, 0.4f);

	// Engine nozzles – dark metallic
	ApplyMat(NozzleL, DarkNozzle, 0.8f, 0.35f);
	ApplyMat(NozzleR, DarkNozzle, 0.8f, 0.35f);

	// Afterburners – orange emissive (off initially, toggled in Tick)
	auto SetupAfterburner = [&](UStaticMeshComponent* Comp) -> UMaterialInstanceDynamic*
	{
		if (!Comp || !Comp->GetStaticMesh()) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Comp->GetMaterial(0), Comp);
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.6f, 0.3f, 0.02f));
			M->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
			M->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
			M->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
			Comp->SetMaterial(0, M);
		}
		return M;
	};
	AfterburnerMatL = SetupAfterburner(AfterburnerL);
	AfterburnerMatR = SetupAfterburner(AfterburnerR);

	// Navigation lights – emissive
	ApplyMat(NavLightL, NavRed, 0.0f, 0.1f, NavRed);
	ApplyMat(NavLightR, NavGreen, 0.0f, 0.1f, NavGreen);
	ApplyMat(TailLight, LightWhite, 0.0f, 0.1f, LightWhite);
}

// ---------------------------------------------------------------------------
// CreateInputAssets
// ---------------------------------------------------------------------------

void APHCRaptorPawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_RaptorThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_RaptorBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_RaptorSteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	PitchUpAction = NewObject<UInputAction>(this, TEXT("IA_RaptorPitchUp"));
	PitchUpAction->ValueType = EInputActionValueType::Boolean;

	PitchDownAction = NewObject<UInputAction>(this, TEXT("IA_RaptorPitchDown"));
	PitchDownAction->ValueType = EInputActionValueType::Boolean;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_RaptorExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Raptor"));

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

	// Pitch up: Space
	VehicleMappingContext->MapKey(PitchUpAction, EKeys::SpaceBar);

	// Pitch down: Left Shift
	VehicleMappingContext->MapKey(PitchDownAction, EKeys::LeftShift);

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
	VehicleMappingContext->MapKey(PitchUpAction, EKeys::Gamepad_FaceButton_Bottom);
	VehicleMappingContext->MapKey(PitchDownAction, EKeys::Gamepad_FaceButton_Right);
	VehicleMappingContext->MapKey(ExitVehicleAction, EKeys::Gamepad_FaceButton_Left);
}

// ---------------------------------------------------------------------------
// SetupPlayerInputComponent
// ---------------------------------------------------------------------------

void APHCRaptorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCRaptorPawn::HandleThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCRaptorPawn::HandleThrottleRelease);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCRaptorPawn::HandleBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCRaptorPawn::HandleBrakeRelease);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCRaptorPawn::HandleSteer);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCRaptorPawn::HandleSteerRelease);
		}
		if (PitchUpAction)
		{
			EIC->BindAction(PitchUpAction, ETriggerEvent::Triggered, this, &APHCRaptorPawn::HandlePitchUp);
			EIC->BindAction(PitchUpAction, ETriggerEvent::Completed, this, &APHCRaptorPawn::HandlePitchUpRelease);
		}
		if (PitchDownAction)
		{
			EIC->BindAction(PitchDownAction, ETriggerEvent::Triggered, this, &APHCRaptorPawn::HandlePitchDown);
			EIC->BindAction(PitchDownAction, ETriggerEvent::Completed, this, &APHCRaptorPawn::HandlePitchDownRelease);
		}
		if (ExitVehicleAction)
		{
			EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCRaptorPawn::HandleExitVehicle);
		}
	}
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void APHCRaptorPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsOccupied) return;

	// ---- Cooldowns ----
	if (FireCooldownRemaining > 0.f)
	{
		FireCooldownRemaining -= DeltaTime;
	}
	if (BombCooldownRemaining > 0.f)
	{
		BombCooldownRemaining -= DeltaTime;
	}

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

	// ---- Steering / Yaw (identical inverse-speed scaling to cars) ----
	if (FMath::Abs(CurrentSpeed) > 10.f)
	{
		const float NormSpeed = FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxSpeed, 0.f, 1.f);
		const float SpeedFactor = FMath::Lerp(1.0f, 0.3f, NormSpeed);
		const float YawDelta = SteeringInput * SteeringSpeed * SpeedFactor * DeltaTime;
		FRotator Rot = GetActorRotation();
		Rot.Yaw += YawDelta;
		SetActorRotation(Rot);
	}

	// ---- Pitch (flight control) ----
	{
		FRotator Rot = GetActorRotation();

		if (FMath::Abs(PitchInput) > 0.01f)
		{
			Rot.Pitch += PitchInput * PitchSpeed * DeltaTime;
			Rot.Pitch = FMath::Clamp(Rot.Pitch, -MaxPitchAngle, MaxPitchAngle);
		}
		else
		{
			// Auto-level: gently return pitch to 0
			Rot.Pitch = FMath::FInterpTo(Rot.Pitch, 0.f, DeltaTime, AutoLevelSpeed);
		}

		// Cosmetic roll: bank into turns (~20° max)
		const float TargetRoll = -SteeringInput * 20.f;
		Rot.Roll = FMath::FInterpTo(Rot.Roll, TargetRoll, DeltaTime, 4.f);

		SetActorRotation(Rot);
	}

	// ---- Movement (full 3D – no Z-zeroing) ----
	if (FMath::Abs(CurrentSpeed) > 1.f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Delta = Forward * CurrentSpeed * DeltaTime;
		AddActorWorldOffset(Delta, false);
	}

	// ---- Ground-aware altitude clamp (trace downward to find terrain) ----
	{
		const FVector Pos = GetActorLocation();
		float GroundZ = 0.f; // default ground level

		// Trace downward from high above to find actual terrain surface
		const FVector TraceStart(Pos.X, Pos.Y, Pos.Z + 5000.f);
		const FVector TraceEnd(Pos.X, Pos.Y, Pos.Z - 5000.f);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		TArray<FHitResult> Hits;
		if (GetWorld()->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (Hit.bStartPenetrating) continue;
				GroundZ = Hit.ImpactPoint.Z;
				break;
			}
		}

		const float FloorZ = GroundZ + MinAltitude;
		if (Pos.Z < FloorZ)
		{
			SetActorLocation(FVector(Pos.X, Pos.Y, FloorZ));
			FRotator Rot = GetActorRotation();
			if (Rot.Pitch < 0.f)
			{
				Rot.Pitch = FMath::FInterpTo(Rot.Pitch, 0.f, DeltaTime, 8.f);
				SetActorRotation(Rot);
			}
		}
	}

	// ---- Afterburner glow (emissive when throttle > 0) ----
	{
		const FLinearColor GlowColor = (ThrottleInput > 0.f)
			? FLinearColor(5.0f, 2.5f, 0.3f)
			: FLinearColor::Black;
		if (AfterburnerMatL)
			AfterburnerMatL->SetVectorParameterValue(TEXT("EmissiveColor"), GlowColor);
		if (AfterburnerMatR)
			AfterburnerMatR->SetVectorParameterValue(TEXT("EmissiveColor"), GlowColor);
	}
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void APHCRaptorPawn::HandleThrottle(const FInputActionValue&)
{
	ThrottleInput = 1.f;
	if (!HasAuthority()) ServerSetThrottle(1.f);
}
void APHCRaptorPawn::HandleThrottleRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
	if (!HasAuthority()) ServerSetThrottle(0.f);
}
void APHCRaptorPawn::HandleBrake(const FInputActionValue&)
{
	ThrottleInput = -1.f;
	if (!HasAuthority()) ServerSetThrottle(-1.f);
}
void APHCRaptorPawn::HandleBrakeRelease(const FInputActionValue&)
{
	if (ThrottleInput < 0.f)
	{
		ThrottleInput = 0.f;
		if (!HasAuthority()) ServerSetThrottle(0.f);
	}
}
void APHCRaptorPawn::HandleSteer(const FInputActionValue& Value)
{
	SteeringInput = Value.Get<float>();
	if (!HasAuthority()) ServerSetSteering(SteeringInput);
}
void APHCRaptorPawn::HandleSteerRelease(const FInputActionValue&)
{
	SteeringInput = 0.f;
	if (!HasAuthority()) ServerSetSteering(0.f);
}
void APHCRaptorPawn::HandlePitchUp(const FInputActionValue&)
{
	PitchInput = 1.f;
	if (!HasAuthority()) ServerSetPitch(1.f);
}
void APHCRaptorPawn::HandlePitchUpRelease(const FInputActionValue&)
{
	if (PitchInput > 0.f)
	{
		PitchInput = 0.f;
		if (!HasAuthority()) ServerSetPitch(0.f);
	}
}
void APHCRaptorPawn::HandlePitchDown(const FInputActionValue&)
{
	PitchInput = -1.f;
	if (!HasAuthority()) ServerSetPitch(-1.f);
}
void APHCRaptorPawn::HandlePitchDownRelease(const FInputActionValue&)
{
	if (PitchInput < 0.f)
	{
		PitchInput = 0.f;
		if (!HasAuthority()) ServerSetPitch(0.f);
	}
}
void APHCRaptorPawn::HandleExitVehicle(const FInputActionValue&)
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
// Rocket launcher
// ---------------------------------------------------------------------------

void APHCRaptorPawn::FireRocket()
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
		+ GetActorForwardVector() * 550.f
		+ FVector(0.f, 0.f, -10.f);
	const FRotator MuzzleRot = GetActorForwardVector().Rotation();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APHCMissileProjectile* Proj = World->SpawnActor<APHCMissileProjectile>(
		APHCMissileProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
	if (Proj)
	{
		Proj->OwnerVehicle = this;
		Proj->Damage = 100.f;

		// Override velocity: high-velocity rocket
		Proj->ManualVelocity = MuzzleRot.Vector() * 25000.f;

		// Neon orange emissive color for Raptor rounds
		if (Proj->ProjectileMesh)
		{
			UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(
				Proj->ProjectileMesh->GetMaterial(0));
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"),
					FLinearColor(1.0f, 0.4f, 0.02f));
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(5.0f, 1.5f, 0.1f));
			}
		}
	}
}

void APHCRaptorPawn::ServerFireRocket_Implementation()
{
	// Kept for RPC signature — firing now handled locally in FireRocket().
}

// ---------------------------------------------------------------------------
// Bomb drop
// ---------------------------------------------------------------------------

void APHCRaptorPawn::DropBomb()
{
	if (BombCooldownRemaining > 0.f)
	{
		return;
	}
	BombCooldownRemaining = BombCooldown;
	ServerDropBomb();
}

void APHCRaptorPawn::ServerDropBomb_Implementation()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Spawn bomb directly below the jet
	const FVector BombLoc = GetActorLocation() + FVector(0.f, 0.f, -50.f);
	const FRotator BombRot(-90.f, 0.f, 0.f); // Pointing straight down

	FActorSpawnParameters SP;
	SP.Owner = this;
	SP.Instigator = GetInstigator();
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APHCMissileProjectile* Bomb = World->SpawnActor<APHCMissileProjectile>(
		APHCMissileProjectile::StaticClass(), BombLoc, BombRot, SP);
	if (Bomb)
	{
		Bomb->OwnerVehicle = this;
		Bomb->Damage = 500.f;
		Bomb->OnHitSpawnClass = APHCNukeExplosion::StaticClass();
		Bomb->SetActorTickEnabled(true);

		// Disable collision and ProjectileMovement entirely — the bomb uses
		// manual gravity in Tick so it reliably falls through everything.
		if (Bomb->CollisionSphere)
		{
			Bomb->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (Bomb->ProjectileMovement)
		{
			Bomb->ProjectileMovement->Deactivate();
		}

		// Manual gravity: starts at -300 UU/s downward, accelerates at 3x gravity
		Bomb->bUseManualGravity = true;
		Bomb->ManualVelocity = FVector(0.f, 0.f, -300.f);
		Bomb->ManualGravityScale = 3.f;

		// Ground is at Z=0, detonate just above it
		Bomb->DetonateAtAltitude = 5.f;

		// Bigger bomb shape
		if (Bomb->ProjectileMesh)
		{
			Bomb->ProjectileMesh->SetRelativeScale3D(FVector(0.6f, 0.5f, 0.5f));
		}

		// Neon orange glow (same as rockets)
		const FLinearColor NeonOrange(5.0f, 1.5f, 0.1f);
		if (Bomb->ProjectileMesh)
		{
			UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(
				Bomb->ProjectileMesh->GetMaterial(0));
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), NeonOrange);
			}
		}

		TArray<UPointLightComponent*> Lights;
		Bomb->GetComponents<UPointLightComponent>(Lights);
		for (UPointLightComponent* L : Lights)
		{
			L->SetLightColor(FLinearColor(1.0f, 0.4f, 0.02f));
			L->SetIntensity(6000.f);
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void APHCRaptorPawn::SetBodyColor(const FLinearColor& Color)
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

void APHCRaptorPawn::SetHeadlightsEnabled(bool bEnabled)
{
	// No headlight spotlights on jet – nav lights are always on.
}

FVector APHCRaptorPawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -400.f + FVector(0.f, 0.f, 100.f);
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCRaptorPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCRaptorPawn, bIsOccupied);
}

void APHCRaptorPawn::ServerSetThrottle_Implementation(float Value)
{
	ThrottleInput = Value;
	if (FMath::Abs(Value) > 0.01f)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("Raptor ServerSetThrottle: %.2f Speed=%.1f Occupied=%d"), Value, CurrentSpeed, bIsOccupied ? 1 : 0);
	}
}
void APHCRaptorPawn::ServerSetSteering_Implementation(float Value) { SteeringInput = Value; }
void APHCRaptorPawn::ServerSetPitch_Implementation(float Value) { PitchInput = Value; }
