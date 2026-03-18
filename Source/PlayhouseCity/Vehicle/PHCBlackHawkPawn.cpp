// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCBlackHawkPawn.h"
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
#include "Components/SpotLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Vehicle/PHCNukeExplosion.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCBlackHawkPawn::APHCBlackHawkPawn()
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
	// UH-60 Black Hawk — military utility helicopter
	//
	// Fuselage ~400 UU long, 4-blade main rotor, 2-blade tail rotor,
	// landing skids, engine housings on top, tail boom with vertical stab.
	// ========================================================================

	// ---- Collision box (root) ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(300.f, 100.f, 80.f));
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

	// ====================================================================
	//  FUSELAGE — main body, belly, rear taper, tail boom
	// ====================================================================

	FuselageMain = MakeCube(TEXT("FuselageMain"),
		FVector(0.f, 0.f, 0.f), FVector(400.f, 150.f, 120.f));
	BodyColoredMeshes.Add(FuselageMain);

	FuselageBelly = MakeCube(TEXT("FuselageBelly"),
		FVector(0.f, 0.f, -65.f), FVector(350.f, 140.f, 15.f));
	BodyColoredMeshes.Add(FuselageBelly);

	FuselageRear = MakeCube(TEXT("FuselageRear"),
		FVector(-220.f, 0.f, 10.f), FVector(120.f, 80.f, 70.f));
	BodyColoredMeshes.Add(FuselageRear);

	TailBoom = MakeCube(TEXT("TailBoom"),
		FVector(-450.f, 0.f, 20.f), FVector(400.f, 40.f, 35.f));
	BodyColoredMeshes.Add(TailBoom);

	// ====================================================================
	//  ENGINE HOUSINGS — on top of fuselage
	// ====================================================================

	EngineHousingL = MakeCube(TEXT("EngineHousingL"),
		FVector(30.f, 50.f, 68.f), FVector(120.f, 55.f, 50.f));
	BodyColoredMeshes.Add(EngineHousingL);

	EngineHousingR = MakeCube(TEXT("EngineHousingR"),
		FVector(30.f, -50.f, 68.f), FVector(120.f, 55.f, 50.f));
	BodyColoredMeshes.Add(EngineHousingR);

	// ====================================================================
	//  MAIN ROTOR — hub cylinder + 4 blade cubes
	// ====================================================================

	MainRotorHub = MakeCyl(TEXT("MainRotorHub"),
		FVector(10.f, 0.f, 100.f), FVector(0.18f, 0.18f, 0.20f));

	// Blades: 350 UU long, 35 UU chord, 6 UU thick — visible from chase cam
	MainBlade0 = MakeCube(TEXT("MainBlade0"),
		FVector(10.f, 0.f, 108.f), FVector(350.f, 35.f, 6.f));

	MainBlade1 = MakeCube(TEXT("MainBlade1"),
		FVector(10.f, 0.f, 108.f), FVector(350.f, 35.f, 6.f));

	MainBlade2 = MakeCube(TEXT("MainBlade2"),
		FVector(10.f, 0.f, 108.f), FVector(350.f, 35.f, 6.f));

	MainBlade3 = MakeCube(TEXT("MainBlade3"),
		FVector(10.f, 0.f, 108.f), FVector(350.f, 35.f, 6.f));

	// Rotor mast (shaft connecting fuselage top to hub)
	UStaticMeshComponent* RotorMast = MakeCyl(TEXT("RotorMast"),
		FVector(10.f, 0.f, 90.f), FVector(0.06f, 0.06f, 0.20f));
	BodyColoredMeshes.Add(RotorMast);

	// ====================================================================
	//  TAIL ROTOR — hub cylinder + 2 blade cubes
	// ====================================================================

	TailRotorHub = MakeCyl(TEXT("TailRotorHub"),
		FVector(-610.f, 25.f, 55.f), FVector(0.10f, 0.10f, 0.10f));

	TailBlade0 = MakeCube(TEXT("TailBlade0"),
		FVector(-610.f, 30.f, 55.f), FVector(70.f, 4.f, 14.f));

	TailBlade1 = MakeCube(TEXT("TailBlade1"),
		FVector(-610.f, 30.f, 55.f), FVector(70.f, 4.f, 14.f));

	// ====================================================================
	//  TAIL STABILIZERS
	// ====================================================================

	VerticalStab = MakeCube(TEXT("VerticalStab"),
		FVector(-600.f, 0.f, 55.f), FVector(60.f, 8.f, 70.f));
	BodyColoredMeshes.Add(VerticalStab);

	HorizStabL = MakeCube(TEXT("HorizStabL"),
		FVector(-590.f, 50.f, 35.f), FVector(50.f, 60.f, 5.f));
	BodyColoredMeshes.Add(HorizStabL);

	HorizStabR = MakeCube(TEXT("HorizStabR"),
		FVector(-590.f, -50.f, 35.f), FVector(50.f, 60.f, 5.f));
	BodyColoredMeshes.Add(HorizStabR);

	// ====================================================================
	//  LANDING SKIDS — two rails + four struts
	// ====================================================================

	SkidL = MakeCube(TEXT("SkidL"),
		FVector(0.f, 80.f, -85.f), FVector(200.f, 8.f, 8.f));
	BodyColoredMeshes.Add(SkidL);

	SkidR = MakeCube(TEXT("SkidR"),
		FVector(0.f, -80.f, -85.f), FVector(200.f, 8.f, 8.f));
	BodyColoredMeshes.Add(SkidR);

	StrutFL = MakeCube(TEXT("StrutFL"),
		FVector(60.f, 80.f, -60.f), FVector(6.f, 6.f, 50.f));
	BodyColoredMeshes.Add(StrutFL);

	StrutFR = MakeCube(TEXT("StrutFR"),
		FVector(60.f, -80.f, -60.f), FVector(6.f, 6.f, 50.f));
	BodyColoredMeshes.Add(StrutFR);

	StrutRL = MakeCube(TEXT("StrutRL"),
		FVector(-60.f, 80.f, -60.f), FVector(6.f, 6.f, 50.f));
	BodyColoredMeshes.Add(StrutRL);

	StrutRR = MakeCube(TEXT("StrutRR"),
		FVector(-60.f, -80.f, -60.f), FVector(6.f, 6.f, 50.f));
	BodyColoredMeshes.Add(StrutRR);

	// ====================================================================
	//  COCKPIT GLASS
	// ====================================================================

	CockpitGlass = MakeCube(TEXT("CockpitGlass"),
		FVector(160.f, 0.f, 30.f), FVector(100.f, 130.f, 80.f));

	// ====================================================================
	//  DOORS
	// ====================================================================

	DoorL = MakeCube(TEXT("DoorL"),
		FVector(-30.f, 76.f, -5.f), FVector(100.f, 4.f, 80.f));
	BodyColoredMeshes.Add(DoorL);

	DoorR = MakeCube(TEXT("DoorR"),
		FVector(-30.f, -76.f, -5.f), FVector(100.f, 4.f, 80.f));
	BodyColoredMeshes.Add(DoorR);

	// ====================================================================
	//  NAVIGATION LIGHTS
	// ====================================================================

	NavLightL = MakeCube(TEXT("NavLightL"),
		FVector(0.f, 78.f, 40.f), FVector(6.f, 4.f, 4.f));

	NavLightR = MakeCube(TEXT("NavLightR"),
		FVector(0.f, -78.f, 40.f), FVector(6.f, 4.f, 4.f));

	TailLight = MakeCube(TEXT("TailLight"),
		FVector(-620.f, 0.f, 55.f), FVector(6.f, 6.f, 4.f));

	// ====================================================================
	//  HEADLIGHT SPOTLIGHTS
	// ====================================================================

	HeadlightSpotL = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotL"));
	HeadlightSpotL->SetupAttachment(VehicleCollision);
	HeadlightSpotL->SetRelativeLocation(FVector(350.f, -60.f, -60.f));
	HeadlightSpotL->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	HeadlightSpotL->SetIntensity(80000.f);
	HeadlightSpotL->SetAttenuationRadius(6000.f);
	HeadlightSpotL->SetOuterConeAngle(35.f);
	HeadlightSpotL->SetInnerConeAngle(20.f);
	HeadlightSpotL->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotL->SetCastShadows(false);
	HeadlightSpotL->SetVisibility(false);

	HeadlightSpotR = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotR"));
	HeadlightSpotR->SetupAttachment(VehicleCollision);
	HeadlightSpotR->SetRelativeLocation(FVector(350.f, 60.f, -60.f));
	HeadlightSpotR->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	HeadlightSpotR->SetIntensity(80000.f);
	HeadlightSpotR->SetAttenuationRadius(6000.f);
	HeadlightSpotR->SetOuterConeAngle(35.f);
	HeadlightSpotR->SetInnerConeAngle(20.f);
	HeadlightSpotR->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotR->SetCastShadows(false);
	HeadlightSpotR->SetVisibility(false);

	// ====================================================================
	//  CAMERA
	// ====================================================================

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleCollision);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 300.f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = true;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 5.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 4.f;

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

