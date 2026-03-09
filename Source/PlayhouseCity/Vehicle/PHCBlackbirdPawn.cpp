// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCBlackbirdPawn.h"
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

APHCBlackbirdPawn::APHCBlackbirdPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 15.f;
	bAlwaysRelevant = true;

	// Prevent AI auto-possession — this pawn is pre-placed in the world
	// and must remain unpossessed until a player enters it.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

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
	// SR-71 Blackbird — iconic Mach 3+ reconnaissance aircraft
	//
	// ~1100 UU long × ~700 UU wingspan × ~80 UU tall.
	// Distinctive features: very long slender fuselage with chines,
	// twin nacelle engines on delta wings, canted-inward vertical stabs,
	// pointed nose cone.
	// ========================================================================

	// ---- Collision box (root) ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(600.f, 400.f, 25.f));
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
	//  FUSELAGE — very long, slender, flattened cross-section
	// ====================================================================

	FuselageMain = MakeCube(TEXT("FuselageMain"),
		FVector(0.f, 0.f, 0.f), FVector(900.f, 120.f, 22.f));
	BodyColoredMeshes.Add(FuselageMain);

	// Upper spine ridge
	FuselageSpine = MakeCube(TEXT("FuselageSpine"),
		FVector(-50.f, 0.f, 12.f), FVector(600.f, 30.f, 5.f));
	BodyColoredMeshes.Add(FuselageSpine);

	// Belly fairing
	FuselageBelly = MakeCube(TEXT("FuselageBelly"),
		FVector(-50.f, 0.f, -12.f), FVector(650.f, 100.f, 5.f));
	BodyColoredMeshes.Add(FuselageBelly);

	// Nose section (long tapered forward section)
	NoseSection = MakeCube(TEXT("NoseSection"),
		FVector(440.f, 0.f, 0.f), FVector(280.f, 65.f, 14.f));
	BodyColoredMeshes.Add(NoseSection);

	// Nose cone (sharp pointed)
	NoseCone = MakeCone(TEXT("NoseCone"),
		FVector(620.f, 0.f, -1.f), FVector(0.25f, 0.15f, 0.60f));
	if (NoseCone) NoseCone->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	BodyColoredMeshes.Add(NoseCone);

	// ====================================================================
	//  CHINES — fuselage-blended strakes (SR-71 signature leading edge)
	// ====================================================================

	ChineL = MakeCube(TEXT("ChineL"),
		FVector(100.f, 80.f, -2.f), FVector(500.f, 40.f, 6.f));
	if (ChineL) ChineL->SetRelativeRotation(FRotator(0.f, -5.f, -2.f));
	BodyColoredMeshes.Add(ChineL);

	ChineR = MakeCube(TEXT("ChineR"),
		FVector(100.f, -80.f, -2.f), FVector(500.f, 40.f, 6.f));
	if (ChineR) ChineR->SetRelativeRotation(FRotator(0.f, 5.f, 2.f));
	BodyColoredMeshes.Add(ChineR);

	// ====================================================================
	//  WINGS — delta planform blending into nacelles
	// ====================================================================

	WingL = MakeCube(TEXT("WingL"),
		FVector(-120.f, 280.f, -4.f), FVector(350.f, 200.f, 5.f));
	if (WingL) WingL->SetRelativeRotation(FRotator(0.f, -10.f, -1.f));
	BodyColoredMeshes.Add(WingL);

	WingR = MakeCube(TEXT("WingR"),
		FVector(-120.f, -280.f, -4.f), FVector(350.f, 200.f, 5.f));
	if (WingR) WingR->SetRelativeRotation(FRotator(0.f, 10.f, 1.f));
	BodyColoredMeshes.Add(WingR);

	// ====================================================================
	//  ENGINE NACELLES — large cylindrical pods mid-wing
	// ====================================================================

	NacelleL = MakeCyl(TEXT("NacelleL"),
		FVector(-80.f, 210.f, -2.f), FVector(0.55f, 0.55f, 3.2f));
	BodyColoredMeshes.Add(NacelleL);

	NacelleR = MakeCyl(TEXT("NacelleR"),
		FVector(-80.f, -210.f, -2.f), FVector(0.55f, 0.55f, 3.2f));
	BodyColoredMeshes.Add(NacelleR);

	// Inlet shock cones (sharp spike in front of each nacelle)
	InletSpikeL = MakeCone(TEXT("InletSpikeL"),
		FVector(200.f, 210.f, -2.f), FVector(0.18f, 0.18f, 0.80f));
	if (InletSpikeL) InletSpikeL->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	BodyColoredMeshes.Add(InletSpikeL);

	InletSpikeR = MakeCone(TEXT("InletSpikeR"),
		FVector(200.f, -210.f, -2.f), FVector(0.18f, 0.18f, 0.80f));
	if (InletSpikeR) InletSpikeR->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	BodyColoredMeshes.Add(InletSpikeR);

	// ====================================================================
	//  ENGINE NOZZLES
	// ====================================================================

	NozzleL = MakeCube(TEXT("NozzleL"),
		FVector(-400.f, 210.f, -2.f), FVector(28.f, 28.f, 22.f));

	NozzleR = MakeCube(TEXT("NozzleR"),
		FVector(-400.f, -210.f, -2.f), FVector(28.f, 28.f, 22.f));

	// ====================================================================
	//  AFTERBURNER CONES
	// ====================================================================

	AfterburnerL = MakeCone(TEXT("AfterburnerL"),
		FVector(-440.f, 210.f, -2.f), FVector(0.16f, 0.16f, 0.45f));
	if (AfterburnerL) AfterburnerL->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));

	AfterburnerR = MakeCone(TEXT("AfterburnerR"),
		FVector(-440.f, -210.f, -2.f), FVector(0.16f, 0.16f, 0.45f));
	if (AfterburnerR) AfterburnerR->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));

	// ====================================================================
	//  VERTICAL STABILIZERS (canted INWARD — SR-71 signature, ~15°)
	// ====================================================================

	VertStabL = MakeCube(TEXT("VertStabL"),
		FVector(-320.f, 210.f, 40.f), FVector(100.f, 8.f, 80.f));
	if (VertStabL) VertStabL->SetRelativeRotation(FRotator(0.f, 0.f, 15.f));
	BodyColoredMeshes.Add(VertStabL);

	VertStabR = MakeCube(TEXT("VertStabR"),
		FVector(-320.f, -210.f, 40.f), FVector(100.f, 8.f, 80.f));
	if (VertStabR) VertStabR->SetRelativeRotation(FRotator(0.f, 0.f, -15.f));
	BodyColoredMeshes.Add(VertStabR);

	// ====================================================================
	//  COCKPIT CANOPY
	// ====================================================================

	CockpitCanopy = MakeCube(TEXT("CockpitCanopy"),
		FVector(280.f, 0.f, 18.f), FVector(100.f, 40.f, 14.f));

	// Canopy frame
	UStaticMeshComponent* CanopyFrame = MakeCube(TEXT("CanopyFrame"),
		FVector(280.f, 0.f, 16.f), FVector(110.f, 44.f, 3.f));
	BodyColoredMeshes.Add(CanopyFrame);

	// ====================================================================
	//  NAVIGATION LIGHTS
	// ====================================================================

	NavLightL = MakeCube(TEXT("NavLightL"),
		FVector(-200.f, 380.f, -3.f), FVector(6.f, 4.f, 4.f));

	NavLightR = MakeCube(TEXT("NavLightR"),
		FVector(-200.f, -380.f, -3.f), FVector(6.f, 4.f, 4.f));

	TailLight = MakeCube(TEXT("TailLight"),
		FVector(-460.f, 0.f, 8.f), FVector(6.f, 6.f, 4.f));

	// ====================================================================
	//  FORMATION LIGHTS (electroluminescent strips for night formation flying)
	// ====================================================================

	// Fuselage spine strip (top centerline)
	FormationLightStrips.Add(MakeCube(TEXT("FmtSpine"),
		FVector(0.f, 0.f, 16.f), FVector(400.f, 3.f, 1.f)));

	// Wing leading edge strips
	FormationLightStrips.Add(MakeCube(TEXT("FmtWingL"),
		FVector(20.f, 250.f, -2.f), FVector(220.f, 2.f, 1.f)));
	FormationLightStrips.Add(MakeCube(TEXT("FmtWingR"),
		FVector(20.f, -250.f, -2.f), FVector(220.f, 2.f, 1.f)));

	// Nacelle side strips
	FormationLightStrips.Add(MakeCube(TEXT("FmtNacL"),
		FVector(-80.f, 240.f, 0.f), FVector(200.f, 1.f, 1.f)));
	FormationLightStrips.Add(MakeCube(TEXT("FmtNacR"),
		FVector(-80.f, -240.f, 0.f), FVector(200.f, 1.f, 1.f)));

	// Vertical stabilizer leading edge strips
	FormationLightStrips.Add(MakeCube(TEXT("FmtVStabL"),
		FVector(-280.f, 205.f, 50.f), FVector(2.f, 2.f, 55.f)));
	FormationLightStrips.Add(MakeCube(TEXT("FmtVStabR"),
		FVector(-280.f, -205.f, 50.f), FVector(2.f, 2.f, 55.f)));

	// All strips hidden by default
	for (UStaticMeshComponent* Strip : FormationLightStrips)
	{
		if (Strip) Strip->SetVisibility(false);
	}

	// Subtle ambient glow from formation lights
	FormationGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("FormationGlow"));
	FormationGlow->SetupAttachment(VehicleCollision);
	FormationGlow->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	FormationGlow->SetIntensity(3000.f);
	FormationGlow->SetAttenuationRadius(1200.f);
	FormationGlow->SetLightColor(FLinearColor(0.2f, 0.8f, 0.3f));
	FormationGlow->SetCastShadows(false);
	FormationGlow->SetVisibility(false);

	// Headlight spotlights — illuminate area ahead of the jet
	HeadlightSpotL = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotL"));
	HeadlightSpotL->SetupAttachment(VehicleCollision);
	HeadlightSpotL->SetRelativeLocation(FVector(350.f, -50.f, -10.f));
	HeadlightSpotL->SetRelativeRotation(FRotator(-5.f, 0.f, 0.f));
	HeadlightSpotL->SetIntensity(80000.f);
	HeadlightSpotL->SetAttenuationRadius(8000.f);
	HeadlightSpotL->SetOuterConeAngle(30.f);
	HeadlightSpotL->SetInnerConeAngle(18.f);
	HeadlightSpotL->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotL->SetCastShadows(false);
	HeadlightSpotL->SetVisibility(false);

	HeadlightSpotR = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotR"));
	HeadlightSpotR->SetupAttachment(VehicleCollision);
	HeadlightSpotR->SetRelativeLocation(FVector(350.f, 50.f, -10.f));
	HeadlightSpotR->SetRelativeRotation(FRotator(-5.f, 0.f, 0.f));
	HeadlightSpotR->SetIntensity(80000.f);
	HeadlightSpotR->SetAttenuationRadius(8000.f);
	HeadlightSpotR->SetOuterConeAngle(30.f);
	HeadlightSpotR->SetInnerConeAngle(18.f);
	HeadlightSpotR->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotR->SetCastShadows(false);
	HeadlightSpotR->SetVisibility(false);

	// ====================================================================
	//  CAMERA
	// ====================================================================

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleCollision);
	CameraBoom->TargetArmLength = 1200.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 400.f);
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

