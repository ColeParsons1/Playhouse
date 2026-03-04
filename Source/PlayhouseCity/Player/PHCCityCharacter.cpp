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
#include "Components/PointLightComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Core/PHCPlayerController.h"
#include "Core/PHCCitySubsystem.h"
#include "Vehicle/PHCMissileProjectile.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerState.h"
#include "Player/PHCPlayerState.h"

// ---------------------------------------------------------------------------
// Constructor — Fortnite-quality character from basic shapes
// ---------------------------------------------------------------------------

APHCCityCharacter::APHCCityCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// ---- Multiplayer replication ----
	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 15.f;
	bAlwaysRelevant = true;

	// ---- Root: UBoxComponent — same pattern as vehicle ----
	CharacterCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CharacterCollision"));
	CharacterCollision->SetBoxExtent(FVector(22.f, 26.f, 90.f));
	CharacterCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CharacterCollision->SetCollisionResponseToAllChannels(ECR_Block);
	CharacterCollision->SetSimulatePhysics(false);
	CharacterCollision->SetHiddenInGame(true);
	SetRootComponent(CharacterCollision);

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

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// ====================================================================
	//  Fortnite-Quality Character — 45 components
	//
	//  180 UU tall. Root at hips (Z=0 relative). Sphere-based organic body.
	//  Cube=100³ at scale 1, Cyl=radius 50 h 100, Sphere=radius 50.
	//  Stylized proportions: slightly large head (~6.5 heads tall).
	// ====================================================================

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* CylMesh  = CylFinder.Succeeded()  ? CylFinder.Object  : nullptr;
	UStaticMesh* SphMesh  = SphereFinder.Succeeded()? SphereFinder.Object : nullptr;

	// --- Helpers ---

	auto MakeComp = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetSimulatePhysics(false);
		C->CastShadow = false;
		C->bUseAttachParentBound = true;
		C->SetBoundsScale(200.f);
		C->bNeverDistanceCull = true;
		return C;
	};

	auto MakeCube = [&](const TCHAR* Name, const FVector& Loc,
		const FVector& SizeUU) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = MakeComp(Name);
		if (CubeMesh) { C->SetStaticMesh(CubeMesh); C->SetRelativeScale3D(SizeUU / 100.f); C->SetRelativeLocation(Loc); }
		return C;
	};

	auto MakeCyl = [&](const TCHAR* Name, const FVector& Loc,
		float R, float H) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = MakeComp(Name);
		if (CylMesh) { C->SetStaticMesh(CylMesh); C->SetRelativeScale3D(FVector(R/50.f, R/50.f, H/100.f)); C->SetRelativeLocation(Loc); }
		return C;
	};

	auto MakeSph = [&](const TCHAR* Name, const FVector& Loc,
		float R) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = MakeComp(Name);
		if (SphMesh) { C->SetStaticMesh(SphMesh); float S = R/50.f; C->SetRelativeScale3D(FVector(S,S,S)); C->SetRelativeLocation(Loc); }
		return C;
	};

	auto MakeOval = [&](const TCHAR* Name, const FVector& Loc,
		const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = MakeComp(Name);
		if (SphMesh) { C->SetStaticMesh(SphMesh); C->SetRelativeScale3D(Scale); C->SetRelativeLocation(Loc); }
		return C;
	};

	// ================================================================
	//  HEAD — large sphere cranium + overlapping jaw for definition
	// ================================================================
	// Head: radius 14 UU, slightly oval (wider side-to-side for Fortnite look)
	HeadMesh = MakeOval(TEXT("HeadMesh"), FVector(0.f, 0.f, 76.f),
		FVector(0.26f, 0.28f, 0.28f));
	// Jaw: overlapping sphere below head for defined jawline/chin
	JawMesh = MakeOval(TEXT("JawMesh"), FVector(2.f, 0.f, 67.f),
		FVector(0.22f, 0.24f, 0.16f));

	// ================================================================
	//  FACE — eyes, nose, mouth, ears, brows
	// ================================================================
	// Eye whites: small spheres, forward on face
	LeftEyeWhiteMesh  = MakeOval(TEXT("LEyeW"), FVector(9.f, -5.f, 78.f), FVector(0.04f, 0.055f, 0.04f));
	RightEyeWhiteMesh = MakeOval(TEXT("REyeW"), FVector(9.f,  5.f, 78.f), FVector(0.04f, 0.055f, 0.04f));
	// Pupils: tiny dark spheres slightly in front of whites
	LeftPupilMesh  = MakeSph(TEXT("LPupil"), FVector(11.f, -5.f, 78.f), 1.5f);
	RightPupilMesh = MakeSph(TEXT("RPupil"), FVector(11.f,  5.f, 78.f), 1.5f);
	// Nose: small sphere projecting forward
	NoseMesh = MakeOval(TEXT("NoseMesh"), FVector(12.f, 0.f, 73.f), FVector(0.06f, 0.04f, 0.05f));
	// Mouth: thin flattened cube
	MouthMesh = MakeCube(TEXT("MouthMesh"), FVector(10.f, 0.f, 69.f), FVector(2.f, 8.f, 1.5f));
	// Ears: flattened oval spheres on sides of head
	LeftEarMesh  = MakeOval(TEXT("LEar"), FVector(-1.f, -14.f, 75.f), FVector(0.04f, 0.03f, 0.06f));
	RightEarMesh = MakeOval(TEXT("REar"), FVector(-1.f,  14.f, 75.f), FVector(0.04f, 0.03f, 0.06f));
	// Eyebrows: thin cubes above eyes
	LeftBrowMesh  = MakeCube(TEXT("LBrow"), FVector(10.f, -5.f, 82.f), FVector(2.f, 6.f, 1.f));
	RightBrowMesh = MakeCube(TEXT("RBrow"), FVector(10.f,  5.f, 82.f), FVector(2.f, 6.f, 1.f));

	// ================================================================
	//  HAIR — buzz cut / fade: top cap + back + sides
	// ================================================================
	HairTopMesh   = MakeOval(TEXT("HairTop"),   FVector(-1.f, 0.f, 88.f),  FVector(0.28f, 0.30f, 0.10f));
	HairBackMesh  = MakeOval(TEXT("HairBack"),  FVector(-8.f, 0.f, 80.f),  FVector(0.12f, 0.26f, 0.18f));
	HairSideLMesh = MakeOval(TEXT("HairSideL"), FVector(-2.f, -13.f, 80.f), FVector(0.06f, 0.06f, 0.16f));
	HairSideRMesh = MakeOval(TEXT("HairSideR"), FVector(-2.f,  13.f, 80.f), FVector(0.06f, 0.06f, 0.16f));

	// ================================================================
	//  NECK
	// ================================================================
	NeckMesh = MakeCyl(TEXT("NeckMesh"), FVector(0.f, 0.f, 60.f), 4.f, 10.f);

	// ================================================================
	//  TORSO — sphere-based for organic curves (NO cubes)
	// ================================================================
	// Chest: athletic build — wide but not deep
	ChestMesh = MakeOval(TEXT("ChestMesh"), FVector(1.f, 0.f, 44.f),
		FVector(0.22f, 0.44f, 0.26f));
	// Abs: tapered V-taper waist
	AbsMesh = MakeOval(TEXT("AbsMesh"), FVector(0.f, 0.f, 28.f),
		FVector(0.16f, 0.32f, 0.18f));
	// Pelvis/hips: lean
	PelvisMesh = MakeOval(TEXT("PelvisMesh"), FVector(0.f, 0.f, 12.f),
		FVector(0.17f, 0.36f, 0.20f));

	// ================================================================
	//  CLOTHING DETAILS
	// ================================================================
	// T-shirt collar
	CollarMesh = MakeCube(TEXT("CollarMesh"), FVector(2.f, 0.f, 56.f), FVector(10.f, 26.f, 2.f));
	// Belt
	BeltMesh = MakeCube(TEXT("BeltMesh"), FVector(0.f, 0.f, 20.f), FVector(12.f, 36.f, 3.f));
	// Belt buckle (small square in front)
	BeltBuckleMesh = MakeCube(TEXT("BeltBuckle"), FVector(8.f, 0.f, 20.f), FVector(3.f, 4.f, 4.f));

	// ================================================================
	//  LEFT ARM — shoulder sphere → upper arm cyl → elbow → forearm → hand
	// ================================================================
	LeftShoulderMesh = MakeSph(TEXT("LShld"),  FVector(0.f, -25.f, 54.f), 7.f);
	LeftUpperArmMesh = MakeCyl(TEXT("LUArm"),  FVector(0.f, -26.f, 44.f), 4.5f, 20.f);
	LeftElbowMesh    = MakeSph(TEXT("LElbow"), FVector(0.f, -26.f, 32.f), 4.f);
	LeftForearmMesh  = MakeCyl(TEXT("LFArm"),  FVector(0.f, -26.f, 20.f), 4.f, 18.f);
	LeftHandMesh     = MakeSph(TEXT("LHand"),  FVector(0.f, -26.f,  6.f), 3.5f);

	// ================================================================
	//  RIGHT ARM — mirror
	// ================================================================
	RightShoulderMesh = MakeSph(TEXT("RShld"),  FVector(0.f, 25.f, 54.f), 7.f);
	RightUpperArmMesh = MakeCyl(TEXT("RUArm"),  FVector(0.f, 26.f, 44.f), 4.5f, 20.f);
	RightElbowMesh    = MakeSph(TEXT("RElbow"), FVector(0.f, 26.f, 32.f), 4.f);
	RightForearmMesh  = MakeCyl(TEXT("RFArm"),  FVector(0.f, 26.f, 20.f), 4.f, 18.f);
	RightHandMesh     = MakeSph(TEXT("RHand"),  FVector(0.f, 26.f,  6.f), 3.5f);

	// ================================================================
	//  LEFT LEG — thigh → knee → shin → foot + toe cap + sole
	// ================================================================
	LeftThighMesh = MakeCyl(TEXT("LThigh"), FVector(0.f, -11.f, -12.f), 5.5f, 28.f);
	LeftKneeMesh  = MakeSph(TEXT("LKnee"),  FVector(0.f, -11.f, -30.f), 5.f);
	LeftShinMesh  = MakeCyl(TEXT("LShin"),  FVector(0.f, -11.f, -48.f), 4.5f, 24.f);
	LeftFootMesh  = MakeCube(TEXT("LFoot"), FVector(4.f, -11.f, -68.f), FVector(22.f, 12.f, 8.f));
	LeftToeMesh   = MakeOval(TEXT("LToe"),  FVector(14.f, -11.f, -67.f), FVector(0.10f, 0.12f, 0.08f));
	LeftSoleMesh  = MakeCube(TEXT("LSole"), FVector(4.f, -11.f, -73.f), FVector(22.f, 12.f, 2.5f));

	// ================================================================
	//  RIGHT LEG — mirror
	// ================================================================
	RightThighMesh = MakeCyl(TEXT("RThigh"), FVector(0.f, 11.f, -12.f), 5.5f, 28.f);
	RightKneeMesh  = MakeSph(TEXT("RKnee"),  FVector(0.f, 11.f, -30.f), 5.f);
	RightShinMesh  = MakeCyl(TEXT("RShin"),  FVector(0.f, 11.f, -48.f), 4.5f, 24.f);
	RightFootMesh  = MakeCube(TEXT("RFoot"), FVector(4.f, 11.f, -68.f), FVector(22.f, 12.f, 8.f));
	RightToeMesh   = MakeOval(TEXT("RToe"),  FVector(14.f, 11.f, -67.f), FVector(0.10f, 0.12f, 0.08f));
	RightSoleMesh  = MakeCube(TEXT("RSole"), FVector(4.f, 11.f, -73.f), FVector(22.f, 12.f, 2.5f));

	// ================================================================
	//  SCYTHE WEAPON — handle + blade + edge (default equipped)
	// ================================================================
	ScytheHandleMesh = MakeCyl(TEXT("ScytheHandle"), FVector(0.f, 26.f, 56.f), 1.5f, 100.f);
	ScytheBladeMesh  = MakeOval(TEXT("ScytheBlade"),  FVector(14.f, 26.f, 108.f), FVector(0.40f, 0.03f, 0.14f));
	ScytheBladeEdgeMesh = MakeCube(TEXT("ScytheEdge"), FVector(26.f, 26.f, 108.f), FVector(24.f, 1.f, 1.5f));

	// ================================================================
	//  MINIGUN WEAPON — body + barrel cluster + muzzle flash
	// ================================================================
	MinigunBodyMesh   = MakeCyl(TEXT("MinigunBody"),   FVector(0.f, 26.f, 40.f), 3.f, 50.f);
	MinigunBarrelMesh = MakeCyl(TEXT("MinigunBarrel"), FVector(0.f, 26.f, 70.f), 1.5f, 60.f);
	MinigunMuzzleMesh = MakeCyl(TEXT("MinigunMuzzle"), FVector(0.f, 26.f, 100.f), 4.f, 3.f);

	// ================================================================
	//  BAZOOKA (RPG) WEAPON — shoulder-mounted tube launcher
	// ================================================================
	BazookaTubeMesh       = MakeCyl(TEXT("BazookaTube"),       FVector(0.f, 26.f, 56.f), 4.f, 120.f);
	BazookaRearEndMesh    = MakeCyl(TEXT("BazookaRear"),       FVector(0.f, 26.f, -4.f), 5.5f, 8.f);
	BazookaFrontSightMesh = MakeCube(TEXT("BazookaFrontSight"),FVector(0.f, 26.f, 112.f), FVector(2.f, 2.f, 6.f));
	BazookaRearSightMesh  = MakeCube(TEXT("BazookaRearSight"), FVector(0.f, 26.f, 20.f),  FVector(2.f, 2.f, 5.f));
	BazookaGripMesh       = MakeCube(TEXT("BazookaGrip"),      FVector(0.f, 26.f, 40.f),  FVector(3.f, 6.f, 14.f));
	BazookaScopeMesh      = MakeCyl(TEXT("BazookaScope"),      FVector(0.f, 26.f, 80.f), 2.5f, 18.f);
	BazookaMuzzleMesh     = MakeSph(TEXT("BazookaMuzzle"),     FVector(0.f, 26.f, 120.f), 8.f);

	// ================================================================
	//  DAGGER WEAPON — short handle + blade + cross-guard
	// ================================================================
	DaggerHandleMesh = MakeCyl(TEXT("DaggerHandle"), FVector(0.f, 26.f, 10.f), 1.2f, 14.f);
	DaggerBladeMesh  = MakeCube(TEXT("DaggerBlade"),  FVector(0.f, 26.f, 30.f), FVector(2.f, 1.2f, 18.f));
	DaggerGuardMesh  = MakeCube(TEXT("DaggerGuard"),  FVector(0.f, 26.f, 18.f), FVector(8.f, 1.5f, 2.f));

	// ================================================================
	//  AK-47 WEAPON — receiver body + barrel + banana mag + stock + muzzle flash
	// ================================================================
	AK47BodyMesh   = MakeCube(TEXT("AK47Body"),   FVector(0.f, 26.f, 40.f), FVector(28.f, 3.5f, 5.f));
	AK47BarrelMesh = MakeCyl(TEXT("AK47Barrel"),  FVector(0.f, 26.f, 70.f), 1.2f, 50.f);
	AK47MagMesh    = MakeCube(TEXT("AK47Mag"),    FVector(0.f, 26.f, 30.f), FVector(4.f, 2.5f, 16.f));
	AK47StockMesh  = MakeCube(TEXT("AK47Stock"),  FVector(0.f, 26.f, 38.f), FVector(24.f, 3.f, 4.f));
	AK47MuzzleMesh = MakeSph(TEXT("AK47Muzzle"),  FVector(0.f, 26.f, 100.f), 5.f);

	// Enable shadows on major body parts for visual depth and grounding
	auto Shadow = [](UStaticMeshComponent* C) { if (C) C->CastShadow = true; };
	Shadow(HeadMesh); Shadow(JawMesh); Shadow(NeckMesh);
	Shadow(ChestMesh); Shadow(AbsMesh); Shadow(PelvisMesh);
	Shadow(LeftShoulderMesh); Shadow(RightShoulderMesh);
	Shadow(LeftUpperArmMesh); Shadow(RightUpperArmMesh);
	Shadow(LeftForearmMesh); Shadow(RightForearmMesh);
	Shadow(LeftHandMesh); Shadow(RightHandMesh);
	Shadow(LeftThighMesh); Shadow(RightThighMesh);
	Shadow(LeftShinMesh); Shadow(RightShinMesh);
	Shadow(LeftFootMesh); Shadow(RightFootMesh);
	Shadow(ScytheHandleMesh); Shadow(ScytheBladeMesh);

	// Muzzle flash dynamic point light (shared by all ranged weapons)
	MuzzleFlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlashLight"));
	MuzzleFlashLight->SetupAttachment(RootComponent);
	MuzzleFlashLight->SetIntensity(0.f);
	MuzzleFlashLight->SetAttenuationRadius(600.f);
	MuzzleFlashLight->SetLightColor(FColor(255, 180, 80));
	MuzzleFlashLight->SetCastShadows(false);
	MuzzleFlashLight->SetVisibility(false);

	// Massive bounds to prevent any form of culling on mobile
	CharacterCollision->SetBoundsScale(200.f);
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

	// ADS (aim down sights)
	ADSAction = NewObject<UInputAction>(this, TEXT("IA_ADS"));
	ADSAction->ValueType = EInputActionValueType::Boolean;