void APHCBlackHawkPawn::BeginPlay()
{
	Super::BeginPlay();
	CreateInputAssets();

	// -- Stealth gray colors (matching F-22 Raptor) --
	const FLinearColor StealthGray(0.35f, 0.37f, 0.38f);
	const FLinearColor DarkDoor(0.30f, 0.32f, 0.33f);
	const FLinearColor CanopyTint(0.02f, 0.03f, 0.06f);
	const FLinearColor RotorDark(0.28f, 0.29f, 0.30f);
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
			M->SetScalarParameterValue(TEXT("Metallic"), 0.95f);
			M->SetScalarParameterValue(TEXT("Roughness"), 0.12f);
			Comp->SetMaterial(0, M);
		}
	};

	// Body panels — stealth gray ClearCoat
	if (FuselageMain && FuselageMain->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = ClearCoatMaterial
			? (UMaterialInterface*)ClearCoatMaterial
			: FuselageMain->GetMaterial(0);
		BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, FuselageMain);
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), StealthGray);
			BodyMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.95f);
			BodyMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.12f);
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

	// Doors — slightly darker
	ApplyClearCoat(DoorL, DarkDoor);
	ApplyClearCoat(DoorR, DarkDoor);

	// Cockpit glass
	if (CockpitGlass && CockpitGlass->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = GlassMaterial
			? (UMaterialInterface*)GlassMaterial
			: CockpitGlass->GetMaterial(0);
		UMaterialInstanceDynamic* GlassMat = UMaterialInstanceDynamic::Create(BaseMat, CockpitGlass);
		if (GlassMat)
		{
			GlassMat->SetVectorParameterValue(TEXT("BaseColor"), CanopyTint);
			GlassMat->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
			GlassMat->SetScalarParameterValue(TEXT("Metallic"), 0.3f);
			GlassMat->SetScalarParameterValue(TEXT("Roughness"), 0.02f);
			CockpitGlass->SetMaterial(0, GlassMat);
		}
	}

	// Rotor blades — dark metallic
	ApplyMat(MainRotorHub, RotorDark, 0.8f, 0.3f);
	ApplyMat(MainBlade0, RotorDark, 0.7f, 0.25f);
	ApplyMat(MainBlade1, RotorDark, 0.7f, 0.25f);
	ApplyMat(MainBlade2, RotorDark, 0.7f, 0.25f);
	ApplyMat(MainBlade3, RotorDark, 0.7f, 0.25f);
	ApplyMat(TailRotorHub, RotorDark, 0.8f, 0.3f);
	ApplyMat(TailBlade0, RotorDark, 0.7f, 0.25f);
	ApplyMat(TailBlade1, RotorDark, 0.7f, 0.25f);

	// Navigation lights — emissive
	ApplyMat(NavLightL, NavRed, 0.0f, 0.1f, NavRed);
	ApplyMat(NavLightR, NavGreen, 0.0f, 0.1f, NavGreen);
	ApplyMat(TailLight, LightWhite, 0.0f, 0.1f, LightWhite);
}