void APHCBlackbirdPawn::BeginPlay()
{
	Super::BeginPlay();
	CreateInputAssets();

	// -- Gold plating colors --
	const FLinearColor GoldBody(0.83f, 0.68f, 0.22f);
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

	// Body panels — gold ClearCoat
	if (FuselageMain && FuselageMain->GetStaticMesh())
	{
		UMaterialInterface* BaseMat = ClearCoatMaterial
			? (UMaterialInterface*)ClearCoatMaterial
			: FuselageMain->GetMaterial(0);
		BodyMaterial = UMaterialInstanceDynamic::Create(BaseMat, FuselageMain);
		if (BodyMaterial)
		{
			BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), GoldBody);
			BodyMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.95f);
			BodyMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.12f);
			FuselageMain->SetMaterial(0, BodyMaterial);
		}
	}
	for (UStaticMeshComponent* C : BodyColoredMeshes)
	{
		if (C != FuselageMain)
		{
			ApplyClearCoat(C, GoldBody);
		}
	}

	// Cockpit canopy — Glass
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

	// Engine nozzles — dark metallic
	ApplyMat(NozzleL, DarkNozzle, 0.8f, 0.35f);
	ApplyMat(NozzleR, DarkNozzle, 0.8f, 0.35f);

	// Afterburners — orange emissive
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

	// Navigation lights — emissive
	ApplyMat(NavLightL, NavRed, 0.0f, 0.1f, NavRed);
	ApplyMat(NavLightR, NavGreen, 0.0f, 0.1f, NavGreen);
	ApplyMat(TailLight, LightWhite, 0.0f, 0.1f, LightWhite);

	// Formation light strips — green electroluminescent (initially off)
	const FLinearColor FmtGreen(0.1f, 0.4f, 0.15f);
	for (UStaticMeshComponent* Strip : FormationLightStrips)
	{
		ApplyMat(Strip, FmtGreen, 0.0f, 0.2f, FLinearColor::Black);
	}
}