#if !PLATFORM_IOS && !PLATFORM_ANDROID
	DefaultMappingContext->MapKey(ADSAction, EKeys::RightMouseButton);
#endif
	DefaultMappingContext->MapKey(ADSAction, EKeys::Gamepad_LeftTrigger);
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void APHCCityCharacter::BeginPlay()
{
	Super::BeginPlay();

	// On dedicated server: skip input, materials, and visual setup entirely
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityCharacter::BeginPlay -- Dedicated server, skipping visual setup. Pawn at %s"),
			*GetActorLocation().ToString());
		return;
	}

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
			if (ADSAction)
			{
				EIC->BindAction(ADSAction, ETriggerEvent::Started, this, &APHCCityCharacter::HandleADSStarted);
				EIC->BindAction(ADSAction, ETriggerEvent::Completed, this, &APHCCityCharacter::HandleADSCompleted);
			}
		}
	}

	// ---- PBR Materials — production quality character rendering ----------
	UMaterial* ColorMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));

	// Try subsurface skin material (created by editor on first run)
	UMaterial* SkinMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCSkin.M_PHCSkin"));
	if (!SkinMat) SkinMat = ColorMat; // graceful fallback

	auto ApplyColor = [&](UStaticMeshComponent* Comp,
		const FLinearColor& Color, float Roughness, float Metallic = 0.f)
	{
		if (!Comp || !Comp->GetStaticMesh() || !ColorMat) return;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ColorMat, Comp);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			Comp->SetMaterial(0, MID);
			BodyMIDs.Add(MID);
		}
	};

	// Skin material: warm subsurface glow + emissive warmth
	auto ApplySkin = [&](UStaticMeshComponent* Comp,
		const FLinearColor& Color, float Roughness)
	{
		if (!Comp || !Comp->GetStaticMesh() || !SkinMat) return;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(SkinMat, Comp);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			MID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			// SSS params (effective with M_PHCSkin, ignored by M_PHCColor)
			MID->SetVectorParameterValue(TEXT("SubsurfaceColor"),
				FLinearColor(0.82f, 0.22f, 0.12f));
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.35f);
			// Subtle warm emissive for skin warmth
			MID->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(0.012f, 0.006f, 0.003f));
			Comp->SetMaterial(0, MID);
			BodyMIDs.Add(MID);
		}
	};

	// Color palette
	const FLinearColor Skin(0.55f, 0.38f, 0.26f);           // warm medium brown
	const FLinearColor SkinDark(0.42f, 0.28f, 0.18f);       // darker skin (lips, ears)
	const FLinearColor Hair(0.02f, 0.02f, 0.03f);           // near-black
	const FLinearColor EyeWhite(0.92f, 0.92f, 0.90f);       // off-white
	const FLinearColor Pupil(0.04f, 0.06f, 0.04f);          // very dark
	const FLinearColor BrowColor(0.06f, 0.04f, 0.03f);      // dark brown
	const FLinearColor Shirt(0.12f, 0.38f, 0.18f);          // forest green tee
	const FLinearColor ShirtTrim(0.10f, 0.32f, 0.14f);      // slightly darker trim
	const FLinearColor Denim(0.08f, 0.10f, 0.20f);          // dark indigo jeans
	const FLinearColor Belt(0.06f, 0.04f, 0.03f);           // dark leather
	const FLinearColor Buckle(0.65f, 0.60f, 0.50f);         // brushed metal
	const FLinearColor Shoe(0.92f, 0.92f, 0.90f);           // white sneaker
	const FLinearColor ShoeToe(0.88f, 0.88f, 0.86f);        // slightly off-white
	const FLinearColor Sole(0.05f, 0.05f, 0.05f);           // dark rubber

	// Skin (head, jaw, neck, forearms, elbows, hands) — warm SSS
	ApplySkin(HeadMesh,    Skin, 0.45f);
	ApplySkin(JawMesh,     Skin, 0.45f);
	ApplySkin(NeckMesh,    Skin, 0.50f);
	ApplySkin(LeftElbowMesh,    Skin, 0.45f);
	ApplySkin(RightElbowMesh,   Skin, 0.45f);
	ApplySkin(LeftForearmMesh,  Skin, 0.45f);
	ApplySkin(RightForearmMesh, Skin, 0.45f);
	ApplySkin(LeftHandMesh,     Skin, 0.50f);
	ApplySkin(RightHandMesh,    Skin, 0.50f);

	// Face features
	ApplyColor(LeftEyeWhiteMesh,  EyeWhite, 0.20f);
	ApplyColor(RightEyeWhiteMesh, EyeWhite, 0.20f);
	ApplyColor(LeftPupilMesh,     Pupil, 0.15f);
	ApplyColor(RightPupilMesh,    Pupil, 0.15f);
	ApplySkin(NoseMesh,          Skin, 0.50f);
	ApplySkin(MouthMesh,         SkinDark, 0.60f);
	ApplySkin(LeftEarMesh,       SkinDark, 0.50f);
	ApplySkin(RightEarMesh,      SkinDark, 0.50f);
	ApplyColor(LeftBrowMesh,      BrowColor, 0.80f);
	ApplyColor(RightBrowMesh,     BrowColor, 0.80f);

	// Hair
	ApplyColor(HairTopMesh,   Hair, 0.85f);
	ApplyColor(HairBackMesh,  Hair, 0.85f);
	ApplyColor(HairSideLMesh, Hair, 0.85f);
	ApplyColor(HairSideRMesh, Hair, 0.85f);

	// Shirt (chest, abs, upper arms, shoulders, collar)
	ApplyColor(ChestMesh,          Shirt, 0.82f);
	ApplyColor(AbsMesh,            Shirt, 0.82f);
	ApplyColor(CollarMesh,         ShirtTrim, 0.80f);
	ApplyColor(LeftShoulderMesh,   Shirt, 0.82f);
	ApplyColor(RightShoulderMesh,  Shirt, 0.82f);
	ApplyColor(LeftUpperArmMesh,   Shirt, 0.82f);
	ApplyColor(RightUpperArmMesh,  Shirt, 0.82f);

	// Denim jeans
	ApplyColor(PelvisMesh,     Denim, 0.78f);
	ApplyColor(LeftThighMesh,  Denim, 0.78f);
	ApplyColor(RightThighMesh, Denim, 0.78f);
	ApplyColor(LeftKneeMesh,   Denim, 0.78f);
	ApplyColor(RightKneeMesh,  Denim, 0.78f);
	ApplyColor(LeftShinMesh,   Denim, 0.78f);
	ApplyColor(RightShinMesh,  Denim, 0.78f);

	// Belt + buckle
	ApplyColor(BeltMesh,       Belt, 0.35f);
	ApplyColor(BeltBuckleMesh, Buckle, 0.30f, 0.6f);

	// Shoes
	ApplyColor(LeftFootMesh,   Shoe, 0.50f);
	ApplyColor(RightFootMesh,  Shoe, 0.50f);
	ApplyColor(LeftToeMesh,    ShoeToe, 0.45f);
	ApplyColor(RightToeMesh,   ShoeToe, 0.45f);
	ApplyColor(LeftSoleMesh,   Sole, 0.90f);
	ApplyColor(RightSoleMesh,  Sole, 0.90f);

	// Scythe materials
	const FLinearColor ScytheWood(0.18f, 0.10f, 0.06f);   // dark walnut
	const FLinearColor ScytheSteel(0.20f, 0.22f, 0.25f);  // dark steel blade
	const FLinearColor ScytheEdge(0.75f, 0.78f, 0.80f);   // bright cutting edge
	ApplyColor(ScytheHandleMesh,    ScytheWood,  0.75f);
	ApplyColor(ScytheBladeMesh,     ScytheSteel, 0.30f, 0.85f);
	ApplyColor(ScytheBladeEdgeMesh, ScytheEdge,  0.15f, 0.95f);

	// Minigun materials
	const FLinearColor Gunmetal(0.12f, 0.12f, 0.14f);
	const FLinearColor BarrelSteel(0.25f, 0.25f, 0.28f);
	ApplyColor(MinigunBodyMesh,   Gunmetal,    0.35f, 0.9f);
	ApplyColor(MinigunBarrelMesh, BarrelSteel, 0.25f, 0.95f);
	ApplyColor(MinigunMuzzleMesh, FLinearColor(0.08f, 0.08f, 0.08f), 0.40f, 0.8f);

	// Bazooka materials
	const FLinearColor BazookaOlive(0.22f, 0.24f, 0.14f);  // military olive drab
	const FLinearColor BazookaSight(0.10f, 0.10f, 0.10f);   // dark steel sights
	const FLinearColor BazookaRubber(0.06f, 0.06f, 0.06f);  // dark rubber grip
	const FLinearColor BazookaAnodized(0.04f, 0.04f, 0.06f);// black anodized scope
	ApplyColor(BazookaTubeMesh,       BazookaOlive,    0.85f, 0.25f);
	ApplyColor(BazookaRearEndMesh,    BazookaOlive,    0.85f, 0.25f);
	ApplyColor(BazookaFrontSightMesh, BazookaSight,    0.40f, 0.6f);
	ApplyColor(BazookaRearSightMesh,  BazookaSight,    0.40f, 0.6f);
	ApplyColor(BazookaGripMesh,       BazookaRubber,   0.95f, 0.0f);
	ApplyColor(BazookaScopeMesh,      BazookaAnodized, 0.30f, 0.7f);
	// Muzzle flash: bright emissive (applied manually for emissive)
	if (BazookaMuzzleMesh && BazookaMuzzleMesh->GetStaticMesh() && ColorMat)
	{
		UMaterialInstanceDynamic* MuzMID = UMaterialInstanceDynamic::Create(ColorMat, BazookaMuzzleMesh);
		if (MuzMID)
		{
			MuzMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.f, 0.6f, 0.1f));
			MuzMID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(10.f, 6.f, 2.f));
			MuzMID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			MuzMID->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
			BazookaMuzzleMesh->SetMaterial(0, MuzMID);
		}
	}

	// Dagger materials — crimson-tinted steel blade, dark leather handle
	const FLinearColor DaggerSteel(0.72f, 0.12f, 0.08f);   // crimson steel
	const FLinearColor DaggerHandle(0.08f, 0.06f, 0.04f);  // dark leather
	const FLinearColor DaggerGuard(0.60f, 0.58f, 0.50f);   // brushed brass
	ApplyColor(DaggerBladeMesh,  DaggerSteel,  0.18f, 0.90f);
	ApplyColor(DaggerHandleMesh, DaggerHandle, 0.80f, 0.0f);
	ApplyColor(DaggerGuardMesh,  DaggerGuard,  0.25f, 0.70f);

	// AK-47 materials — dark gunmetal body/barrel, wooden stock, dark brown mag
	const FLinearColor AK47Metal(0.20f, 0.20f, 0.22f);
	const FLinearColor AK47Wood(0.30f, 0.20f, 0.08f);
	const FLinearColor AK47MagColor(0.15f, 0.12f, 0.05f);
	ApplyColor(AK47BodyMesh,   AK47Metal,    0.35f, 0.9f);
	ApplyColor(AK47BarrelMesh, AK47Metal,    0.25f, 0.95f);
	ApplyColor(AK47MagMesh,    AK47MagColor, 0.70f, 0.3f);
	ApplyColor(AK47StockMesh,  AK47Wood,     0.80f, 0.0f);
	// Muzzle flash: bright emissive (hidden when not firing)
	if (AK47MuzzleMesh && AK47MuzzleMesh->GetStaticMesh() && ColorMat)
	{
		UMaterialInstanceDynamic* AKMuzMID = UMaterialInstanceDynamic::Create(ColorMat, AK47MuzzleMesh);
		if (AKMuzMID)
		{
			AKMuzMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.f, 0.6f, 0.1f));
			AKMuzMID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(10.f, 6.f, 2.f));
			AKMuzMID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			AKMuzMID->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
			AK47MuzzleMesh->SetMaterial(0, AKMuzMID);
		}
	}

	// Mark as grounded immediately — ground plane has no collision.
	bHasBeenGrounded = true;

	// ---- 5% taller ----
	SetActorScale3D(FVector(1.05f));

	// ---- Cyan underglow (visible to other players, helps spot each other) ----
	{
		UPointLightComponent* GlowLight = NewObject<UPointLightComponent>(this, TEXT("CyanUnderglow"));
		GlowLight->SetupAttachment(RootComponent);
		GlowLight->SetRelativeLocation(FVector(0.f, 0.f, -85.f));
		GlowLight->SetLightColor(FLinearColor(0.f, 1.f, 1.f));
		GlowLight->SetIntensity(8000.f);
		GlowLight->SetAttenuationRadius(600.f);
		GlowLight->SetCastShadows(false);
		GlowLight->MaxDrawDistance = 15000.f;
		GlowLight->RegisterComponent();
	}

	// ---- Floating player name above head ----
	{
		NameTextComp = NewObject<UTextRenderComponent>(this, TEXT("PlayerNameText"));
		NameTextComp->SetupAttachment(RootComponent);
		NameTextComp->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
		NameTextComp->SetHorizontalAlignment(EHTA_Center);
		NameTextComp->SetVerticalAlignment(EVRTA_TextBottom);
		NameTextComp->SetWorldSize(18.f);
		NameTextComp->SetTextRenderColor(FColor::White);
		NameTextComp->SetText(FText::GetEmpty());
		NameTextComp->RegisterComponent();
		bNameTextSet = false;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityCharacter::BeginPlay -- Pawn at %s"),
		*GetActorLocation().ToString());
}