// ---------------------------------------------------------------------------
// CreateInputAssets (same flight controls as Blackbird/Raptor)
// ---------------------------------------------------------------------------

void APHCBlackHawkPawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_BlackHawkThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_BlackHawkBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_BlackHawkSteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	PitchUpAction = NewObject<UInputAction>(this, TEXT("IA_BlackHawkPitchUp"));
	PitchUpAction->ValueType = EInputActionValueType::Boolean;

	PitchDownAction = NewObject<UInputAction>(this, TEXT("IA_BlackHawkPitchDown"));
	PitchDownAction->ValueType = EInputActionValueType::Boolean;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_BlackHawkExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_BlackHawk"));

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

	VehicleMappingContext->MapKey(PitchUpAction, EKeys::SpaceBar);
	VehicleMappingContext->MapKey(PitchDownAction, EKeys::LeftShift);
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

void APHCBlackHawkPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCBlackHawkPawn::HandleThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCBlackHawkPawn::HandleThrottleRelease);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCBlackHawkPawn::HandleBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCBlackHawkPawn::HandleBrakeRelease);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCBlackHawkPawn::HandleSteer);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCBlackHawkPawn::HandleSteerRelease);
		}
		if (PitchUpAction)
		{
			EIC->BindAction(PitchUpAction, ETriggerEvent::Triggered, this, &APHCBlackHawkPawn::HandlePitchUp);
			EIC->BindAction(PitchUpAction, ETriggerEvent::Completed, this, &APHCBlackHawkPawn::HandlePitchUpRelease);
		}
		if (PitchDownAction)
		{
			EIC->BindAction(PitchDownAction, ETriggerEvent::Triggered, this, &APHCBlackHawkPawn::HandlePitchDown);
			EIC->BindAction(PitchDownAction, ETriggerEvent::Completed, this, &APHCBlackHawkPawn::HandlePitchDownRelease);
		}
		if (ExitVehicleAction)
		{
			EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCBlackHawkPawn::HandleExitVehicle);
		}
	}
}