// ---------------------------------------------------------------------------
// CreateInputAssets (same flight controls as F-22)
// ---------------------------------------------------------------------------

void APHCBlackbirdPawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_BlackbirdThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_BlackbirdBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_BlackbirdSteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	PitchUpAction = NewObject<UInputAction>(this, TEXT("IA_BlackbirdPitchUp"));
	PitchUpAction->ValueType = EInputActionValueType::Boolean;

	PitchDownAction = NewObject<UInputAction>(this, TEXT("IA_BlackbirdPitchDown"));
	PitchDownAction->ValueType = EInputActionValueType::Boolean;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_BlackbirdExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Blackbird"));

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

void APHCBlackbirdPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCBlackbirdPawn::HandleThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCBlackbirdPawn::HandleThrottleRelease);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCBlackbirdPawn::HandleBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCBlackbirdPawn::HandleBrakeRelease);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCBlackbirdPawn::HandleSteer);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCBlackbirdPawn::HandleSteerRelease);
		}
		if (PitchUpAction)
		{
			EIC->BindAction(PitchUpAction, ETriggerEvent::Triggered, this, &APHCBlackbirdPawn::HandlePitchUp);
			EIC->BindAction(PitchUpAction, ETriggerEvent::Completed, this, &APHCBlackbirdPawn::HandlePitchUpRelease);
		}
		if (PitchDownAction)
		{
			EIC->BindAction(PitchDownAction, ETriggerEvent::Triggered, this, &APHCBlackbirdPawn::HandlePitchDown);
			EIC->BindAction(PitchDownAction, ETriggerEvent::Completed, this, &APHCBlackbirdPawn::HandlePitchDownRelease);
		}
		if (ExitVehicleAction)
		{
			EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCBlackbirdPawn::HandleExitVehicle);
		}
	}
}