// ---------------------------------------------------------------------------
// Tick — movement, ground trace, animation
// ---------------------------------------------------------------------------

void APHCCityCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// If attached to a vehicle as a visible rider (dirt bike), skip all
	// movement and ground-following — the vehicle handles positioning.
	if (GetAttachParentActor() != nullptr)
		return;

	// ---- Floating name: set from PlayerState profile once available ------
	if (!bNameTextSet && NameTextComp)
	{
		if (APHCPlayerState* PS = GetPlayerState<APHCPlayerState>())
		{
			const FPHCPlayerProfile& Prof = PS->GetPlayerProfile();
			if (Prof.PlayerId > 0 || Prof.bIsGuest)
			{
				FString Name = Prof.bIsGuest ? TEXT("Guest") : Prof.Username;
				if (Name.IsEmpty()) Name = TEXT("Guest");
				NameTextComp->SetText(FText::FromString(Name));
				bNameTextSet = true;
			}
		}
	}

	// ---- ADS interpolation + FOV (COD-style) ----------------------------
	if (bFirstPersonMode)
	{
		const float ADSTarget = bADS ? 1.f : 0.f;
		ADSAlpha = FMath::FInterpTo(ADSAlpha, ADSTarget, DeltaTime, ADSInterpSpeed);
		if (FollowCamera)
		{
			FollowCamera->SetFieldOfView(FMath::Lerp(ADSFOVHip, ADSFOVAim, ADSAlpha));
		}
	}
	else
	{
		ADSAlpha = 0.f;
		bADS = false;
		if (FollowCamera) FollowCamera->SetFieldOfView(90.f);
	}

	// ---- Recoil recovery (COD-style: pull back toward zero) -------------
	if (bFirstPersonMode && GetController())
	{
		if (RecoilPitchAccum > 0.01f)
		{
			const bool bCurrentlyFiring = (bMinigunFiring || bAK47Firing);
			const float RecRate = bCurrentlyFiring ? 2.f : RecoilRecoverySpeed;
			const float PitchRecover = FMath::Min(RecRate * DeltaTime, RecoilPitchAccum);
			AddControllerPitchInput(-PitchRecover);
			RecoilPitchAccum -= PitchRecover;
		}
		else { RecoilPitchAccum = 0.f; }

		if (FMath::Abs(RecoilYawAccum) > 0.01f)
		{
			const float YawRecover = FMath::Sign(RecoilYawAccum) * FMath::Min(3.f * DeltaTime, FMath::Abs(RecoilYawAccum));
			AddControllerYawInput(-YawRecover);
			RecoilYawAccum -= YawRecover;
		}
		else { RecoilYawAccum = 0.f; }
	}

	// ---- Camera shake (RPG fire recoil, GTA V style) --------------------
	if (CameraShakeTimer > 0.f && CameraBoom)
	{
		CameraShakeTimer -= DeltaTime;
		const FVector BaseOffset = bFirstPersonMode
			? FVector(0.f, 0.f, 76.f)
			: FVector(0.f, 75.f, 60.f);
		const float Decay = FMath::Max(CameraShakeTimer / 0.35f, 0.f);
		const float ShakeX = FMath::RandRange(-CameraShakeIntensity, CameraShakeIntensity) * Decay;
		const float ShakeZ = FMath::RandRange(-CameraShakeIntensity, CameraShakeIntensity) * Decay;
		CameraBoom->SocketOffset = BaseOffset + FVector(0.f, ShakeX, ShakeZ);

		if (CameraShakeTimer <= 0.f)
		{
			CameraBoom->SocketOffset = BaseOffset;
		}
	}

	// ---- Per-shot micro-shake for automatic weapons ---------------------
	if (bFirstPersonMode && ShotShakeTimer > 0.f)
	{
		ShotShakeTimer -= DeltaTime;
		if (GetController())
		{
			const float ShakeMag = 0.05f * FMath::Min(ShotShakeTimer / 0.03f, 1.f);
			AddControllerPitchInput(FMath::RandRange(-ShakeMag, ShakeMag) * 0.3f);
			AddControllerYawInput(FMath::RandRange(-ShakeMag, ShakeMag) * 0.15f);
		}
	}

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

	const float Speed = bJetpackEquipped ? 3576.f : (bIsSprinting ? SprintSpeed : WalkSpeed);

	// ---- Jetpack hover mode ---------------------------------------------
	if (bJetpackEquipped)
	{
		const float JetpackSpeed = 3576.f; // 80 mph in UU/s
		const float JetpackClimbSpeed = 1800.f; // vertical speed UU/s

		// Mobile: use look-Y (right thumb vertical swipe) for altitude
		float VertInput = JetpackVerticalInput;
		if (bHUDTouchActive && FMath::Abs(TouchLookY) > 0.01f)
		{
			VertInput = FMath::Clamp(-TouchLookY * 3.f, -1.f, 1.f);
		}

		// Horizontal (sweep to avoid flying through buildings)
		if (!MoveDir.IsNearlyZero())
		{
			const FVector Delta = MoveDir * JetpackSpeed * DeltaTime;
			AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
		}

		// Vertical — free flight up/down, floor clamp at 58 UU
		FVector Pos = GetActorLocation();
		float NewZ = Pos.Z + VertInput * JetpackClimbSpeed * DeltaTime;
		if (NewZ < 58.f) NewZ = 58.f; // don't go below ground
		SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));

		// Animate exhaust cone flicker under boots
		if (JetpackFlameL)
		{
			float Flicker = 0.9f + FMath::Sin(GetWorld()->GetTimeSeconds() * 20.f) * 0.1f;
			JetpackFlameL->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.08f * Flicker));
		}
		if (JetpackFlameR)
		{
			float Flicker = 0.9f + FMath::Cos(GetWorld()->GetTimeSeconds() * 22.f) * 0.1f;
			JetpackFlameR->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.08f * Flicker));
		}

		// don't return — weapon/animation code runs below
	}
	else
	{

	// ---- Horizontal movement (with wall collision line trace) ----------------
	if (!MoveDir.IsNearlyZero())
	{
		const FVector Delta = MoveDir * Speed * DeltaTime;
		const FVector OldPos = GetActorLocation();
		const float MoveLen = FVector(Delta.X, Delta.Y, 0.f).Size();
		const FVector MoveNorm = FVector(Delta.X, Delta.Y, 0.f).GetSafeNormal();

		// Thin line trace at chest height (above terrain/road surface) to detect building walls
		const FVector TraceStart(OldPos.X, OldPos.Y, OldPos.Z + 80.f);
		const FVector TraceEnd = TraceStart + MoveNorm * (MoveLen + 50.f);
		FCollisionQueryParams WallParams;
		WallParams.AddIgnoredActor(this);
		FHitResult WallHit;

		bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			WallHit, TraceStart, TraceEnd, ECC_WorldStatic, WallParams);

		if (bBlocked && !WallHit.bStartPenetrating && WallHit.Distance < MoveLen + 30.f)
		{
			// Wall ahead — move only up to 30 UU before the wall
			const float SafeDist = FMath::Max(0.f, WallHit.Distance - 30.f);
			if (SafeDist > 1.f)
				AddActorWorldOffset(MoveNorm * FMath::Min(SafeDist, MoveLen), false);
			// Slide along the wall
			const FVector Remainder = MoveNorm * (MoveLen - FMath::Min(SafeDist, MoveLen));
			const FVector SlideDir = FVector::VectorPlaneProject(Remainder, WallHit.Normal);
			if (SlideDir.Size2D() > 1.f)
				AddActorWorldOffset(FVector(SlideDir.X, SlideDir.Y, 0.f), false);
		}
		else
		{
			// No wall — move freely
			AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
		}
	}

	// ---- Ground following -----------------------------------------------
	{
		const FVector Pos = GetActorLocation();

		// --- Underground tunnel zone override ---
		// Tunnel: X from 102000 to 116000, Y from 28000 to 32000
		// Slope: Z=60 at X=116000, Z=-8000 at X=102000
		// Airport floor: X from 100000 to 200000, Y from -10000 to 70000, Z=-8000
		const bool bInTunnel = (Pos.X >= 102000.f && Pos.X <= 116000.f &&
			Pos.Y >= 28000.f && Pos.Y <= 32000.f);
		const bool bInAirport = (Pos.X >= 100000.f && Pos.X <= 200000.f &&
			Pos.Y >= -10000.f && Pos.Y <= 70000.f && Pos.Z < 60.f);

		// Character pivot is ~58 UU above their feet (floor clamp on surface = 58)
		const float FootOffset = 58.f;

		if (bInTunnel || bInAirport)
		{
			float TargetZ;
			if (bInTunnel && Pos.X >= 102000.f && Pos.X <= 116000.f)
			{
				// Linear slope: X=116000 → Z=60,  X=102000 → Z=-8000+FootOffset
				const float Alpha = (Pos.X - 102000.f) / (116000.f - 102000.f);
				TargetZ = FMath::Lerp(-8000.f + FootOffset, 60.f, Alpha);
				if (Pos.Z < TargetZ - 200.f) TargetZ = -8000.f + FootOffset;
			}
			else
			{
				TargetZ = -8000.f + FootOffset; // airport floor
			}
			bHasBeenGrounded = true;
			bWasGrounded = true;
			ManualVelocityZ = 0.f;
			const float NewZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, 12.f);
			SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
		}
		else
		{
			// Normal ground trace for above-ground areas
			const FVector TraceStart(Pos.X, Pos.Y, Pos.Z + 5000.f);
			const FVector TraceEnd(Pos.X, Pos.Y, Pos.Z - 2000.f);
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(this);

			TArray<FHitResult> Hits;
			bool bFoundSurface = false;
			if (GetWorld()->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic, Params))
			{
				// Find the highest surface at or near our feet
				float BestZ = -1e9f;
				for (const FHitResult& Hit : Hits)
				{
					if (Hit.bStartPenetrating) continue;
					const float SurfZ = Hit.ImpactPoint.Z + FootOffset;
					// Accept surfaces up to 200 UU above us (terrain slopes) and any below
					if (SurfZ > BestZ && SurfZ < Pos.Z + 200.f)
					{
						BestZ = SurfZ;
						bFoundSurface = true;
					}
				}
				// Terrain height supplement (guaranteed terrain following via math)
				if (UPHCCitySubsystem* CitySub = GetWorld()->GetSubsystem<UPHCCitySubsystem>())
				{
					const float TerrainZ = CitySub->GetTerrainHeight(Pos.X, Pos.Y) + FootOffset;
					if (TerrainZ > BestZ && TerrainZ < Pos.Z + 200.f)
					{
						BestZ = TerrainZ;
						bFoundSurface = true;
					}
				}
				if (bFoundSurface)
				{
					bHasBeenGrounded = true;
					ManualVelocityZ = 0.f;
					const float NewZ = FMath::FInterpTo(Pos.Z, BestZ, DeltaTime, 12.f);
					SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
				}
			}
			if (!bFoundSurface && bHasBeenGrounded)
			{
				if (bWasGrounded) ManualVelocityZ = 0.f;
				ManualVelocityZ -= 980.f * DeltaTime;
				float NewZ = Pos.Z + ManualVelocityZ * DeltaTime;
				if (NewZ <= FootOffset) { NewZ = FootOffset; ManualVelocityZ = 0.f; }
				SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
			}
			bWasGrounded = bFoundSurface;
		}
	}

	} // end else (not jetpack)

	// ---- Orient body to movement direction ------------------------------
	if (!MoveDir.IsNearlyZero())
	{
		const FRotator DesiredRot = MoveDir.Rotation();
		const FRotator CurrentRot = GetActorRotation();
		const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, 10.f);
		SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
	}

	// =====================================================================
	//  Walk Animation — natural human gait with spring-damped physics
	//  Features: harmonic sine, torso twist, hip sway, head stabilization,
	//  momentum lean, idle animation, hand/hair spring lag, breathing
	// =====================================================================
	{
		const bool bMoving = !MoveDir.IsNearlyZero() && !bJetpackEquipped;
		const float TargetBlend = bMoving ? 1.f : 0.f;
		WalkAnimBlend = bJetpackEquipped ? 0.f : FMath::FInterpTo(WalkAnimBlend, TargetBlend, DeltaTime, 8.f);

		if (bMoving)
		{
			const float CycleSpeed = bIsSprinting ? 16.f : 13.f;
			WalkCyclePhase += CycleSpeed * DeltaTime;
			IdleTimer = 0.f;
		}
		else if (!bJetpackEquipped)
		{
			IdleTimer += DeltaTime;
		}
		else
		{
			IdleTimer = 0.f;
		}

		// Organic sine — adds harmonics for natural, non-robotic gait
		auto OrgSin = [](float P) -> float
		{
			return FMath::Sin(P) + 0.12f * FMath::Sin(2.f * P) + 0.05f * FMath::Sin(3.f * P - 0.4f);
		};

		const float ThighAmp    = bIsSprinting ? 45.f : 35.f;
		const float UpperArmAmp = bIsSprinting ? 40.f : 30.f;
		const float KneeBendMax = bIsSprinting ? 55.f : 40.f;
		const float ElbowBendMax= bIsSprinting ? 50.f : 35.f;
		const float LeanAmp     = bIsSprinting ? 10.f :  6.f;
		const float BobAmp      = bIsSprinting ?  3.f :  2.f;

		const float Phase = WalkCyclePhase;
		const float Blend = WalkAnimBlend;

		// Primary swing — organic contralateral gait
		const float OrgPhase = OrgSin(Phase);
		const float LThighSwing  =  OrgPhase * ThighAmp * Blend;
		const float RThighSwing  = -LThighSwing;
		const float RArmSwing    =  OrgPhase * UpperArmAmp * Blend;
		const float LArmSwing    = -RArmSwing;

		// Secondary bend (knees on back-swing, elbows flex)
		const float LKneeBend  = FMath::Max(0.f, -LThighSwing / (ThighAmp + 0.01f)) * KneeBendMax * Blend;
		const float RKneeBend  = FMath::Max(0.f, -RThighSwing / (ThighAmp + 0.01f)) * KneeBendMax * Blend;
		const float LElbowBend = (0.3f + 0.7f * FMath::Abs(LArmSwing / (UpperArmAmp + 0.01f))) * ElbowBendMax * Blend;
		const float RElbowBend = (0.3f + 0.7f * FMath::Abs(RArmSwing / (UpperArmAmp + 0.01f))) * ElbowBendMax * Blend;

		// --- Natural torso dynamics ---
		const float TorsoLean  = LeanAmp * Blend;
		const float VertBounce = FMath::Cos(Phase * 2.f) * BobAmp * Blend;

		// Shoulder/hip counter-rotation (twist) — the key to natural walk
		const float TwistAmp = bIsSprinting ? 8.f : 5.f;
		const float PelvisYaw =  OrgPhase * TwistAmp * 0.4f * Blend;
		const float ChestYaw  = -OrgPhase * TwistAmp * Blend;

		// Lateral hip sway (weight shift side to side)
		const float SwayAmp = bIsSprinting ? 2.5f : 1.5f;
		const float HipSway = FMath::Sin(Phase) * SwayAmp * Blend;

		// ---- Momentum lean (acceleration-based torso tilt) ----
		const FVector WorldVel = MoveDir * Speed;
		const FVector Accel = (WorldVel - PrevFrameWorldVel) / FMath::Max(DeltaTime, 0.001f);
		PrevFrameWorldVel = WorldVel;

		// Project acceleration into actor-local forward/right for pitch/roll lean
		const FRotator ActorYaw(0.f, GetActorRotation().Yaw, 0.f);
		const FVector LocalAccel = ActorYaw.UnrotateVector(Accel);
		const float MomentumPitch = LeanPitchSpring.Update(
			FMath::Clamp(-LocalAccel.X * 0.003f, -12.f, 12.f), 80.f, 12.f, DeltaTime);
		const float MomentumRoll  = LeanRollSpring.Update(
			FMath::Clamp(LocalAccel.Y * 0.002f, -8.f, 8.f), 80.f, 12.f, DeltaTime);

		// ---- Idle animation (visible when standing still) ----
		const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const float BreathScale = FMath::Sin(WorldTime * 2.5f) * 0.008f * (1.f - Blend);
		const float IdleFade = FMath::Clamp(IdleTimer - 0.5f, 0.f, 1.f); // ramp in after 0.5s

		// Weight shift: slow lateral sway when idle
		const float IdleWeightShift = FMath::Sin(WorldTime * 0.8f) * 1.5f * IdleFade;
		// Micro head nod: subtle look-around
		const float IdleHeadYaw   = FMath::Sin(WorldTime * 0.6f + 1.2f) * 3.f * IdleFade;
		const float IdleHeadPitch = FMath::Sin(WorldTime * 0.45f) * 1.5f * IdleFade;

		// ---- Pivot constants (match constructor rest positions) ----
		const float ShPivZ    = 54.f;
		const float HipPivZ   =  2.f;
		const float UArmHalf  = 10.f;
		const float FArmHalf  =  9.f;
		const float ThighHalf = 14.f;
		const float ShinHalf  = 12.f;

		// ---- AnimateArm: shoulder → upper arm → elbow → forearm → hand ----
		auto AnimateArm = [&](
			UStaticMeshComponent* UA, UStaticMeshComponent* FA,
			UStaticMeshComponent* EJ, UStaticMeshComponent* HN,
			UStaticMeshComponent* SH,
			float Y, float Swing, float Bend)
		{
			const float Sway = HipSway * 0.3f;
			const FRotator URot(Swing, 0.f, 0.f);
			const FVector UOff = URot.RotateVector(FVector(0.f, 0.f, -UArmHalf));
			if (UA) { UA->SetRelativeLocation(FVector(UOff.X, Y + Sway, ShPivZ + UOff.Z + VertBounce)); UA->SetRelativeRotation(FRotator(Swing, 0.f, 0.f)); }

			const FVector ELocal = URot.RotateVector(FVector(0.f, 0.f, -UArmHalf * 2.f));
			const FVector EPos(ELocal.X, Y + Sway, ShPivZ + ELocal.Z + VertBounce);
			if (EJ) { EJ->SetRelativeLocation(EPos); }

			const float FAngle = Swing - Bend;
			const FRotator FRot(FAngle, 0.f, 0.f);
			const FVector FOff = FRot.RotateVector(FVector(0.f, 0.f, -FArmHalf));
			if (FA) { FA->SetRelativeLocation(FVector(EPos.X + FOff.X, Y + Sway, EPos.Z + FOff.Z)); FA->SetRelativeRotation(FRotator(FAngle, 0.f, 0.f)); }

			if (HN)
			{
				const FVector HLocal = FRot.RotateVector(FVector(0.f, 0.f, -(FArmHalf * 2.f + 3.f)));
				HN->SetRelativeLocation(FVector(EPos.X + HLocal.X, Y + Sway, EPos.Z + HLocal.Z));
			}

			if (SH) { SH->SetRelativeLocation(FVector(UOff.X * 0.2f, Y + (Y > 0 ? -1.f : 1.f) + Sway, ShPivZ + VertBounce)); }
		};

		// ---- AnimateLeg: thigh → knee → shin → foot → toe → sole ----
		auto AnimateLeg = [&](
			UStaticMeshComponent* TH, UStaticMeshComponent* SN,
			UStaticMeshComponent* KN, UStaticMeshComponent* FT,
			UStaticMeshComponent* TOE, UStaticMeshComponent* SL,
			float Y, float Swing, float Bend)
		{
			const FRotator TRot(Swing, 0.f, 0.f);
			const FVector TOff = TRot.RotateVector(FVector(0.f, 0.f, -ThighHalf));
			if (TH) { TH->SetRelativeLocation(FVector(TOff.X, Y + HipSway, HipPivZ + TOff.Z)); TH->SetRelativeRotation(FRotator(Swing, 0.f, 0.f)); }

			const FVector KLocal = TRot.RotateVector(FVector(0.f, 0.f, -ThighHalf * 2.f));
			const FVector KPos(KLocal.X, Y + HipSway, HipPivZ + KLocal.Z);
			if (KN) { KN->SetRelativeLocation(KPos); }

			const float SAngle = Swing + Bend;
			const FRotator SRot(SAngle, 0.f, 0.f);
			const FVector SOff = SRot.RotateVector(FVector(0.f, 0.f, -ShinHalf));
			if (SN) { SN->SetRelativeLocation(FVector(KPos.X + SOff.X, Y + HipSway, KPos.Z + SOff.Z)); SN->SetRelativeRotation(FRotator(SAngle, 0.f, 0.f)); }

			const FVector FLocal = SRot.RotateVector(FVector(0.f, 0.f, -(ShinHalf * 2.f + 4.f)));
			float FZ = FMath::Max(KPos.Z + FLocal.Z, -73.f);
			float FX = KPos.X + FLocal.X + 4.f;
			if (FT)  { FT->SetRelativeLocation(FVector(FX, Y + HipSway, FZ)); FT->SetRelativeRotation(FRotator::ZeroRotator); }
			if (TOE) { TOE->SetRelativeLocation(FVector(FX + 10.f, Y + HipSway, FZ + 1.f)); }
			if (SL)  { SL->SetRelativeLocation(FVector(FX, Y + HipSway, FZ - 5.f)); SL->SetRelativeRotation(FRotator::ZeroRotator); }
		};

		// Arms — gun-holding pose overrides normal walk swing
		if (CurrentWeaponType == 0)
		{
			// ---- Minigun hold pose: both arms forward, gripping the gun ----
			const float GunBob = VertBounce * 0.5f;
			const float GunSway = HipSway * 0.3f;

			// Right arm: upper arm pitched forward ~70 deg, forearm forward
			const float RAimPitch = 70.f;
			auto PoseArm = [&](
				UStaticMeshComponent* UA, UStaticMeshComponent* FA,
				UStaticMeshComponent* EJ, UStaticMeshComponent* HN,
				UStaticMeshComponent* SH,
				float Y, float Pitch, float EBend)
			{
				const FRotator URot(Pitch, 0.f, 0.f);
				const FVector UOff = URot.RotateVector(FVector(0.f, 0.f, -UArmHalf));
				if (UA) { UA->SetRelativeLocation(FVector(UOff.X, Y + GunSway, ShPivZ + UOff.Z + GunBob)); UA->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f)); }

				const FVector ELocal = URot.RotateVector(FVector(0.f, 0.f, -UArmHalf * 2.f));
				const FVector EPos(ELocal.X, Y + GunSway, ShPivZ + ELocal.Z + GunBob);
				if (EJ) { EJ->SetRelativeLocation(EPos); }

				const float FAngle = Pitch - EBend;
				const FRotator FRot(FAngle, 0.f, 0.f);
				const FVector FOff = FRot.RotateVector(FVector(0.f, 0.f, -FArmHalf));
				if (FA) { FA->SetRelativeLocation(FVector(EPos.X + FOff.X, Y + GunSway, EPos.Z + FOff.Z)); FA->SetRelativeRotation(FRotator(FAngle, 0.f, 0.f)); }

				if (HN)
				{
					const FVector HLocal = FRot.RotateVector(FVector(0.f, 0.f, -(FArmHalf * 2.f + 3.f)));
					HN->SetRelativeLocation(FVector(EPos.X + HLocal.X, Y + GunSway, EPos.Z + HLocal.Z));
				}
				if (SH) { SH->SetRelativeLocation(FVector(UOff.X * 0.2f, Y + (Y > 0 ? -1.f : 1.f) + GunSway, ShPivZ + GunBob)); }
			};

			// Right arm: main grip — forward 70 deg, slight elbow bend
			PoseArm(RightUpperArmMesh, RightForearmMesh, RightElbowMesh, RightHandMesh,
				RightShoulderMesh, 26.f, RAimPitch, 15.f);
			// Left arm: support grip — forward 75 deg, more elbow bend to reach under barrel
			PoseArm(LeftUpperArmMesh, LeftForearmMesh, LeftElbowMesh, LeftHandMesh,
				LeftShoulderMesh, -26.f, 75.f, 25.f);
		}
		else
		{
			// Normal walk swing
			AnimateArm(LeftUpperArmMesh, LeftForearmMesh, LeftElbowMesh, LeftHandMesh,
				LeftShoulderMesh, -26.f, LArmSwing, LElbowBend);
			AnimateArm(RightUpperArmMesh, RightForearmMesh, RightElbowMesh, RightHandMesh,
				RightShoulderMesh, 26.f, RArmSwing, RElbowBend);
		}

		// ---- Hand spring-damper lag (follow-through / overshoot) ----
		if (LeftHandMesh)
		{
			FVector LHP = LeftHandMesh->GetRelativeLocation();
			LHP.X += LHandLagX.Update(0.f, 40.f, 8.f, DeltaTime);
			LHP.Z += LHandLagZ.Update(0.f, 40.f, 8.f, DeltaTime);
			LeftHandMesh->SetRelativeLocation(LHP);
		}
		if (RightHandMesh)
		{
			FVector RHP = RightHandMesh->GetRelativeLocation();
			RHP.X += RHandLagX.Update(0.f, 40.f, 8.f, DeltaTime);
			RHP.Z += RHandLagZ.Update(0.f, 40.f, 8.f, DeltaTime);
			RightHandMesh->SetRelativeLocation(RHP);
		}
		// Feed hand springs with velocity change (drives overshoot)
		LHandLagX.Vel += -Accel.X * 0.0004f;
		LHandLagZ.Vel += -Accel.Z * 0.0004f;
		RHandLagX.Vel += -Accel.X * 0.0004f;
		RHandLagZ.Vel += -Accel.Z * 0.0004f;

		// Legs
		AnimateLeg(LeftThighMesh, LeftShinMesh, LeftKneeMesh, LeftFootMesh,
			LeftToeMesh, LeftSoleMesh, -11.f, LThighSwing, LKneeBend);
		AnimateLeg(RightThighMesh, RightShinMesh, RightKneeMesh, RightFootMesh,
			RightToeMesh, RightSoleMesh, 11.f, RThighSwing, RKneeBend);

		// ---- Torso: lean + momentum + bounce + twist + sway + breathing ----
		if (ChestMesh)
		{
			ChestMesh->SetRelativeLocation(FVector(1.f, HipSway * 0.7f, 44.f + VertBounce));
			ChestMesh->SetRelativeRotation(FRotator(
				TorsoLean + MomentumPitch, ChestYaw, MomentumRoll));
			ChestMesh->SetRelativeScale3D(FVector(0.22f, 0.44f, 0.26f + BreathScale));
		}
		if (AbsMesh)
		{
			AbsMesh->SetRelativeLocation(FVector(0.f, HipSway * 0.5f, 28.f + VertBounce * 0.8f));
			AbsMesh->SetRelativeRotation(FRotator(
				TorsoLean * 0.6f + MomentumPitch * 0.5f,
				(ChestYaw + PelvisYaw) * 0.5f,
				MomentumRoll * 0.5f));
			AbsMesh->SetRelativeScale3D(FVector(0.16f, 0.32f, 0.18f + BreathScale * 0.5f));
		}
		if (PelvisMesh)
		{
			PelvisMesh->SetRelativeLocation(FVector(0.f,
				HipSway + IdleWeightShift, 12.f + VertBounce * 0.4f));
			PelvisMesh->SetRelativeRotation(FRotator(0.f, PelvisYaw, 0.f));
		}

		// Clothing follows torso
		if (CollarMesh) CollarMesh->SetRelativeLocation(FVector(2.f, HipSway * 0.7f, 56.f + VertBounce));
		if (BeltMesh)   BeltMesh->SetRelativeLocation(FVector(0.f, HipSway + IdleWeightShift * 0.5f, 20.f + VertBounce * 0.5f));
		if (BeltBuckleMesh) BeltBuckleMesh->SetRelativeLocation(FVector(8.f, HipSway + IdleWeightShift * 0.5f, 20.f + VertBounce * 0.5f));

		// Neck
		if (NeckMesh) NeckMesh->SetRelativeLocation(FVector(0.f, HipSway * 0.5f, 60.f + VertBounce * 0.7f));

		// ---- Head: stabilized + momentum + idle micro-movement ----
		const float HeadYaw = -ChestYaw * 0.3f + IdleHeadYaw;
		const FVector HeadPos(0.f, HipSway * 0.2f + IdleWeightShift * 0.3f,
			76.f + VertBounce * 0.3f);
		const FRotator HeadRot(
			-TorsoLean * 0.25f - MomentumPitch * 0.15f + IdleHeadPitch,
			HeadYaw,
			-MomentumRoll * 0.1f);

		if (HeadMesh) { HeadMesh->SetRelativeLocation(HeadPos); HeadMesh->SetRelativeRotation(HeadRot); }
		if (JawMesh)  { JawMesh->SetRelativeLocation(HeadPos + HeadRot.RotateVector(FVector(2.f, 0.f, -9.f))); JawMesh->SetRelativeRotation(HeadRot); }

		// Face features follow head
		auto FaceFollow = [&](UStaticMeshComponent* M, const FVector& Offset)
		{
			if (M) { M->SetRelativeLocation(HeadPos + HeadRot.RotateVector(Offset)); M->SetRelativeRotation(HeadRot); }
		};

		FaceFollow(LeftEyeWhiteMesh,  FVector(9.f, -5.f, 2.f));
		FaceFollow(RightEyeWhiteMesh, FVector(9.f,  5.f, 2.f));
		FaceFollow(LeftPupilMesh,     FVector(11.f, -5.f, 2.f));
		FaceFollow(RightPupilMesh,    FVector(11.f,  5.f, 2.f));
		FaceFollow(NoseMesh,          FVector(12.f,  0.f, -3.f));
		FaceFollow(MouthMesh,         FVector(10.f,  0.f, -7.f));
		FaceFollow(LeftEarMesh,       FVector(-1.f, -14.f, -1.f));
		FaceFollow(RightEarMesh,      FVector(-1.f,  14.f, -1.f));
		FaceFollow(LeftBrowMesh,      FVector(10.f, -5.f, 6.f));
		FaceFollow(RightBrowMesh,     FVector(10.f,  5.f, 6.f));

		// Hair follows head + spring bounce for secondary motion
		const float HairBounce = HairLagZ.Update(0.f, 50.f, 7.f, DeltaTime);
		HairLagZ.Vel += (-Accel.Z * 0.0005f + VertBounce * 0.8f);

		auto HairFollow = [&](UStaticMeshComponent* M, const FVector& Offset)
		{
			if (M)
			{
				FVector P = HeadPos + HeadRot.RotateVector(Offset);
				P.Z += HairBounce;
				M->SetRelativeLocation(P);
			}
		};

		HairFollow(HairTopMesh,   FVector(-1.f, 0.f, 12.f));
		HairFollow(HairBackMesh,  FVector(-8.f, 0.f, 4.f));
		HairFollow(HairSideLMesh, FVector(-2.f, -13.f, 4.f));
		HairFollow(HairSideRMesh, FVector(-2.f,  13.f, 4.f));

		// Camera-relative FP grip helper: keeps weapon in view at any pitch
		const float FPCamPitch = (bFirstPersonMode && GetController())
			? GetController()->GetControlRotation().Pitch
			: 0.f;
		const FVector FPCamOrigin(0.f, 0.f, 76.f); // matches CameraBoom SocketOffset
		const FRotator FPPitchRot(-FPCamPitch, 0.f, 0.f);

		// ---- FP Weapon Bob (figure-8 using WalkCyclePhase) ----
		FVector FPWeaponOffset = FVector::ZeroVector;
		if (bFirstPersonMode)
		{
			const float BobScale = FMath::Lerp(1.f, 0.15f, ADSAlpha);
			WeaponBobH = FMath::Sin(WalkCyclePhase) * 1.2f * WalkAnimBlend * BobScale;
			WeaponBobV = FMath::Sin(2.f * WalkCyclePhase) * 0.8f * WalkAnimBlend * BobScale;

			// ---- Weapon Sway (spring-based viewmodel lag) ----
			const float CurYaw = GetController() ? GetController()->GetControlRotation().Yaw : 0.f;
			const float CurPitch = GetController() ? GetController()->GetControlRotation().Pitch : 0.f;
			float YawDelta = CurYaw - PrevAimYaw;
			if (YawDelta > 180.f) YawDelta -= 360.f;
			if (YawDelta < -180.f) YawDelta += 360.f;
			float PitchDelta = CurPitch - PrevAimPitch;
			if (PitchDelta > 180.f) PitchDelta -= 360.f;
			if (PitchDelta < -180.f) PitchDelta += 360.f;
			PrevAimYaw = CurYaw;
			PrevAimPitch = CurPitch;

			WeaponSwayX.Vel += -YawDelta * 0.6f;
			WeaponSwayY.Vel += -PitchDelta * 0.4f;
			const float SwayX = WeaponSwayX.Update(0.f, 60.f, 10.f, DeltaTime);
			const float SwayZ = WeaponSwayY.Update(0.f, 60.f, 10.f, DeltaTime);
			const float SwayScale = FMath::Lerp(1.f, 0.2f, ADSAlpha);

			// ---- Weapon Kick (per-shot impulse, spring recovery) ----
			const float KickZ = WeaponKickZ.Update(0.f, 80.f, 12.f, DeltaTime);
			const float KickX = WeaponKickX.Update(0.f, 80.f, 12.f, DeltaTime);

			FPWeaponOffset = FVector(
				KickX + SwayX * SwayScale * 0.5f,
				WeaponBobH + SwayX * SwayScale,
				WeaponBobV + SwayZ * SwayScale + KickZ
			);
		}
		// Usage: GripPt = FPCamOrigin + FPPitchRot.RotateVector(CamLocalOffset)

		// ---- Dagger: held in right hand, stab animation ----
		if (DaggerHandleMesh && DaggerBladeMesh && DaggerGuardMesh)
		{
			const bool bShowDagger = (CurrentWeaponType == 4);
			DaggerHandleMesh->SetVisibility(bShowDagger);
			DaggerBladeMesh->SetVisibility(bShowDagger);
			DaggerGuardMesh->SetVisibility(bShowDagger);

			if (bShowDagger)
			{
				const FVector RHand = bFirstPersonMode
					? FPCamOrigin + FPPitchRot.RotateVector(FVector(25.f, 12.f, -18.f) + FPWeaponOffset)
					: (RightHandMesh
						? RightHandMesh->GetRelativeLocation()
						: FVector(0.f, 26.f, 6.f));

				// Hold angle: blade points upward by default
				float HoldPitch = -15.f;

				// Stab thrust animation
				if (DaggerStabTimer > 0.f)
				{
					DaggerStabTimer -= DeltaTime;
					const float MaxStabTime = 0.25f;
					float Progress = 1.f - FMath::Max(DaggerStabTimer, 0.f) / MaxStabTime;
					Progress = FMath::InterpEaseOut(0.f, 1.f, Progress, 3.f);
					// Forward thrust: pitch rotates blade forward (reversed)
					HoldPitch = FMath::Lerp(-15.f, -140.f, Progress);
				}

				const FRotator HRot(HoldPitch, 0.f, 0.f);

				// Handle below hand
				const FVector HandleOff = HRot.RotateVector(FVector(0.f, 0.f, 7.f));
				DaggerHandleMesh->SetRelativeLocation(RHand + HandleOff);
				DaggerHandleMesh->SetRelativeRotation(HRot);

				// Blade above hand
				const FVector BladeOff = HRot.RotateVector(FVector(0.f, 0.f, 22.f));
				DaggerBladeMesh->SetRelativeLocation(RHand + BladeOff);
				DaggerBladeMesh->SetRelativeRotation(HRot);

				// Cross-guard at grip/blade junction
				const FVector GuardOff = HRot.RotateVector(FVector(0.f, 0.f, 14.f));
				DaggerGuardMesh->SetRelativeLocation(RHand + GuardOff);
				DaggerGuardMesh->SetRelativeRotation(HRot);
			}
		}

		// ---- Scythe: follows right hand, swing animation ----
		if (ScytheHandleMesh && ScytheBladeMesh && ScytheBladeEdgeMesh)
		{
			const bool bShowScythe = (CurrentWeaponType == 3);
			ScytheHandleMesh->SetVisibility(bShowScythe);
			ScytheBladeMesh->SetVisibility(bShowScythe);
			ScytheBladeEdgeMesh->SetVisibility(bShowScythe);

			if (bShowScythe)
			{
				const FVector RHand = bFirstPersonMode
					? FPCamOrigin + FPPitchRot.RotateVector(FVector(25.f, 15.f, -21.f) + FPWeaponOffset)
					: (RightHandMesh
						? RightHandMesh->GetRelativeLocation()
						: FVector(0.f, 26.f, 6.f));

				// Hold angle: handle extends upward and slightly back
				float HoldPitch = -15.f;

				// Swing arc when attacking
				if (ScytheSwingTimer > 0.f)
				{
					ScytheSwingTimer -= DeltaTime;
					const float MaxSwingTime = 0.45f;
					float Progress = 1.f - FMath::Max(ScytheSwingTimer, 0.f) / MaxSwingTime;
					Progress = FMath::InterpEaseOut(0.f, 1.f, Progress, 2.f);
					HoldPitch = FMath::Lerp(-15.f, -250.f, Progress);
				}

				const FRotator HRot(HoldPitch, 0.f, 0.f);
				const float HandleHalf = 50.f;

				const FVector HandleOff = HRot.RotateVector(FVector(0.f, 0.f, HandleHalf));
				ScytheHandleMesh->SetRelativeLocation(RHand + HandleOff);
				ScytheHandleMesh->SetRelativeRotation(HRot);

				const FVector BladeOff = HRot.RotateVector(FVector(14.f, 0.f, HandleHalf + 10.f));
				ScytheBladeMesh->SetRelativeLocation(RHand + BladeOff);
				ScytheBladeMesh->SetRelativeRotation(HRot);

				const FVector EdgeOff = HRot.RotateVector(FVector(26.f, 0.f, HandleHalf + 10.f));
				ScytheBladeEdgeMesh->SetRelativeLocation(RHand + EdgeOff);
				ScytheBladeEdgeMesh->SetRelativeRotation(HRot);
			}
		}

		// ---- Minigun: held between both hands, barrel spin, continuous fire ----
		if (MinigunBodyMesh && MinigunBarrelMesh && MinigunMuzzleMesh)
		{
			const bool bShowMinigun = (CurrentWeaponType == 0);
			MinigunBodyMesh->SetVisibility(bShowMinigun);
			MinigunBarrelMesh->SetVisibility(bShowMinigun);
			MinigunMuzzleMesh->SetVisibility(bShowMinigun && bMinigunFiring);

			if (bShowMinigun)
			{
				// Grip point: midpoint between both hands
				const FVector MiniHip(30.f, 5.f, -21.f);
				const FVector MiniADS(22.f, 0.f, -17.f);
				const FVector GripCenter = bFirstPersonMode
					? FPCamOrigin + FPPitchRot.RotateVector(FMath::Lerp(MiniHip, MiniADS, ADSAlpha) + FPWeaponOffset)
					: [&]() {
						const FVector RH = RightHandMesh
							? RightHandMesh->GetRelativeLocation()
							: FVector(20.f, 26.f, 40.f);
						const FVector LH = LeftHandMesh
							? LeftHandMesh->GetRelativeLocation()
							: FVector(22.f, -26.f, 38.f);
						return (RH + LH) * 0.5f;
					}();

				// Camera pitch so the gun aims where the player is looking
				const float MinigunAimPitch = FPCamPitch;
				const FRotator MiniAimRot(-MinigunAimPitch, 0.f, 0.f);

				// Gun points forward (rotated 90 deg so cylinder axis = forward) + aim pitch
				const FRotator GunRot(90.f - MinigunAimPitch, 0.f, 0.f);

				// Body: at grip center, extends forward
				MinigunBodyMesh->SetRelativeLocation(GripCenter + MiniAimRot.RotateVector(FVector(12.f, 0.f, 0.f)));
				MinigunBodyMesh->SetRelativeRotation(GunRot);

				// Barrel cluster: in front of body
				MinigunBarrelMesh->SetRelativeLocation(GripCenter + MiniAimRot.RotateVector(FVector(40.f, 0.f, 0.f)));

				// Spin barrels
				if (bMinigunFiring)
				{
					MinigunSpinAngle += 2400.f * DeltaTime;
				}
				else
				{
					MinigunSpinAngle *= FMath::Max(0.f, 1.f - 2.f * DeltaTime);
				}
				MinigunBarrelMesh->SetRelativeRotation(FRotator(90.f - MinigunAimPitch, MinigunSpinAngle, 0.f));

				// Muzzle flash at barrel tip
				MinigunMuzzleMesh->SetRelativeLocation(GripCenter + MiniAimRot.RotateVector(FVector(70.f, 0.f, 0.f)));
				MinigunMuzzleMesh->SetRelativeRotation(GunRot);

				// Muzzle flash emissive flicker
				if (bMinigunFiring && MinigunMuzzleMesh->GetNumMaterials() > 0)
				{
					UMaterialInstanceDynamic* MuzzleMID = Cast<UMaterialInstanceDynamic>(
						MinigunMuzzleMesh->GetMaterial(0));
					if (MuzzleMID)
					{
						const float Flicker = FMath::RandRange(0.5f, 1.0f);
						MuzzleMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							FLinearColor(8.f * Flicker, 4.f * Flicker, 0.5f * Flicker));
					}
				}

				// Continuous fire: spawn bullets at fire rate
				if (bMinigunFiring)
				{
					MinigunFireTimer -= DeltaTime;
					if (MinigunFireTimer <= 0.f)
					{
						MinigunFireTimer = 0.05f; // 20 rounds/sec (matches Dune Buggy)
						FireWeapon();
					}
				}
				else
				{
					MinigunFireTimer = 0.f;
				}
			}
		}

		// ---- Bazooka: held on right shoulder, fires like tank cannon ----
		{
			const bool bShowBazooka = (CurrentWeaponType == 1);

			auto SetVis = [](UStaticMeshComponent* C, bool V) { if (C) C->SetVisibility(V); };
			SetVis(BazookaTubeMesh, bShowBazooka);
			SetVis(BazookaRearEndMesh, bShowBazooka);
			SetVis(BazookaFrontSightMesh, bShowBazooka);
			SetVis(BazookaRearSightMesh, bShowBazooka);
			SetVis(BazookaGripMesh, bShowBazooka);
			SetVis(BazookaScopeMesh, bShowBazooka);
			SetVis(BazookaMuzzleMesh, bShowBazooka && BazookaMuzzleFlashTimer > 0.f);

			if (bShowBazooka)
			{
				// Shoulder weapon pose: tube on right shoulder pointing forward
				const FVector BazHip(20.f, 12.f, -11.f);
				const FVector BazADSP(15.f, 2.f, -8.f);
				const FVector RShoulder = bFirstPersonMode
					? FPCamOrigin + FPPitchRot.RotateVector(FMath::Lerp(BazHip, BazADSP, ADSAlpha) + FPWeaponOffset)
					: (RightShoulderMesh
						? RightShoulderMesh->GetRelativeLocation()
						: FVector(0.f, 22.f, 46.f));

				// Cylinder axis is Z, so Pitch 90 points it forward (actor X) + aim pitch
				const FRotator BazGunRot(90.f - FPCamPitch, 0.f, 0.f);
				const FRotator BazAimRot(-FPCamPitch, 0.f, 0.f);
				const FVector TubeCenter = RShoulder + BazAimRot.RotateVector(FVector(25.f, 4.f, 0.f));

				if (BazookaTubeMesh)
				{
					BazookaTubeMesh->SetRelativeLocation(TubeCenter);
					BazookaTubeMesh->SetRelativeRotation(BazGunRot);
				}
				if (BazookaRearEndMesh)
				{
					BazookaRearEndMesh->SetRelativeLocation(TubeCenter + BazAimRot.RotateVector(FVector(-60.f, 0.f, 0.f)));
					BazookaRearEndMesh->SetRelativeRotation(BazGunRot);
				}
				if (BazookaFrontSightMesh)
				{
					BazookaFrontSightMesh->SetRelativeLocation(TubeCenter + BazAimRot.RotateVector(FVector(52.f, 0.f, 5.f)));
				}
				if (BazookaRearSightMesh)
				{
					BazookaRearSightMesh->SetRelativeLocation(TubeCenter + BazAimRot.RotateVector(FVector(-20.f, 0.f, 5.f)));
				}
				if (BazookaGripMesh)
				{
					BazookaGripMesh->SetRelativeLocation(TubeCenter + BazAimRot.RotateVector(FVector(0.f, 0.f, -10.f)));
				}
				if (BazookaScopeMesh)
				{
					BazookaScopeMesh->SetRelativeLocation(TubeCenter + BazAimRot.RotateVector(FVector(16.f, 0.f, 7.f)));
					BazookaScopeMesh->SetRelativeRotation(BazGunRot);
				}
				if (BazookaMuzzleMesh)
				{
					BazookaMuzzleMesh->SetRelativeLocation(TubeCenter + BazAimRot.RotateVector(FVector(68.f, 0.f, 0.f)));
				}

				// Timers
				if (BazookaMuzzleFlashTimer > 0.f) BazookaMuzzleFlashTimer -= DeltaTime;
				if (BazookaFireTimer > 0.f) BazookaFireTimer -= DeltaTime;
			}
		}

		// ---- AK-47: held in right hand, hold-to-fire ----
		if (AK47BodyMesh && AK47BarrelMesh && AK47MagMesh && AK47StockMesh && AK47MuzzleMesh)
		{
			const bool bShowAK = (CurrentWeaponType == 5);
			AK47BodyMesh->SetVisibility(bShowAK);
			AK47BarrelMesh->SetVisibility(bShowAK);
			AK47MagMesh->SetVisibility(bShowAK);
			AK47StockMesh->SetVisibility(bShowAK);
			AK47MuzzleMesh->SetVisibility(bShowAK && bAK47Firing);

			if (bShowAK)
			{
				const FVector AKHip(30.f, 8.f, -19.f);
				const FVector AKADSPos(22.f, 0.f, -15.f);
				const FVector GripPt = bFirstPersonMode
					? FPCamOrigin + FPPitchRot.RotateVector(FMath::Lerp(AKHip, AKADSPos, ADSAlpha) + FPWeaponOffset)
					: (RightHandMesh
						? RightHandMesh->GetRelativeLocation()
						: FVector(0.f, 26.f, 6.f));

				// Get camera pitch so the gun aims where the player is looking
				const float AimPitch = FPCamPitch;
				const FRotator AimRot(-AimPitch, 0.f, 0.f); // negate: component-relative pitch

				// Cylinder barrel needs 90 pitch + aim pitch
				const FRotator BarrelRot(90.f - AimPitch, 0.f, 0.f);

				// Body/receiver centered at grip
				AK47BodyMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(10.f, 0.f, 0.f)));
				AK47BodyMesh->SetRelativeRotation(AimRot);

				// Barrel extends forward from body
				AK47BarrelMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(45.f, 0.f, 0.f)));
				AK47BarrelMesh->SetRelativeRotation(BarrelRot);

				// Banana mag underneath body, angled slightly
				AK47MagMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(6.f, 0.f, -12.f)));
				AK47MagMesh->SetRelativeRotation(FRotator(8.f - AimPitch, 0.f, 0.f));

				// Stock extends behind from body
				AK47StockMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(-22.f, 0.f, -1.f)));
				AK47StockMesh->SetRelativeRotation(AimRot);

				// Muzzle flash at barrel tip
				AK47MuzzleMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(72.f, 0.f, 0.f)));

				// Muzzle flash emissive flicker
				if (bAK47Firing && AK47MuzzleMesh->GetNumMaterials() > 0)
				{
					UMaterialInstanceDynamic* MuzzleMID = Cast<UMaterialInstanceDynamic>(
						AK47MuzzleMesh->GetMaterial(0));
					if (MuzzleMID)
					{
						const float Flicker = FMath::RandRange(0.5f, 1.0f);
						MuzzleMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							FLinearColor(8.f * Flicker, 4.f * Flicker, 0.5f * Flicker));
					}
				}

				// Continuous fire: spawn bullets at fire rate (~8 rounds/sec)
				if (bAK47Firing)
				{
					AK47FireTimer -= DeltaTime;
					if (AK47FireTimer <= 0.f)
					{
						AK47FireTimer = 0.12f;
						FireWeapon();
					}
				}
				else
				{
					AK47FireTimer = 0.f;
				}
			}
		}

		// ---- Muzzle flash dynamic light (shared by all ranged weapons) ----
		if (bFirstPersonMode && MuzzleFlashLight)
		{
			if (MuzzleFlashLightTimer > 0.f)
			{
				MuzzleFlashLightTimer -= DeltaTime;
				MuzzleFlashLight->SetVisibility(true);
				MuzzleFlashLight->SetIntensity(8000.f * FMath::Max(MuzzleFlashLightTimer / 0.04f, 0.f));
				// Position at muzzle of current weapon
				if (CurrentWeaponType == 5 && AK47MuzzleMesh)
					MuzzleFlashLight->SetRelativeLocation(AK47MuzzleMesh->GetRelativeLocation());
				else if (CurrentWeaponType == 0 && MinigunMuzzleMesh)
					MuzzleFlashLight->SetRelativeLocation(MinigunMuzzleMesh->GetRelativeLocation());
				else if (CurrentWeaponType == 1 && BazookaMuzzleMesh)
					MuzzleFlashLight->SetRelativeLocation(BazookaMuzzleMesh->GetRelativeLocation());
			}
			else
			{
				MuzzleFlashLight->SetVisibility(false);
				MuzzleFlashLight->SetIntensity(0.f);
			}
		}

		// ---- Hit marker timer decay ----
		if (HitMarkerTimer > 0.f) HitMarkerTimer -= DeltaTime;
	}

	// =====================================================================
	//  PvP damage flash — red emissive on body meshes (visible to all clients)
	// =====================================================================
	if (PvPDamageFlashTimer > 0.f)
	{
		PvPDamageFlashTimer -= DeltaTime;
		const FLinearColor FlashColor = PvPDamageFlashTimer > 0.f
			? FLinearColor(4.f, 0.f, 0.f) : FLinearColor::Black;
		for (UMaterialInstanceDynamic* MID : BodyMIDs)
		{
			if (MID) MID->SetVectorParameterValue(TEXT("EmissiveColor"), FlashColor);
		}
	}

	// =====================================================================
	//  Network: send position/rotation to server so other clients see us move.
	//  Only the autonomous proxy (owning client on a dedicated server) needs
	//  to do this — the server then updates the authoritative actor, which
	//  replicates to all simulated proxies via SetReplicateMovement(true).
	// =====================================================================
	if (IsLocallyControlled() && !HasAuthority())
	{
		NetMoveSendTimer -= DeltaTime;
		if (NetMoveSendTimer <= 0.f)
		{
			NetMoveSendTimer = 1.f / 15.f;  // 15 Hz
			ServerUpdateMovement(GetActorLocation(), GetActorRotation());
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
	const float SensMult = (bFirstPersonMode && ADSAlpha > 0.01f)
		? FMath::Lerp(1.f, ADSSensitivityMult, ADSAlpha) : 1.f;
	AddControllerYawInput(LookVector.X * SensMult);
	AddControllerPitchInput(LookVector.Y * SensMult);
}

void APHCCityCharacter::HandleJumpStarted(const FInputActionValue& /*Value*/)
{
	if (bJetpackEquipped)
	{
		JetpackVerticalInput = 1.f; // ascend
		return;
	}
	if (bHasBeenGrounded)
	{
		ManualVelocityZ = 420.f;
		bHasBeenGrounded = true;
		bWasGrounded = false;
	}
}

void APHCCityCharacter::HandleJumpCompleted(const FInputActionValue& /*Value*/)
{
	if (bJetpackEquipped) JetpackVerticalInput = 0.f;
}

void APHCCityCharacter::HandleSprintStarted(const FInputActionValue& /*Value*/)
{
	if (bJetpackEquipped)
	{
		JetpackVerticalInput = -1.f; // descend
		return;
	}
	bIsSprinting = true;
}

void APHCCityCharacter::HandleSprintCompleted(const FInputActionValue& /*Value*/)
{
	if (bJetpackEquipped) { JetpackVerticalInput = 0.f; return; }
	bIsSprinting = false;
}

void APHCCityCharacter::HandleInteract(const FInputActionValue& /*Value*/)
{
	bPendingVehicleInteract = true;
}

void APHCCityCharacter::HandleADSStarted(const FInputActionValue& /*Value*/)
{
	SetADS(true);
}

void APHCCityCharacter::HandleADSCompleted(const FInputActionValue& /*Value*/)
{
	SetADS(false);
}

void APHCCityCharacter::SetADS(bool bNewADS)
{
	bADS = bNewADS;
}

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------

void APHCCityCharacter::FireWeapon()
{
	if (CurrentWeaponType < 0) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Use controller rotation (includes camera pitch) so bullets go where you're looking
	const FRotator SpawnRot = GetController()
		? GetController()->GetControlRotation()
		: GetActorRotation();
	const FVector AimDir = SpawnRot.Vector();
	// Spawn tracer exactly at the muzzle flash orb
	FVector SpawnLoc;
	if (CurrentWeaponType == 5 && AK47MuzzleMesh)
		SpawnLoc = AK47MuzzleMesh->GetComponentLocation();
	else if (CurrentWeaponType == 0 && MinigunMuzzleMesh)
		SpawnLoc = MinigunMuzzleMesh->GetComponentLocation();
	else if (CurrentWeaponType == 1 && BazookaMuzzleMesh)
		SpawnLoc = BazookaMuzzleMesh->GetComponentLocation();
	else
		SpawnLoc = GetActorLocation() + AimDir * 40.f + FVector(0.f, 0.f, 60.f);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (CurrentWeaponType == 0 || CurrentWeaponType == 5) // Minigun / AK-47 — hitscan
	{
		// No spread — bullets go exactly where you aim
		const FVector ShotDir = AimDir;

		// Hitscan line trace
		const FVector TraceStart = FollowCamera
			? FollowCamera->GetComponentLocation()
			: (GetActorLocation() + FVector(0.f, 0.f, 76.f));
		const FVector TraceEnd = TraceStart + ShotDir * 50000.f;

		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(this);
		FHitResult HitResult;
		bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, TraceParams);

		if (bHit && HitResult.GetActor() && HitResult.GetActor()->ActorHasTag(FName("GiantCrab")))
		{
			const float BulletDamage = (CurrentWeaponType == 5) ? 12.f : 8.f;
			UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
			if (CitySub)
			{
				for (int32 CI = 0; CI < UPHCCitySubsystem::NumGiantCrabs; ++CI)
				{
					if (CitySub->Crabs[CI].Actor == HitResult.GetActor())
					{
						CitySub->Crabs[CI].Health -= BulletDamage;
						CitySub->Crabs[CI].DamageFlashTimer = 0.15f;
						HitMarkerTimer = 0.25f;
						if (CitySub->Crabs[CI].Health <= 0.f)
						{
							CrabKills++;
							const FVector CrabLoc = HitResult.GetActor()->GetActorLocation();
							APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
								APHCMissileProjectile::StaticClass(), CrabLoc, FRotator::ZeroRotator);
							if (TempProj) { TempProj->SpawnExplosion(CrabLoc); TempProj->Destroy(); }
							CitySub->RespawnCrab(CI);
						}
						break;
					}
				}
			}
		}

		// Check NPC helicopter hit
		if (bHit && HitResult.GetActor() && HitResult.GetActor()->ActorHasTag(FName("NPCHelicopter")))
		{
			const float BulletDamage = (CurrentWeaponType == 5) ? 12.f : 8.f;
			UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
			if (CitySub && CitySub->NPCHelicopter && !CitySub->bNPCHeliCrashing)
			{
				CitySub->NPCHeliHealth -= BulletDamage;
				CitySub->NPCHeliDamageFlashTimer = 0.15f;
				HitMarkerTimer = 0.25f;
				if (CitySub->NPCHeliHealth <= 0.f)
				{
					CitySub->OnNPCHeliDestroyed();
				}
			}
		}

		// Check player character hit (PvP)
		if (bHit && HitResult.GetActor() && HitResult.GetActor() != this)
		{
			APHCCityCharacter* HitPlayer = Cast<APHCCityCharacter>(HitResult.GetActor());
			if (HitPlayer)
			{
				const float BulletDmg = (CurrentWeaponType == 5) ? 12.f : 8.f;
				ServerInflictDamage(HitPlayer, BulletDmg);
				HitMarkerTimer = 0.25f;
			}
		}

		// Check NPC player hit
		if (bHit && HitResult.GetActor() && HitResult.GetActor()->ActorHasTag(FName("NPCPlayer")))
		{
			const float BulletDamage = (CurrentWeaponType == 5) ? 12.f : 8.f;
			UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
			if (CitySub)
			{
				for (int32 NI = 0; NI < UPHCCitySubsystem::NumNPCPlayers; ++NI)
				{
					if (CitySub->NPCPlayers[NI].Actor == HitResult.GetActor())
					{
						CitySub->NPCPlayers[NI].Health -= BulletDamage;
						CitySub->NPCPlayers[NI].DamageFlashTimer = 0.15f;
						HitMarkerTimer = 0.25f;
						if (CitySub->NPCPlayers[NI].Health <= 0.f)
						{
							const FVector NPCLoc = HitResult.GetActor()->GetActorLocation();
							APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
								APHCMissileProjectile::StaticClass(), NPCLoc, FRotator::ZeroRotator);
							if (TempProj) { TempProj->SpawnExplosion(NPCLoc); TempProj->Destroy(); }
							CitySub->RespawnNPCPlayer(NI);
						}
						break;
					}
				}
			}
		}

		// Visual tracer projectile (cosmetic only)
		APHCMissileProjectile* Bullet = World->SpawnActor<APHCMissileProjectile>(
			APHCMissileProjectile::StaticClass(), SpawnLoc, ShotDir.Rotation(), SP);
		if (Bullet)
		{
			Bullet->OwnerVehicle = this;
		}

		// COD recoil: per-shot pitch kick + yaw scatter
		if (bFirstPersonMode && GetController())
		{
			const float RecoilMult = FMath::Lerp(1.f, 0.5f, ADSAlpha);
			const float PitchKick = FMath::RandRange(0.04f, 0.08f) * RecoilMult;
			AddControllerPitchInput(PitchKick);
			RecoilPitchAccum += PitchKick;

			const float YawKick = FMath::RandRange(-0.03f, 0.03f) * RecoilMult;
			AddControllerYawInput(YawKick);
			RecoilYawAccum += YawKick;

			// Viewmodel kick impulse
			const float KickScale = (CurrentWeaponType == 5) ? 1.f : 0.5f;
			WeaponKickZ.Vel += 2.f * RecoilMult * KickScale;
			WeaponKickX.Vel += -1.f * RecoilMult * KickScale;

			ShotShakeTimer = 0.03f;
			MuzzleFlashLightTimer = 0.04f;
		}
	}
	else if (CurrentWeaponType == 1) // RPG — identical to Tank cannon
	{
		if (BazookaFireTimer > 0.f) return;
		BazookaFireTimer = 2.0f; // 2s cooldown (matches Tank)

		const FVector MuzzleLoc = GetActorLocation()
			+ AimDir * 120.f
			+ FVector(0.f, 0.f, 40.f);
		const FRotator MuzzleRot = SpawnRot;
		const FVector Dir = AimDir;

		APHCMissileProjectile* Proj = World->SpawnActor<APHCMissileProjectile>(
			APHCMissileProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
		if (Proj)
		{
			Proj->OwnerVehicle = this;
			Proj->Tags.Add(FName("RPGRocket"));
			Proj->bIsRPGRocket = true;
			Proj->bUseManualGravity = true;
			Proj->ManualVelocity = Dir * 15000.f;
			Proj->ManualGravityScale = 0.3f;

			// Smoke trail so the flight path is visible
			Proj->TrailDotScale = 0.8f;
			Proj->TrailDotLifespan = 2.5f;
			Proj->TrailDotInterval = 0.02f;
			Proj->TrailDotRandomSpread = 20.f;
			Proj->StartTrail(FLinearColor(0.5f, 0.5f, 0.45f));
		}
	}
	else if (CurrentWeaponType == 2) // Sword — 50 damage per hit (2 hits to kill)
	{
		const FVector MyLoc = GetActorLocation();
		const FVector Forward = GetActorForwardVector();
		const float SwingRange = 600.f;
		const float SwordDamage = 50.f;

		UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A || A->IsHidden() || !A->ActorHasTag(FName("GiantCrab"))) continue;

			const FVector CrabLoc = A->GetActorLocation();
			if (FVector::Dist2D(MyLoc, CrabLoc) > SwingRange) continue;

			const FVector ToCrab = (CrabLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToCrab) < -0.2f) continue;

			// Find matching crab in subsystem and apply damage
			if (CitySub)
			{
				for (int32 CI = 0; CI < UPHCCitySubsystem::NumGiantCrabs; ++CI)
				{
					if (CitySub->Crabs[CI].Actor == A)
					{
						CitySub->Crabs[CI].Health -= SwordDamage;
						CitySub->Crabs[CI].DamageFlashTimer = 0.2f;
						if (CitySub->Crabs[CI].Health <= 0.f)
						{
							CrabKills++;
							APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
								APHCMissileProjectile::StaticClass(), CrabLoc, FRotator::ZeroRotator);
							if (TempProj) { TempProj->SpawnExplosion(CrabLoc); TempProj->Destroy(); }
							CitySub->RespawnCrab(CI);
						}
						break;
					}
				}
			}
			break; // Hit one crab per swing
		}

		// Sword vs other players (PvP)
		for (TActorIterator<APHCCityCharacter> PIt(World); PIt; ++PIt)
		{
			APHCCityCharacter* Other = *PIt;
			if (!Other || Other == this || Other->IsHidden()) continue;

			const FVector PLoc = Other->GetActorLocation();
			if (FVector::Dist2D(MyLoc, PLoc) > SwingRange) continue;

			const FVector ToP = (PLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToP) < -0.2f) continue;

			ServerInflictDamage(Other, SwordDamage);
			HitMarkerTimer = 0.25f;
			break;
		}

		// Sword vs NPC players
		for (TActorIterator<AActor> It2(World); It2; ++It2)
		{
			AActor* A = *It2;
			if (!A || A->IsHidden() || !A->ActorHasTag(FName("NPCPlayer"))) continue;

			const FVector NLoc = A->GetActorLocation();
			if (FVector::Dist2D(MyLoc, NLoc) > SwingRange) continue;

			const FVector ToNPC = (NLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToNPC) < -0.2f) continue;

			if (CitySub)
			{
				for (int32 NI = 0; NI < UPHCCitySubsystem::NumNPCPlayers; ++NI)
				{
					if (CitySub->NPCPlayers[NI].Actor == A)
					{
						CitySub->NPCPlayers[NI].Health -= SwordDamage;
						CitySub->NPCPlayers[NI].DamageFlashTimer = 0.2f;
						if (CitySub->NPCPlayers[NI].Health <= 0.f)
						{
							APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
								APHCMissileProjectile::StaticClass(), NLoc, FRotator::ZeroRotator);
							if (TempProj) { TempProj->SpawnExplosion(NLoc); TempProj->Destroy(); }
							CitySub->RespawnNPCPlayer(NI);
						}
						break;
					}
				}
			}
			break;
		}
	}
	else if (CurrentWeaponType == 3) // Scythe — 34 damage per hit (3 hits to kill)
	{
		ScytheSwingTimer = 0.45f;

		const FVector MyLoc = GetActorLocation();
		const FVector Forward = GetActorForwardVector();
		const float SwingRange = 800.f;
		const float ScytheDamage = 34.f;

		UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A || A->IsHidden() || !A->ActorHasTag(FName("GiantCrab"))) continue;

			const FVector CrabLoc = A->GetActorLocation();
			if (FVector::Dist2D(MyLoc, CrabLoc) > SwingRange) continue;

			const FVector ToCrab = (CrabLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToCrab) < -0.2f) continue;

			if (CitySub)
			{
				for (int32 CI = 0; CI < UPHCCitySubsystem::NumGiantCrabs; ++CI)
				{
					if (CitySub->Crabs[CI].Actor == A)
					{
						CitySub->Crabs[CI].Health -= ScytheDamage;
						CitySub->Crabs[CI].DamageFlashTimer = 0.2f;
						if (CitySub->Crabs[CI].Health <= 0.f)
						{
							CrabKills++;
							APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
								APHCMissileProjectile::StaticClass(), CrabLoc, FRotator::ZeroRotator);
							if (TempProj) { TempProj->SpawnExplosion(CrabLoc); TempProj->Destroy(); }
							CitySub->RespawnCrab(CI);
						}
						break;
					}
				}
			}
			break; // Hit one crab per swing
		}

		// Scythe vs other players (PvP)
		for (TActorIterator<APHCCityCharacter> PIt(World); PIt; ++PIt)
		{
			APHCCityCharacter* Other = *PIt;
			if (!Other || Other == this || Other->IsHidden()) continue;

			const FVector PLoc = Other->GetActorLocation();
			if (FVector::Dist2D(MyLoc, PLoc) > SwingRange) continue;

			const FVector ToP = (PLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToP) < -0.2f) continue;

			ServerInflictDamage(Other, ScytheDamage);
			HitMarkerTimer = 0.25f;
			break;
		}

		// Scythe vs NPC players
		for (TActorIterator<AActor> It2(World); It2; ++It2)
		{
			AActor* A = *It2;
			if (!A || A->IsHidden() || !A->ActorHasTag(FName("NPCPlayer"))) continue;

			const FVector NLoc = A->GetActorLocation();
			if (FVector::Dist2D(MyLoc, NLoc) > SwingRange) continue;

			const FVector ToNPC = (NLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToNPC) < -0.2f) continue;

			if (CitySub)
			{
				for (int32 NI = 0; NI < UPHCCitySubsystem::NumNPCPlayers; ++NI)
				{
					if (CitySub->NPCPlayers[NI].Actor == A)
					{
						CitySub->NPCPlayers[NI].Health -= ScytheDamage;
						CitySub->NPCPlayers[NI].DamageFlashTimer = 0.2f;
						if (CitySub->NPCPlayers[NI].Health <= 0.f)
						{
							APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
								APHCMissileProjectile::StaticClass(), NLoc, FRotator::ZeroRotator);
							if (TempProj) { TempProj->SpawnExplosion(NLoc); TempProj->Destroy(); }
							CitySub->RespawnNPCPlayer(NI);
						}
						break;
					}
				}
			}
			break;
		}
	}
	else if (CurrentWeaponType == 4) // Dagger — 1-hit kill on crabs
	{
		// Start stab animation
		DaggerStabTimer = 0.25f;

		// Crabs have NoCollision, so sweep won't find them.
		// Instead, iterate all actors with "GiantCrab" tag within melee range.
		const FVector MyLoc = GetActorLocation();
		const FVector Forward = GetActorForwardVector();
		const float StabRange = 800.f; // generous melee range for dagger

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!A || A->IsHidden() || !A->ActorHasTag(FName("GiantCrab"))) continue;

			const FVector CrabLoc = A->GetActorLocation();
			const float Dist2D = FVector::Dist2D(MyLoc, CrabLoc);
			if (Dist2D > StabRange) continue;

			// Check that the crab is roughly in front of the player (120 degree cone)
			const FVector ToCrab = (CrabLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToCrab) < 0.0f) continue;

			// 1-hit kill: explode and respawn at origin
			CrabKills++;

			APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
				APHCMissileProjectile::StaticClass(), CrabLoc, FRotator::ZeroRotator);
			if (TempProj)
			{
				TempProj->SpawnExplosion(CrabLoc);
				TempProj->Destroy();
			}

			// Find in subsystem and respawn
			UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
			if (CitySub)
			{
				for (int32 CI = 0; CI < UPHCCitySubsystem::NumGiantCrabs; ++CI)
				{
					if (CitySub->Crabs[CI].Actor == A)
					{
						CitySub->Crabs[CI].Health = 0.f;
						CitySub->RespawnCrab(CI);
						break;
					}
				}
			}

			break; // Kill one crab per stab
		}

		// Dagger vs other players (PvP) — instant kill
		for (TActorIterator<APHCCityCharacter> PIt(World); PIt; ++PIt)
		{
			APHCCityCharacter* Other = *PIt;
			if (!Other || Other == this || Other->IsHidden()) continue;

			const FVector PLoc = Other->GetActorLocation();
			if (FVector::Dist2D(MyLoc, PLoc) > StabRange) continue;

			const FVector ToP = (PLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToP) < 0.0f) continue;

			ServerInflictDamage(Other, 200.f);  // guaranteed kill
			HitMarkerTimer = 0.25f;
			break;
		}

		// Dagger vs NPC players — 1-hit kill
		for (TActorIterator<AActor> It2(World); It2; ++It2)
		{
			AActor* A = *It2;
			if (!A || A->IsHidden() || !A->ActorHasTag(FName("NPCPlayer"))) continue;

			const FVector NLoc = A->GetActorLocation();
			if (FVector::Dist2D(MyLoc, NLoc) > StabRange) continue;

			const FVector ToNPC = (NLoc - MyLoc).GetSafeNormal2D();
			if (FVector::DotProduct(Forward.GetSafeNormal2D(), ToNPC) < 0.0f) continue;

			APHCMissileProjectile* TempProj = World->SpawnActor<APHCMissileProjectile>(
				APHCMissileProjectile::StaticClass(), NLoc, FRotator::ZeroRotator);
			if (TempProj) { TempProj->SpawnExplosion(NLoc); TempProj->Destroy(); }

			UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
			if (CitySub)
			{
				for (int32 NI = 0; NI < UPHCCitySubsystem::NumNPCPlayers; ++NI)
				{
					if (CitySub->NPCPlayers[NI].Actor == A)
					{
						CitySub->NPCPlayers[NI].Health = 0.f;
						CitySub->RespawnNPCPlayer(NI);
						break;
					}
				}
			}
			break;
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

void APHCCityCharacter::ServerUpdateMovement_Implementation(FVector_NetQuantize NewLocation, FRotator NewRotation)
{
	SetActorLocation(NewLocation);
	SetActorRotation(NewRotation);
}

// ---------------------------------------------------------------------------
// PvP damage RPCs
// ---------------------------------------------------------------------------

void APHCCityCharacter::ServerInflictDamage_Implementation(APHCCityCharacter* Victim, float Damage)
{
	if (!Victim || Victim == this) return;

	Victim->Health -= Damage;

	// Tell all clients to flash the victim's body red.
	Victim->MulticastDamageFlash();

	if (Victim->Health <= 0.f)
	{
		// Victim died — notify their client.
		Victim->ClientOnKilled();
		Victim->Health = Victim->MaxHealth;
		Victim->SetActorLocation(FVector(0.f, 0.f, 100.f));
	}
	else
	{
		// Victim took damage — notify their client for red screen overlay.
		Victim->ClientOnDamaged(Victim->Health);
	}
}

void APHCCityCharacter::MulticastDamageFlash_Implementation()
{
	PvPDamageFlashTimer = 0.15f;
}

void APHCCityCharacter::ClientOnDamaged_Implementation(float NewHealth)
{
	Health = NewHealth;
	DamageFlashTimer = 0.3f;  // triggers the red screen overlay in HUD NativeTick
}

void APHCCityCharacter::ClientOnKilled_Implementation()
{
	Deaths++;
	Health = MaxHealth;
	DamageFlashTimer = 0.5f;
	SetActorLocation(FVector(0.f, 0.f, 100.f));
}

// ---------------------------------------------------------------------------
// Jetpack
// ---------------------------------------------------------------------------

void APHCCityCharacter::EquipJetpack()
{
	if (bJetpackEquipped) return;
	bJetpackEquipped = true;

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylMesh  = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterial* ColorMat   = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));
	if (!CubeMesh || !CylMesh || !ConeMesh || !ColorMat) return;

	auto MakePart = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& Scale, const FLinearColor& Col, float Metal, float Rough,
		const FName& Name, const FLinearColor& Emis = FLinearColor::Black) -> UStaticMeshComponent*
	{
		auto* C = NewObject<UStaticMeshComponent>(this, Name);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		auto* Mat = UMaterialInstanceDynamic::Create(ColorMat, C);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Col);
		Mat->SetScalarParameterValue(TEXT("Metallic"), Metal);
		Mat->SetScalarParameterValue(TEXT("Roughness"), Rough);
		if (Emis.R > 0.001f || Emis.G > 0.001f || Emis.B > 0.001f)
			Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Emis);
		C->SetMaterial(0, Mat);
		C->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		C->RegisterComponent();
		return C;
	};

	const FLinearColor BootShell(0.08f, 0.08f, 0.10f);   // dark techwear
	const FLinearColor BootTrim(0.20f, 0.20f, 0.25f);    // lighter accent
	const FLinearColor ThrustGlow(0.2f, 0.6f, 1.0f);     // cyan-blue emissive

	// Left boot — outer shell wrapping over the foot
	JetpackBodyMesh = MakePart(CubeMesh,
		FVector(4.f, -11.f, -66.f), FRotator::ZeroRotator,
		FVector(0.24f, 0.14f, 0.12f), BootShell, 0.85f, 0.30f, TEXT("AirBootL"));

	// Right boot — outer shell
	JetpackNozzleL = MakePart(CubeMesh,
		FVector(4.f, 11.f, -66.f), FRotator::ZeroRotator,
		FVector(0.24f, 0.14f, 0.12f), BootShell, 0.85f, 0.30f, TEXT("AirBootR"));

	// Left boot ankle cuff
	JetpackNozzleR = MakePart(CylMesh,
		FVector(2.f, -11.f, -58.f), FRotator::ZeroRotator,
		FVector(0.08f, 0.08f, 0.04f), BootTrim, 0.80f, 0.35f, TEXT("AirCuffL"));

	// Right boot ankle cuff
	JetpackStrapL = MakePart(CylMesh,
		FVector(2.f, 11.f, -58.f), FRotator::ZeroRotator,
		FVector(0.08f, 0.08f, 0.04f), BootTrim, 0.80f, 0.35f, TEXT("AirCuffR"));

	// Left sole thruster — glowing disc under the boot
	JetpackStrapR = MakePart(CylMesh,
		FVector(4.f, -11.f, -74.f), FRotator::ZeroRotator,
		FVector(0.10f, 0.10f, 0.015f), ThrustGlow, 0.0f, 1.0f, TEXT("AirThrusterL"),
		FLinearColor(0.3f, 1.5f, 3.0f));

	// Right sole thruster
	JetpackFlameL = MakePart(CylMesh,
		FVector(4.f, 11.f, -74.f), FRotator::ZeroRotator,
		FVector(0.10f, 0.10f, 0.015f), ThrustGlow, 0.0f, 1.0f, TEXT("AirThrusterR"),
		FLinearColor(0.3f, 1.5f, 3.0f));

	// Left exhaust cone — small thrust flame below sole
	JetpackFlameR = MakePart(ConeMesh,
		FVector(4.f, -11.f, -82.f), FRotator(180.f, 0.f, 0.f),
		FVector(0.04f, 0.04f, 0.08f),
		FLinearColor(0.4f, 0.7f, 1.0f), 0.0f, 1.0f, TEXT("AirFlameL"),
		FLinearColor(1.0f, 3.0f, 5.0f));

	// Right exhaust cone
	// (reuse JetpackFlameL/R names for the flame cones that animate in Tick)
	// We'll store left flame in JetpackFlameR, right flame needs a temp — swap below

	auto* RightFlame = MakePart(ConeMesh,
		FVector(4.f, 11.f, -82.f), FRotator(180.f, 0.f, 0.f),
		FVector(0.04f, 0.04f, 0.08f),
		FLinearColor(0.4f, 0.7f, 1.0f), 0.0f, 1.0f, TEXT("AirFlameR"),
		FLinearColor(1.0f, 3.0f, 5.0f));

	// Reassign so the flame flicker animation in Tick targets the cones
	// JetpackFlameL = right sole thruster (cylinder), JetpackFlameR = left exhaust cone
	// We need the two cones in JetpackFlameL/R for the flicker anim
	JetpackFlameL = JetpackFlameR; // left exhaust cone
	JetpackFlameR = RightFlame;    // right exhaust cone

	// Glow lights under each boot
	auto AddGlow = [&](const FVector& Loc, const FColor& Col, const FName& Name) -> UPointLightComponent*
	{
		auto* PL = NewObject<UPointLightComponent>(this, Name);
		PL->SetRelativeLocation(Loc);
		PL->SetIntensity(4000.f);
		PL->SetAttenuationRadius(400.f);
		PL->SetLightColor(Col);
		PL->SetCastShadows(false);
		PL->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		PL->RegisterComponent();
		return PL;
	};

	JetpackGlowL = AddGlow(FVector(4.f, -11.f, -80.f), FColor(80, 180, 255), TEXT("AirGlowL"));
	JetpackGlowR = AddGlow(FVector(4.f, 11.f, -80.f), FColor(80, 180, 255), TEXT("AirGlowR"));
}