// ---------------------------------------------------------------------------
// Tick (flight model + rotor animation)
// ---------------------------------------------------------------------------

void APHCBlackHawkPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ---- Rotor animation (runs whether occupied or not) ----
	{
		const float TargetMainSpeed = bIsOccupied ? 720.f : 0.f;
		MainRotorSpeed = FMath::FInterpTo(MainRotorSpeed, TargetMainSpeed, DeltaTime, 4.f);

		MainRotorAngle += MainRotorSpeed * DeltaTime;
		if (MainRotorAngle > 360.f) MainRotorAngle -= 360.f;

		if (MainBlade0) MainBlade0->SetRelativeRotation(FRotator(0.f, 0.f, MainRotorAngle));
		if (MainBlade1) MainBlade1->SetRelativeRotation(FRotator(0.f, 0.f, MainRotorAngle + 90.f));
		if (MainBlade2) MainBlade2->SetRelativeRotation(FRotator(0.f, 0.f, MainRotorAngle + 180.f));
		if (MainBlade3) MainBlade3->SetRelativeRotation(FRotator(0.f, 0.f, MainRotorAngle + 270.f));

		const float TailSpeed = bIsOccupied ? 1080.f : 0.f;
		TailRotorAngle += TailSpeed * DeltaTime;
		if (TailRotorAngle > 360.f) TailRotorAngle -= 360.f;

		if (TailBlade0) TailBlade0->SetRelativeRotation(FRotator(TailRotorAngle, 0.f, 0.f));
		if (TailBlade1) TailBlade1->SetRelativeRotation(FRotator(TailRotorAngle + 90.f, 0.f, 0.f));
	}

	// ---- Damage flash ----
	if (DamageFlashTimer > 0.f)
	{
		DamageFlashTimer -= DeltaTime;
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("EmissiveColor"),
				DamageFlashTimer > 0.f ? FLinearColor(0.4f, 0.02f, 0.0f) : FLinearColor::Black);
		}
	}

	// ---- Crash sequence ----
	if (bIsCrashing)
	{
		CrashTimer += DeltaTime;

		FVector Pos = GetActorLocation();
		FRotator Rot = GetActorRotation();

		const float FallSpeed = 800.f + CrashTimer * 200.f;
		Pos.Z -= FallSpeed * DeltaTime;
		Rot.Yaw += 180.f * DeltaTime;
		Rot.Roll = FMath::FInterpTo(Rot.Roll, 45.f, DeltaTime, 1.5f);
		Rot.Pitch = FMath::FInterpTo(Rot.Pitch, -20.f, DeltaTime, 1.0f);

		SetActorLocation(Pos);
		SetActorRotation(Rot);

		if (Pos.Z <= 10.f && !IsActorBeingDestroyed())
		{
			bIsCrashing = false;
			UWorld* W = GetWorld();
			if (W)
			{
				FActorSpawnParameters SP;
				SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				W->SpawnActor<APHCNukeExplosion>(APHCNukeExplosion::StaticClass(),
					FVector(Pos.X, Pos.Y, FMath::Max(Pos.Z, 0.f)), FRotator::ZeroRotator, SP);
			}
			SetActorHiddenInGame(true);
			Destroy();
		}
		return;
	}

	if (!bIsOccupied) return;

	// Cooldowns
	if (FireCooldownRemaining > 0.f) FireCooldownRemaining -= DeltaTime;
	if (BombCooldownRemaining > 0.f) BombCooldownRemaining -= DeltaTime;

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
	CurrentSpeed = FMath::Clamp(CurrentSpeed, -MaxSpeed * 0.3f, MaxSpeed);

	// ---- Steering / Yaw ----
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
			Rot.Pitch = FMath::FInterpTo(Rot.Pitch, 0.f, DeltaTime, AutoLevelSpeed);
		}

		const float TargetRoll = -SteeringInput * 15.f;
		Rot.Roll = FMath::FInterpTo(Rot.Roll, TargetRoll, DeltaTime, 4.f);
		SetActorRotation(Rot);
	}

	// ---- Movement (full 3D) ----
	if (FMath::Abs(CurrentSpeed) > 1.f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Delta = Forward * CurrentSpeed * DeltaTime;
		AddActorWorldOffset(Delta, false);
	}

	// ---- Ground-aware altitude clamp ----
	{
		const FVector Pos = GetActorLocation();

		// Trace downward from slightly above the aircraft to find ground below.
		// Start only 500 UU above (not 5000) to avoid hitting ceilings in
		// enclosed spaces like the underground airport.
		const FVector TraceStart(Pos.X, Pos.Y, Pos.Z + 500.f);
		const FVector TraceEnd(Pos.X, Pos.Y, Pos.Z - 5000.f);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		TArray<FHitResult> Hits;

		bool bFoundGround = false;
		float GroundZ = 0.f;

		if (GetWorld()->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			for (const FHitResult& Hit : Hits)
			{
				if (Hit.bStartPenetrating) continue;
				// Only treat hits at or below the aircraft as ground (skip ceilings).
				if (Hit.ImpactPoint.Z <= Pos.Z + 100.f)
				{
					GroundZ = Hit.ImpactPoint.Z;
					bFoundGround = true;
					break;
				}
			}
		}

		if (bFoundGround)
		{
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
	}
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

void APHCBlackHawkPawn::HandleThrottle(const FInputActionValue&)
{
	ThrottleInput = 1.f;
	if (!HasAuthority()) ServerSetThrottle(1.f);
}
void APHCBlackHawkPawn::HandleThrottleRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
	if (!HasAuthority()) ServerSetThrottle(0.f);
}
void APHCBlackHawkPawn::HandleBrake(const FInputActionValue&)
{
	ThrottleInput = -1.f;
	if (!HasAuthority()) ServerSetThrottle(-1.f);
}
void APHCBlackHawkPawn::HandleBrakeRelease(const FInputActionValue&)
{
	if (ThrottleInput < 0.f)
	{
		ThrottleInput = 0.f;
		if (!HasAuthority()) ServerSetThrottle(0.f);
	}
}
void APHCBlackHawkPawn::HandleSteer(const FInputActionValue& Value)
{
	SteeringInput = Value.Get<float>();
	if (!HasAuthority()) ServerSetSteering(SteeringInput);
}
void APHCBlackHawkPawn::HandleSteerRelease(const FInputActionValue&)
{
	SteeringInput = 0.f;
	if (!HasAuthority()) ServerSetSteering(0.f);
}
void APHCBlackHawkPawn::HandlePitchUp(const FInputActionValue&)
{
	PitchInput = 1.f;
	if (!HasAuthority()) ServerSetPitch(1.f);
}
void APHCBlackHawkPawn::HandlePitchUpRelease(const FInputActionValue&)
{
	if (PitchInput > 0.f)
	{
		PitchInput = 0.f;
		if (!HasAuthority()) ServerSetPitch(0.f);
	}
}
void APHCBlackHawkPawn::HandlePitchDown(const FInputActionValue&)
{
	PitchInput = -1.f;
	if (!HasAuthority()) ServerSetPitch(-1.f);
}
void APHCBlackHawkPawn::HandlePitchDownRelease(const FInputActionValue&)
{
	if (PitchInput < 0.f)
	{
		PitchInput = 0.f;
		if (!HasAuthority()) ServerSetPitch(0.f);
	}
}
void APHCBlackHawkPawn::HandleExitVehicle(const FInputActionValue&)
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