// ---------------------------------------------------------------------------
// Tick (same flight model as F-22, but much faster)
// ---------------------------------------------------------------------------

void APHCBlackbirdPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

		const float TargetRoll = -SteeringInput * 20.f;
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

	// ---- Afterburner glow ----
	{
		const FLinearColor GlowColor = (ThrottleInput > 0.f)
			? FLinearColor(6.0f, 3.5f, 0.5f) // brighter gold glow
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

void APHCBlackbirdPawn::HandleThrottle(const FInputActionValue&)
{
	ThrottleInput = 1.f;
	if (!HasAuthority()) ServerSetThrottle(1.f);
}
void APHCBlackbirdPawn::HandleThrottleRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
	if (!HasAuthority()) ServerSetThrottle(0.f);
}
void APHCBlackbirdPawn::HandleBrake(const FInputActionValue&)
{
	ThrottleInput = -1.f;
	if (!HasAuthority()) ServerSetThrottle(-1.f);
}
void APHCBlackbirdPawn::HandleBrakeRelease(const FInputActionValue&)
{
	if (ThrottleInput < 0.f)
	{
		ThrottleInput = 0.f;
		if (!HasAuthority()) ServerSetThrottle(0.f);
	}
}
void APHCBlackbirdPawn::HandleSteer(const FInputActionValue& Value)
{
	SteeringInput = Value.Get<float>();
	if (!HasAuthority()) ServerSetSteering(SteeringInput);
}
void APHCBlackbirdPawn::HandleSteerRelease(const FInputActionValue&)
{
	SteeringInput = 0.f;
	if (!HasAuthority()) ServerSetSteering(0.f);
}
void APHCBlackbirdPawn::HandlePitchUp(const FInputActionValue&)
{
	PitchInput = 1.f;
	if (!HasAuthority()) ServerSetPitch(1.f);
}
void APHCBlackbirdPawn::HandlePitchUpRelease(const FInputActionValue&)
{
	if (PitchInput > 0.f)
	{
		PitchInput = 0.f;
		if (!HasAuthority()) ServerSetPitch(0.f);
	}
}
void APHCBlackbirdPawn::HandlePitchDown(const FInputActionValue&)
{
	PitchInput = -1.f;
	if (!HasAuthority()) ServerSetPitch(-1.f);
}
void APHCBlackbirdPawn::HandlePitchDownRelease(const FInputActionValue&)
{
	if (PitchInput < 0.f)
	{
		PitchInput = 0.f;
		if (!HasAuthority()) ServerSetPitch(0.f);
	}
}
void APHCBlackbirdPawn::HandleExitVehicle(const FInputActionValue&)
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
// Rocket launcher (same as F-22)
// ---------------------------------------------------------------------------