void APHCCityCharacter::UnequipJetpack()
{
	if (!bJetpackEquipped) return;
	bJetpackEquipped = false;

	auto Destroy = [](UActorComponent*& C)
	{
		if (C) { C->DestroyComponent(); C = nullptr; }
	};

	Destroy(reinterpret_cast<UActorComponent*&>(JetpackBodyMesh));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackNozzleL));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackNozzleR));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackFlameL));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackFlameR));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackStrapL));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackStrapR));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackGlowL));
	Destroy(reinterpret_cast<UActorComponent*&>(JetpackGlowR));
}

// ---------------------------------------------------------------------------
// SetFirstPersonMode — Toggle between FP and TP camera
// ---------------------------------------------------------------------------

void APHCCityCharacter::SetFirstPersonMode(bool bFirstPerson)
{
	bFirstPersonMode = bFirstPerson;

	if (!bFirstPerson)
	{
		bADS = false;
		ADSAlpha = 0.f;
		RecoilPitchAccum = 0.f;
		RecoilYawAccum = 0.f;
	}

	if (!CameraBoom) return;

	if (bFirstPerson)
	{
		CameraBoom->TargetArmLength = 0.f;
		CameraBoom->SocketOffset = FVector(0.f, 0.f, 76.f);
		CameraBoom->bEnableCameraLag = false;
		CameraBoom->bEnableCameraRotationLag = false;
		bUseControllerRotationYaw = true;
	}
	else
	{
		CameraBoom->TargetArmLength = 450.f;
		CameraBoom->SocketOffset = FVector(0.f, 75.f, 60.f);
		CameraBoom->bEnableCameraLag = true;
		CameraBoom->CameraLagSpeed = 10.f;
		CameraBoom->bEnableCameraRotationLag = true;
		CameraBoom->CameraRotationLagSpeed = 8.f;
		bUseControllerRotationYaw = false;
	}

	const bool bShow = !bFirstPerson;
	auto SetVis = [bShow](UStaticMeshComponent* M) { if (M) M->SetVisibility(bShow); };

	// Head & Face
	SetVis(HeadMesh); SetVis(JawMesh); SetVis(NeckMesh);
	SetVis(LeftEyeWhiteMesh); SetVis(RightEyeWhiteMesh);
	SetVis(LeftPupilMesh); SetVis(RightPupilMesh);
	SetVis(NoseMesh); SetVis(MouthMesh);
	SetVis(LeftEarMesh); SetVis(RightEarMesh);
	SetVis(LeftBrowMesh); SetVis(RightBrowMesh);

	// Hair
	SetVis(HairTopMesh); SetVis(HairBackMesh);
	SetVis(HairSideLMesh); SetVis(HairSideRMesh);

	// Torso
	SetVis(ChestMesh); SetVis(AbsMesh); SetVis(PelvisMesh);
	SetVis(CollarMesh); SetVis(BeltMesh); SetVis(BeltBuckleMesh);

	// Left Arm
	SetVis(LeftShoulderMesh); SetVis(LeftUpperArmMesh);
	SetVis(LeftElbowMesh); SetVis(LeftForearmMesh); SetVis(LeftHandMesh);

	// Right Arm
	SetVis(RightShoulderMesh); SetVis(RightUpperArmMesh);
	SetVis(RightElbowMesh); SetVis(RightForearmMesh); SetVis(RightHandMesh);

	// Left Leg
	SetVis(LeftThighMesh); SetVis(LeftKneeMesh); SetVis(LeftShinMesh);
	SetVis(LeftFootMesh); SetVis(LeftToeMesh); SetVis(LeftSoleMesh);

	// Right Leg
	SetVis(RightThighMesh); SetVis(RightKneeMesh); SetVis(RightShinMesh);
	SetVis(RightFootMesh); SetVis(RightToeMesh); SetVis(RightSoleMesh);

	// Jetpack
	SetVis(JetpackBodyMesh); SetVis(JetpackNozzleL); SetVis(JetpackNozzleR);
	SetVis(JetpackFlameL); SetVis(JetpackFlameR);
	SetVis(JetpackStrapL); SetVis(JetpackStrapR);
}