void APHCBlackHawkPawn::FireRocket()
{
	if (FireCooldownRemaining > 0.f) return;
	FireCooldownRemaining = FireCooldown;

	UWorld* World = GetWorld();
	if (!World) return;

	const FRotator AimRot = GetActorForwardVector().Rotation();
	const FVector AimDir = GetActorForwardVector();

	const FVector MuzzleLoc = GetActorLocation()
		+ GetActorForwardVector() * 250.f
		+ FVector(0.f, 0.f, -80.f);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APHCMissileProjectile* Proj = World->SpawnActor<APHCMissileProjectile>(
		APHCMissileProjectile::StaticClass(), MuzzleLoc, AimRot, SP);
	if (Proj)
	{
		Proj->OwnerVehicle = this;
		Proj->Damage = 100.f;
		Proj->ManualVelocity = AimDir * 30000.f;

		if (Proj->ProjectileMesh)
		{
			UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(
				Proj->ProjectileMesh->GetMaterial(0));
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"),
					FLinearColor(0.8f, 0.3f, 0.1f));
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(5.0f, 2.5f, 0.5f));
			}
		}
	}
}

void APHCBlackHawkPawn::ServerFireRocket_Implementation()
{
}

// ---------------------------------------------------------------------------
// Bomb drop
// ---------------------------------------------------------------------------