void APHCBlackbirdPawn::FireRocket()
{
	if (FireCooldownRemaining > 0.f) return;
	FireCooldownRemaining = FireCooldown;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector MuzzleLoc = GetActorLocation()
		+ GetActorForwardVector() * 650.f
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
		Proj->ManualVelocity = MuzzleRot.Vector() * 30000.f;

		if (Proj->ProjectileMesh)
		{
			UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(
				Proj->ProjectileMesh->GetMaterial(0));
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"),
					FLinearColor(0.9f, 0.7f, 0.1f));
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(6.0f, 4.0f, 0.5f));
			}
		}
	}
}

void APHCBlackbirdPawn::ServerFireRocket_Implementation()
{
}

// ---------------------------------------------------------------------------
// Bomb drop (same as F-22)
// ---------------------------------------------------------------------------

void APHCBlackbirdPawn::DropBomb()
{
	if (BombCooldownRemaining > 0.f) return;
	BombCooldownRemaining = BombCooldown;
	ServerDropBomb();
}

void APHCBlackbirdPawn::ServerDropBomb_Implementation()
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

		const FLinearColor GoldGlow(6.0f, 4.0f, 0.5f);
		if (Bomb->ProjectileMesh)
		{
			UMaterialInstanceDynamic* Mat = Cast<UMaterialInstanceDynamic>(
				Bomb->ProjectileMesh->GetMaterial(0));
			if (Mat)
				Mat->SetVectorParameterValue(TEXT("BaseColor"), GoldGlow);
		}

		TArray<UPointLightComponent*> Lights;
		Bomb->GetComponents<UPointLightComponent>(Lights);
		for (UPointLightComponent* L : Lights)
		{
			L->SetLightColor(FLinearColor(0.9f, 0.7f, 0.1f));
			L->SetIntensity(6000.f);
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void APHCBlackbirdPawn::SetBodyColor(const FLinearColor& Color)
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

void APHCBlackbirdPawn::SetHeadlightsEnabled(bool bEnabled)
{
	// Toggle formation light strips visibility + emissive
	const FLinearColor Emis = bEnabled
		? FLinearColor(0.5f, 2.5f, 0.8f) : FLinearColor::Black;
	for (UStaticMeshComponent* Strip : FormationLightStrips)
	{
		if (!Strip) continue;
		Strip->SetVisibility(bEnabled);
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Strip->GetMaterial(0)))
		{
			M->SetVectorParameterValue(TEXT("EmissiveColor"), Emis);
		}
	}
	if (FormationGlow) FormationGlow->SetVisibility(bEnabled);
	if (HeadlightSpotL) HeadlightSpotL->SetVisibility(bEnabled);
	if (HeadlightSpotR) HeadlightSpotR->SetVisibility(bEnabled);
}

FVector APHCBlackbirdPawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -500.f + FVector(0.f, 0.f, 100.f);
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCBlackbirdPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCBlackbirdPawn, bIsOccupied);
}

void APHCBlackbirdPawn::ServerSetThrottle_Implementation(float Value) { ThrottleInput = Value; }
void APHCBlackbirdPawn::ServerSetSteering_Implementation(float Value) { SteeringInput = Value; }
void APHCBlackbirdPawn::ServerSetPitch_Implementation(float Value) { PitchInput = Value; }