void APHCBlackHawkPawn::DropBomb()
{
	if (BombCooldownRemaining > 0.f) return;
	BombCooldownRemaining = BombCooldown;
	ServerDropBomb();
}

void APHCBlackHawkPawn::ServerDropBomb_Implementation()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector BombLoc = GetActorLocation() + FVector(0.f, 0.f, -50.f);
	const FRotator BombRot(-90.f, 0.f, 0.f);

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

		if (Bomb->CollisionSphere)
			Bomb->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Bomb->ProjectileMovement)
			Bomb->ProjectileMovement->Deactivate();

		Bomb->bUseManualGravity = true;
		Bomb->ManualVelocity = FVector(0.f, 0.f, -300.f);
		Bomb->ManualGravityScale = 3.f;
		Bomb->DetonateAtAltitude = 5.f;

		if (Bomb->ProjectileMesh)
			Bomb->ProjectileMesh->SetRelativeScale3D(FVector(0.6f, 0.5f, 0.5f));

		const FLinearColor OrangeGlow(5.0f, 2.5f, 0.5f);
		if (Bomb->ProjectileMesh)
		{
			UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(
				Bomb->ProjectileMesh->GetMaterial(0));
			if (Mat)
				Mat->SetVectorParameterValue(TEXT("BaseColor"), OrangeGlow);
		}

		TArray<UPointLightComponent*> Lights;
		Bomb->GetComponents<UPointLightComponent>(Lights);
		for (UPointLightComponent* L : Lights)
		{
			L->SetLightColor(FLinearColor(0.8f, 0.4f, 0.1f));
			L->SetIntensity(6000.f);
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void APHCBlackHawkPawn::SetBodyColor(const FLinearColor& Color)
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

void APHCBlackHawkPawn::SetHeadlightsEnabled(bool bEnabled)
{
	if (HeadlightSpotL) HeadlightSpotL->SetVisibility(bEnabled);
	if (HeadlightSpotR) HeadlightSpotR->SetVisibility(bEnabled);
}

FVector APHCBlackHawkPawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -300.f + FVector(0.f, 0.f, 100.f);
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCBlackHawkPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCBlackHawkPawn, bIsOccupied);
}

void APHCBlackHawkPawn::ServerSetThrottle_Implementation(float Value) { ThrottleInput = Value; }
void APHCBlackHawkPawn::ServerSetSteering_Implementation(float Value) { SteeringInput = Value; }
void APHCBlackHawkPawn::ServerSetPitch_Implementation(float Value) { PitchInput = Value; }
