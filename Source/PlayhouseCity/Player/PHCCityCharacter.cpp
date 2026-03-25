// Copyright PlayhouseCity. All Rights Reserved.

#include "Player/PHCCityCharacter.h"
#include "PlayhouseCity.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Core/PHCPlayerController.h"
#include "Core/PHCCitySubsystem.h"
#include "Core/PHCGameInstance.h"
#include "Vehicle/PHCMissileProjectile.h"
#include "Vehicle/PHCRaptorPawn.h"
#include "Vehicle/PHCBlackbirdPawn.h"
#include "Vehicle/PHCBlackHawkPawn.h"
#include "Vehicle/PHCVehiclePawn.h"
#include "Vehicle/PHCDuneBuggyPawn.h"
#include "Vehicle/PHCDirtBikePawn.h"
#include "Vehicle/PHCTankPawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
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
	CameraBoom->TargetArmLength = 320.f;
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
	//  Mixamo Skeletal Mesh Character
	// ====================================================================
	CharacterSkelMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterSkelMesh"));
	CharacterSkelMesh->SetupAttachment(RootComponent);
	CharacterSkelMesh->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
	CharacterSkelMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	CharacterSkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterSkelMesh->CastShadow = true;
	CharacterSkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	// ====================================================================
	//  Legacy 45-component body (hidden — kept for NPC system compatibility)
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
	NeckMesh = MakeCyl(TEXT("NeckMesh"), FVector(0.f, 0.f, 56.f), 4.f, 10.f);

	// ================================================================
	//  TORSO — flat cube-based for masculine PUBG tactical vest look
	// ================================================================
	// Chest: flat, wide, angular — like a ballistic vest plate, reaches up to neck
	ChestMesh = MakeCube(TEXT("ChestMesh"), FVector(0.f, 0.f, 40.f),
		FVector(10.f, 26.f, 22.f));
	// Abs: flat stomach — overlaps chest and pelvis generously
	AbsMesh = MakeCube(TEXT("AbsMesh"), FVector(0.f, 0.f, 22.f),
		FVector(9.f, 20.f, 18.f));
	// Pelvis/hips: wider to cover leg tops, overlaps abs
	PelvisMesh = MakeCube(TEXT("PelvisMesh"), FVector(0.f, 0.f, 8.f),
		FVector(10.f, 24.f, 22.f));

	// ================================================================
	//  CLOTHING DETAILS
	// ================================================================
	// T-shirt collar
	CollarMesh = MakeCube(TEXT("CollarMesh"), FVector(2.f, 0.f, 52.f), FVector(10.f, 26.f, 2.f));
	// Belt
	BeltMesh = MakeCube(TEXT("BeltMesh"), FVector(0.f, 0.f, 20.f), FVector(12.f, 36.f, 3.f));
	// Belt buckle (small square in front)
	BeltBuckleMesh = MakeCube(TEXT("BeltBuckle"), FVector(8.f, 0.f, 20.f), FVector(3.f, 4.f, 4.f));

	// ================================================================
	//  LEFT ARM — shoulder sphere → upper arm cyl → elbow → forearm → hand
	// ================================================================
	LeftShoulderMesh = MakeSph(TEXT("LShld"),  FVector(0.f, -18.f, 50.f), 7.f);
	LeftUpperArmMesh = MakeCyl(TEXT("LUArm"),  FVector(0.f, -19.f, 40.f), 4.5f, 20.f);
	LeftElbowMesh    = MakeSph(TEXT("LElbow"), FVector(0.f, -19.f, 32.f), 4.f);
	LeftForearmMesh  = MakeCyl(TEXT("LFArm"),  FVector(0.f, -19.f, 20.f), 4.f, 18.f);
	LeftHandMesh     = MakeSph(TEXT("LHand"),  FVector(0.f, -19.f,  6.f), 3.5f);

	// ================================================================
	//  RIGHT ARM — mirror
	// ================================================================
	RightShoulderMesh = MakeSph(TEXT("RShld"),  FVector(0.f, 18.f, 50.f), 7.f);
	RightUpperArmMesh = MakeCyl(TEXT("RUArm"),  FVector(0.f, 19.f, 40.f), 4.5f, 20.f);
	RightElbowMesh    = MakeSph(TEXT("RElbow"), FVector(0.f, 19.f, 32.f), 4.f);
	RightForearmMesh  = MakeCyl(TEXT("RFArm"),  FVector(0.f, 19.f, 20.f), 4.f, 18.f);
	RightHandMesh     = MakeSph(TEXT("RHand"),  FVector(0.f, 19.f,  6.f), 3.5f);

	// ================================================================
	//  LEFT LEG — thigh → knee → shin → foot + toe cap + sole
	// ================================================================
	LeftThighMesh = MakeCyl(TEXT("LThigh"), FVector(0.f, -11.f, -8.f), 6.5f, 28.f);
	LeftKneeMesh  = MakeSph(TEXT("LKnee"),  FVector(0.f, -11.f, -30.f), 5.f);
	LeftShinMesh  = MakeCyl(TEXT("LShin"),  FVector(0.f, -11.f, -48.f), 4.5f, 24.f);
	LeftFootMesh  = MakeCube(TEXT("LFoot"), FVector(4.f, -11.f, -68.f), FVector(22.f, 12.f, 8.f));
	LeftToeMesh   = MakeOval(TEXT("LToe"),  FVector(14.f, -11.f, -67.f), FVector(0.10f, 0.12f, 0.08f));
	LeftSoleMesh  = MakeCube(TEXT("LSole"), FVector(4.f, -11.f, -73.f), FVector(22.f, 12.f, 2.5f));

	// ================================================================
	//  RIGHT LEG — mirror
	// ================================================================
	RightThighMesh = MakeCyl(TEXT("RThigh"), FVector(0.f, 11.f, -8.f), 6.5f, 28.f);
	RightKneeMesh  = MakeSph(TEXT("RKnee"),  FVector(0.f, 11.f, -30.f), 5.f);
	RightShinMesh  = MakeCyl(TEXT("RShin"),  FVector(0.f, 11.f, -48.f), 4.5f, 24.f);
	RightFootMesh  = MakeCube(TEXT("RFoot"), FVector(4.f, 11.f, -68.f), FVector(22.f, 12.f, 8.f));
	RightToeMesh   = MakeOval(TEXT("RToe"),  FVector(14.f, 11.f, -67.f), FVector(0.10f, 0.12f, 0.08f));
	RightSoleMesh  = MakeCube(TEXT("RSole"), FVector(4.f, 11.f, -73.f), FVector(22.f, 12.f, 2.5f));

	// ================================================================
	//  LIGHTSABER — Obi-Wan Kenobi style hilt + blue plasma blade
	// ================================================================
	SaberHiltMesh    = MakeCyl(TEXT("SaberHilt"),    FVector(0.f, 26.f, 10.f), 1.8f, 22.f);
	SaberEmitterMesh = MakeCyl(TEXT("SaberEmitter"), FVector(0.f, 26.f, 22.f), 2.2f, 4.f);
	SaberPommelMesh  = MakeSph(TEXT("SaberPommel"),  FVector(0.f, 26.f, -1.f), 2.4f);
	SaberBladeMesh   = MakeCyl(TEXT("SaberBlade"),   FVector(0.f, 26.f, 60.f), 1.2f, 70.f);
	SaberCoreMesh    = MakeCyl(TEXT("SaberCore"),     FVector(0.f, 26.f, 60.f), 0.5f, 70.f);

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

	// ================================================================
	//  50 CAL SNIPER RIFLE — long receiver + barrel + scope + stock + mag + muzzle
	// ================================================================
	SniperBodyMesh   = MakeCube(TEXT("SniperBody"),   FVector(0.f, 26.f, 40.f), FVector(44.f, 3.f, 4.5f));
	SniperBarrelMesh = MakeCyl(TEXT("SniperBarrel"),  FVector(0.f, 26.f, 80.f), 1.4f, 80.f);
	SniperScopeMesh  = MakeCyl(TEXT("SniperScope"),   FVector(0.f, 26.f, 55.f), 2.f, 20.f);
	SniperStockMesh  = MakeCube(TEXT("SniperStock"),  FVector(0.f, 26.f, 34.f), FVector(30.f, 3.f, 5.f));
	SniperMagMesh    = MakeCube(TEXT("SniperMag"),    FVector(0.f, 26.f, 28.f), FVector(4.f, 2.5f, 14.f));
	SniperMuzzleMesh = MakeSph(TEXT("SniperMuzzle"),  FVector(0.f, 26.f, 125.f), 6.f);

	// (Flashlight spotlight created in BeginPlay)

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

	// ---- Daily login check ----
	if (IsLocallyControlled())
	{
		CheckDailyLogin();

		// Dev account: max out level
		if (UPHCGameInstance* DevGI = Cast<UPHCGameInstance>(GetWorld()->GetGameInstance()))
		{
			if (DevGI->LocalPlayerProfile.Username == TEXT("Dev"))
			{
				PlayerLevel = 18;
				XPToNextLevel = 100 * PlayerLevel;
				PlayerXP = 0;
			}
		}
	}

	// ---- Mixamo skeletal mesh: load mesh + animations and hide old body ----
	USkeletalMesh* MixamoMesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Characters/Mixamo/Ch15_nonPBR.Ch15_nonPBR"));
	if (MixamoMesh && CharacterSkelMesh)
	{
		CharacterSkelMesh->SetSkeletalMeshAsset(MixamoMesh);
		UE_LOG(LogPlayhouseCity, Warning, TEXT("Mixamo mesh loaded: %s"), *MixamoMesh->GetName());
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("Mixamo mesh FAILED to load! MixamoMesh=%d CharacterSkelMesh=%d"),
			MixamoMesh ? 1 : 0, CharacterSkelMesh ? 1 : 0);
	}

	IdleAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Mixamo/Animations/idle.idle"));
	WalkAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Mixamo/Animations/walking.walking"));
	RunAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Mixamo/Animations/running.running"));
	JumpAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Mixamo/Animations/jumping_up.jumping_up"));
	RollerBladeAnim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Mixamo/Animations/rollerblade_walk_Anim.rollerblade_walk_Anim"));

	UE_LOG(LogPlayhouseCity, Warning, TEXT("Mixamo anims: idle=%d walk=%d run=%d jump=%d rollerblade=%d"),
		IdleAnim ? 1 : 0, WalkAnim ? 1 : 0, RunAnim ? 1 : 0, JumpAnim ? 1 : 0, RollerBladeAnim ? 1 : 0);

	if (CharacterSkelMesh && CharacterSkelMesh->GetSkeletalMeshAsset() && IdleAnim)
	{
		CharacterSkelMesh->PlayAnimation(IdleAnim, true);
		UE_LOG(LogPlayhouseCity, Log, TEXT("Mixamo character loaded with %d animations"),
			(IdleAnim ? 1 : 0) + (WalkAnim ? 1 : 0) + (RunAnim ? 1 : 0) + (JumpAnim ? 1 : 0));

		// Hide all old static mesh body parts
		auto HideOld = [](UStaticMeshComponent* M) { if (M) M->SetVisibility(false); };
		HideOld(HeadMesh); HideOld(JawMesh); HideOld(NeckMesh);
		HideOld(LeftEyeWhiteMesh); HideOld(RightEyeWhiteMesh);
		HideOld(LeftPupilMesh); HideOld(RightPupilMesh);
		HideOld(NoseMesh); HideOld(MouthMesh);
		HideOld(LeftEarMesh); HideOld(RightEarMesh);
		HideOld(LeftBrowMesh); HideOld(RightBrowMesh);
		HideOld(HairTopMesh); HideOld(HairBackMesh);
		HideOld(HairSideLMesh); HideOld(HairSideRMesh);
		HideOld(ChestMesh); HideOld(AbsMesh); HideOld(PelvisMesh);
		HideOld(CollarMesh); HideOld(BeltMesh); HideOld(BeltBuckleMesh);
		HideOld(LeftShoulderMesh); HideOld(LeftUpperArmMesh);
		HideOld(LeftElbowMesh); HideOld(LeftForearmMesh); HideOld(LeftHandMesh);
		HideOld(RightShoulderMesh); HideOld(RightUpperArmMesh);
		HideOld(RightElbowMesh); HideOld(RightForearmMesh); HideOld(RightHandMesh);
		HideOld(LeftThighMesh); HideOld(LeftKneeMesh); HideOld(LeftShinMesh);
		HideOld(LeftFootMesh); HideOld(LeftToeMesh); HideOld(LeftSoleMesh);
		HideOld(RightThighMesh); HideOld(RightKneeMesh); HideOld(RightShinMesh);
		HideOld(RightFootMesh); HideOld(RightToeMesh); HideOld(RightSoleMesh);
	}

	// ---- PBR Materials — production quality character rendering ----------
	UMaterial* ColorMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));

	// Try subsurface skin material (created by editor on first run)
	UMaterial* SkinMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCSkin.M_PHCSkin"));
	if (!SkinMat) SkinMat = ColorMat; // graceful fallback

	// F-22 ClearCoat material for armor pieces
	UMaterial* ClearCoatMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCClearCoat.M_PHCClearCoat"));
	if (!ClearCoatMat) ClearCoatMat = ColorMat; // fallback

	auto ApplyArmor = [&](UStaticMeshComponent* Comp,
		const FLinearColor& Color, float Roughness, float Metallic = 0.9f)
	{
		if (!Comp || !Comp->GetStaticMesh() || !ClearCoatMat) return;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ClearCoatMat, Comp);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			MID->SetScalarParameterValue(TEXT("ClearCoat"), 1.0f);
			MID->SetScalarParameterValue(TEXT("ClearCoatRoughness"), 0.03f);
			Comp->SetMaterial(0, MID);
			BodyMIDs.Add(MID);
		}
	};

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
				FLinearColor(0.90f, 0.18f, 0.08f));  // Richer blood-warm SSS
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.45f);  // Stronger translucency
			// Subtle warm emissive for lifelike skin warmth
			MID->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(0.015f, 0.007f, 0.003f));
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
	// F-22 stealth armor colors
	const FLinearColor ArmorBase(0.04f, 0.04f, 0.05f);      // near-black armor plate
	const FLinearColor ArmorTrim(0.35f, 0.37f, 0.38f);      // F-22 stealth gray (collar/belt)
	const FLinearColor Denim(0.08f, 0.10f, 0.20f);           // blue denim jeans
	const FLinearColor DenimDark(0.06f, 0.08f, 0.16f);      // darker denim (knees/seams)
	const FLinearColor Belt(0.12f, 0.08f, 0.04f);           // brown leather belt
	const FLinearColor Buckle(0.50f, 0.45f, 0.30f);         // brass buckle
	const FLinearColor Shoe(0.06f, 0.06f, 0.07f);           // dark shoes
	const FLinearColor ShoeToe(0.05f, 0.05f, 0.06f);
	const FLinearColor Sole(0.03f, 0.03f, 0.03f);

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

	// F-22 stealth armor — ClearCoat material matching the F-22 Raptor
	ApplyArmor(ChestMesh,          ArmorBase, 0.25f, 0.9f);
	ApplyArmor(AbsMesh,            ArmorBase, 0.25f, 0.9f);
	ApplyArmor(CollarMesh,         ArmorTrim, 0.20f, 0.9f);
	ApplyArmor(LeftShoulderMesh,   ArmorBase, 0.25f, 0.9f);
	ApplyArmor(RightShoulderMesh,  ArmorBase, 0.25f, 0.9f);
	ApplyArmor(LeftUpperArmMesh,   ArmorBase, 0.25f, 0.9f);
	ApplyArmor(RightUpperArmMesh,  ArmorBase, 0.25f, 0.9f);

	// Armor on elbows and forearms
	ApplyArmor(LeftElbowMesh,    ArmorTrim, 0.22f, 0.9f);
	ApplyArmor(RightElbowMesh,   ArmorTrim, 0.22f, 0.9f);
	ApplyArmor(LeftForearmMesh,  ArmorBase, 0.28f, 0.9f);
	ApplyArmor(RightForearmMesh, ArmorBase, 0.28f, 0.9f);

	// Armor legs — ClearCoat matching F-22
	// Blue denim jeans
	ApplyColor(PelvisMesh,     Denim, 0.78f);
	ApplyColor(LeftThighMesh,  Denim, 0.78f);
	ApplyColor(RightThighMesh, Denim, 0.78f);
	ApplyColor(LeftKneeMesh,   DenimDark, 0.75f);
	ApplyColor(RightKneeMesh,  DenimDark, 0.75f);
	ApplyColor(LeftShinMesh,   Denim, 0.78f);
	ApplyColor(RightShinMesh,  Denim, 0.78f);

	// Brown leather belt + brass buckle
	ApplyColor(BeltMesh,       Belt, 0.65f);
	ApplyColor(BeltBuckleMesh, Buckle, 0.25f, 0.7f);

	// Shoes
	ApplyColor(LeftFootMesh,   Shoe, 0.35f, 0.4f);
	ApplyColor(RightFootMesh,  Shoe, 0.35f, 0.4f);
	ApplyColor(LeftToeMesh,    ShoeToe, 0.30f, 0.4f);
	ApplyColor(RightToeMesh,   ShoeToe, 0.30f, 0.4f);
	ApplyColor(LeftSoleMesh,   Sole, 0.90f);
	ApplyColor(RightSoleMesh,  Sole, 0.90f);

	// Scythe materials
	const FLinearColor ScytheWood(0.18f, 0.10f, 0.06f);   // dark walnut
	const FLinearColor ScytheSteel(0.20f, 0.22f, 0.25f);  // dark steel blade
	const FLinearColor ScytheEdge(0.75f, 0.78f, 0.80f);   // bright cutting edge
	ApplyColor(ScytheHandleMesh,    ScytheWood,  0.75f);
	ApplyColor(ScytheBladeMesh,     ScytheSteel, 0.30f, 0.85f);
	ApplyColor(ScytheBladeEdgeMesh, ScytheEdge,  0.15f, 0.95f);

	// Lightsaber materials — Darth Vader style (vivid red)
	{
		// Hilt: dark gunmetal with black grip
		ApplyColor(SaberHiltMesh, FLinearColor(0.18f, 0.18f, 0.20f), 0.30f, 0.85f);
		// Emitter ring: dark chrome accent
		ApplyColor(SaberEmitterMesh, FLinearColor(0.25f, 0.22f, 0.22f), 0.25f, 0.90f);
		// Pommel: black
		ApplyColor(SaberPommelMesh, FLinearColor(0.08f, 0.08f, 0.10f), 0.35f, 0.80f);
		// Blade: vivid red plasma (heavy emissive glow)
		if (SaberBladeMesh && SaberBladeMesh->GetStaticMesh() && ColorMat)
		{
			UMaterialInstanceDynamic* BladeMID = UMaterialInstanceDynamic::Create(ColorMat, SaberBladeMesh);
			if (BladeMID)
			{
				BladeMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.0f, 0.02f, 0.02f));
				BladeMID->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
				BladeMID->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
				BladeMID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(12.0f, 0.4f, 0.2f));
				SaberBladeMesh->SetMaterial(0, BladeMID);
			}
		}
		// Core: white-hot center of blade (slight pink tint)
		if (SaberCoreMesh && SaberCoreMesh->GetStaticMesh() && ColorMat)
		{
			UMaterialInstanceDynamic* CoreMID = UMaterialInstanceDynamic::Create(ColorMat, SaberCoreMesh);
			if (CoreMID)
			{
				CoreMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.0f, 0.75f, 0.75f));
				CoreMID->SetScalarParameterValue(TEXT("Roughness"), 0.0f);
				CoreMID->SetScalarParameterValue(TEXT("Metallic"), 0.0f);
				CoreMID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(16.0f, 4.0f, 3.0f));
				SaberCoreMesh->SetMaterial(0, CoreMID);
			}
		}
	}

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

	// Sniper materials — dark gunmetal with cyan glow
	const FLinearColor SniperMetal(0.15f, 0.15f, 0.18f);
	const FLinearColor SniperStock(0.10f, 0.08f, 0.06f);
	const FLinearColor SniperScope(0.05f, 0.05f, 0.08f);
	ApplyColor(SniperBodyMesh,   SniperMetal, 0.30f, 0.9f);
	ApplyColor(SniperBarrelMesh, SniperMetal, 0.20f, 0.95f);
	ApplyColor(SniperScopeMesh,  SniperScope, 0.15f, 0.8f);
	ApplyColor(SniperStockMesh,  SniperStock, 0.75f, 0.0f);
	ApplyColor(SniperMagMesh,    SniperMetal, 0.65f, 0.3f);
	// Sniper muzzle flash: bright emissive (hidden when not firing)
	if (SniperMuzzleMesh && SniperMuzzleMesh->GetStaticMesh() && ColorMat)
	{
		UMaterialInstanceDynamic* SnMuzMID = UMaterialInstanceDynamic::Create(ColorMat, SniperMuzzleMesh);
		if (SnMuzMID)
		{
			SnMuzMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.f, 0.6f, 0.1f));
			SnMuzMID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(10.f, 6.f, 2.f));
			SnMuzMID->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			SnMuzMID->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
			SniperMuzzleMesh->SetMaterial(0, SnMuzMID);
		}
	}

	// Mark as grounded immediately — ground plane has no collision.
	bHasBeenGrounded = true;

	// ---- Flashlight spotlight (created here, starts hidden) ----
	{
		FlashlightSpot = NewObject<USpotLightComponent>(this, TEXT("FlashlightSpot"));
		FlashlightSpot->SetupAttachment(RootComponent);
		FlashlightSpot->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
		FlashlightSpot->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
		FlashlightSpot->SetLightColor(FLinearColor(1.0f, 0.6f, 0.15f));
		FlashlightSpot->SetIntensity(480000.f);
		FlashlightSpot->SetAttenuationRadius(20000.f);
		FlashlightSpot->SetOuterConeAngle(40.f);
		FlashlightSpot->SetInnerConeAngle(25.f);
		FlashlightSpot->SetCastShadows(false);
		FlashlightSpot->SetVisibility(false);
		FlashlightSpot->RegisterComponent();
	}

	// ---- Lightsaber point light (red glow) ----
	{
		SaberGlow = NewObject<UPointLightComponent>(this, TEXT("SaberGlow"));
		SaberGlow->SetupAttachment(RootComponent);
		SaberGlow->SetLightColor(FLinearColor(1.0f, 0.1f, 0.05f));
		SaberGlow->SetIntensity(5000.f);
		SaberGlow->SetAttenuationRadius(800.f);
		SaberGlow->SetCastShadows(false);
		SaberGlow->SetVisibility(false);
		SaberGlow->RegisterComponent();
	}

	// ---- 5% taller ----
	SetActorScale3D(FVector(1.05f));

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

	// ---- Liquid glass arrow indicator (visible to other players only) ----
	{
		UStaticMesh* ConeMeshAsset = LoadObject<UStaticMesh>(nullptr,
			TEXT("/Engine/BasicShapes/Cone.Cone"));
		UMaterial* GlassMat = LoadObject<UMaterial>(nullptr,
			TEXT("/Game/Materials/M_PHCGlass.M_PHCGlass"));

		if (ConeMeshAsset && GlassMat)
		{
			ArrowIndicator = NewObject<UStaticMeshComponent>(this, TEXT("ArrowIndicator"));
			ArrowIndicator->SetupAttachment(RootComponent);
			ArrowIndicator->SetStaticMesh(ConeMeshAsset);
			ArrowIndicator->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
			ArrowIndicator->SetRelativeRotation(FRotator(180.f, 0.f, 0.f));
			ArrowIndicator->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.8f));
			ArrowIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ArrowIndicator->SetCastShadow(false);
			ArrowIndicator->bOwnerNoSee = true;

			ArrowMID = UMaterialInstanceDynamic::Create(GlassMat, this);
			ArrowMID->SetVectorParameterValue(TEXT("BaseColor"),
				FLinearColor(0.5f, 0.8f, 1.0f));
			ArrowMID->SetScalarParameterValue(TEXT("Opacity"), 0.7f);
			ArrowMID->SetScalarParameterValue(TEXT("Metallic"), 0.3f);
			ArrowMID->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
			ArrowMID->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(3.0f, 5.0f, 10.0f));
			ArrowIndicator->SetMaterial(0, ArrowMID);
			ArrowIndicator->RegisterComponent();
		}
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

	// Ensure old body meshes stay hidden (Mixamo replaces them)
	HideOldBodyMeshes();

	// If attached to a vehicle as a visible rider (dirt bike), skip all
	// movement and ground-following — the vehicle handles positioning.
	if (GetAttachParentActor() != nullptr)
		return;

	// ---- Trip invisibility: hide this character for non-owning clients ----
	if (!IsLocallyControlled())
	{
		if (bIsTripping && !bTripHiddenLocally)
		{
			SetActorHiddenInGame(true);
			bTripHiddenLocally = true;
		}
		else if (!bIsTripping && bTripHiddenLocally)
		{
			SetActorHiddenInGame(false);
			bTripHiddenLocally = false;
		}
	}

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
				// Only show the floating name to other players, not to the local player.
				if (IsLocallyControlled())
				{
					NameTextComp->SetVisibility(false);
				}
				bNameTextSet = true;
			}
		}
	}

	// ---- ADS interpolation + FOV (Fortnite-style: works in 1P and 3P) ----
	{
		const float ADSTarget = bADS ? 1.f : 0.f;
		ADSAlpha = FMath::FInterpTo(ADSAlpha, ADSTarget, DeltaTime, ADSInterpSpeed);

		if (bFirstPersonMode)
		{
			if (FollowCamera)
			{
				const float ScopedFOV = (CurrentWeaponType == 6) ? SniperZoomFOV : ADSFOVAim;
				FollowCamera->SetFieldOfView(FMath::Lerp(ADSFOVHip, ScopedFOV, ADSAlpha));
			}
		}
		else
		{
			// 3rd person ADS: over-the-shoulder shift + zoom
			if (FollowCamera)
			{
				FollowCamera->SetFieldOfView(FMath::Lerp(90.f, 70.f, ADSAlpha));
			}
			if (CameraBoom)
			{
				// Hip: centered behind (0, 75, 60). ADS: shifted right + closer + higher
				const FVector HipOffset(0.f, 75.f, 60.f);
				const FVector ADSOffset(0.f, 90.f, 70.f);
				const float HipArm = 320.f;
				const float ADSArm = 180.f;
				CameraBoom->SocketOffset = FMath::Lerp(HipOffset, ADSOffset, ADSAlpha);
				CameraBoom->TargetArmLength = FMath::Lerp(HipArm, ADSArm, ADSAlpha);
			}
		}
	}

	// ---- Thermal scope post-process (MW2 style) -------------------------
	if (FollowCamera)
	{
		const bool bThermalActive = bThermalScope && CurrentWeaponType == 6
			&& bFirstPersonMode && ADSAlpha > 0.5f;

		FPostProcessSettings& PP = FollowCamera->PostProcessSettings;
		FollowCamera->PostProcessBlendWeight = bThermalActive ? 1.f : 0.f;

		if (bThermalActive)
		{
			// Desaturate scene to near-grayscale with cool blue-green tint
			PP.bOverride_ColorSaturation = true;
			PP.ColorSaturation = FVector4(0.08f, 0.08f, 0.08f, 1.f);

			PP.bOverride_ColorContrast = true;
			PP.ColorContrast = FVector4(1.3f, 1.3f, 1.3f, 1.f);

			PP.bOverride_ColorGain = true;
			PP.ColorGain = FVector4(0.7f, 0.85f, 0.9f, 1.f);

			PP.bOverride_SceneColorTint = true;
			PP.SceneColorTint = FLinearColor(0.6f, 0.75f, 0.8f);

			PP.bOverride_FilmSlope = true;
			PP.FilmSlope = 1.0f;
		}
		else
		{
			PP.bOverride_ColorSaturation = false;
			PP.bOverride_ColorContrast = false;
			PP.bOverride_ColorGain = false;
			PP.bOverride_SceneColorTint = false;
			PP.bOverride_FilmSlope = false;
		}

		// Thermal glow on heat sources
		// Only set emissive when thermal is active; on deactivation, save & restore
		// original emissive values so taillights/headlights aren't wiped out.
		{
			static bool bThermalWasActive = false;
			static TMap<UMaterialInstanceDynamic*, FLinearColor> SavedEmissive;

			auto SetThermalOnActor = [&](AActor* A, const FLinearColor& Emis)
			{
				if (!A) return;
				TArray<UStaticMeshComponent*> Meshes;
				A->GetComponents<UStaticMeshComponent>(Meshes);
				for (UStaticMeshComponent* M : Meshes)
				{
					if (!M) continue;
					if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
					{
						// Save original emissive on first thermal activation
						if (!SavedEmissive.Contains(MID))
						{
							FLinearColor Orig;
							MID->GetVectorParameterValue(TEXT("EmissiveColor"), Orig);
							SavedEmissive.Add(MID, Orig);
						}
						MID->SetVectorParameterValue(TEXT("EmissiveColor"), Emis);
					}
				}
			};

			auto RestoreEmissiveOnActor = [&](AActor* A)
			{
				if (!A) return;
				TArray<UStaticMeshComponent*> Meshes;
				A->GetComponents<UStaticMeshComponent>(Meshes);
				for (UStaticMeshComponent* M : Meshes)
				{
					if (!M) continue;
					if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
					{
						if (FLinearColor* Orig = SavedEmissive.Find(MID))
						{
							MID->SetVectorParameterValue(TEXT("EmissiveColor"), *Orig);
						}
					}
				}
			};

			auto GatherHeatSources = [&](TArray<AActor*>& Out)
			{
				UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("NPCPlayer"), Out);
				TArray<AActor*> Crabs;
				UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("GiantCrab"), Crabs);
				Out.Append(Crabs);
				TArray<AActor*> NPCHelis;
				UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("NPCHelicopter"), NPCHelis);
				Out.Append(NPCHelis);
				for (TActorIterator<APHCCityCharacter> It(GetWorld()); It; ++It)
					if (*It != this) Out.Add(*It);
				for (TActorIterator<APHCVehiclePawn> It(GetWorld()); It; ++It)
					Out.AddUnique(*It);
				for (TActorIterator<APHCDuneBuggyPawn> It(GetWorld()); It; ++It)
					Out.AddUnique(*It);
				for (TActorIterator<APHCRaptorPawn> It(GetWorld()); It; ++It)
					Out.AddUnique(*It);
				for (TActorIterator<APHCTankPawn> It(GetWorld()); It; ++It)
					Out.AddUnique(*It);
				for (TActorIterator<APHCDirtBikePawn> It(GetWorld()); It; ++It)
					Out.AddUnique(*It);
				for (TActorIterator<APHCBlackbirdPawn> It(GetWorld()); It; ++It)
					Out.AddUnique(*It);
				for (TActorIterator<APHCBlackHawkPawn> It(GetWorld()); It; ++It)
					Out.AddUnique(*It);
			};

			if (bThermalActive)
			{
				const FLinearColor ThermalEmis(6.f, 5.5f, 4.f);
				TArray<AActor*> HeatSources;
				GatherHeatSources(HeatSources);
				for (AActor* A : HeatSources)
					SetThermalOnActor(A, ThermalEmis);
			}
			else if (bThermalWasActive)
			{
				// Transitioning off — restore original emissive values
				TArray<AActor*> HeatSources;
				GatherHeatSources(HeatSources);
				for (AActor* A : HeatSources)
					RestoreEmissiveOnActor(A);
				SavedEmissive.Empty();
			}

			bThermalWasActive = bThermalActive;
		}
	}

	// ---- Recoil recovery (pull back toward zero, works in 1P and 3P) ----
	if (GetController())
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

	// ---- Bloom recovery (Fortnite-style: spread decays when not firing) ----
	{
		const bool bCurrentlyFiring = (bMinigunFiring || bAK47Firing);
		if (!bCurrentlyFiring && BloomAccum > 0.f)
		{
			BloomAccum = FMath::Max(0.f, BloomAccum - BloomRecoveryRate * DeltaTime);
		}
		// Movement spread penalty: walking adds spread, sprinting adds more
		const FVector Vel = GetVelocity();
		const float Speed = Vel.Size2D();
		BloomMovePenalty = FMath::GetMappedRangeValueClamped(
			FVector2D(10.f, 2000.f), FVector2D(0.f, 2.0f), Speed);
	}

	// ---- Camera shake (RPG fire recoil, GTA V style) --------------------
	if (CameraShakeTimer > 0.f && CameraBoom)
	{
		CameraShakeTimer -= DeltaTime;
		const FVector BaseOffset = bFirstPersonMode
			? FVector(0.f, 0.f, 96.f)
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

	// ---- Camera ground clamp (prevent seeing under the map) -------------
	if (!bFirstPersonMode && CameraBoom && FollowCamera && GetController())
	{
		const FVector CamPos = FollowCamera->GetComponentLocation();
		float GroundZ = 0.f;
		if (UPHCCitySubsystem* CitySub = GetWorld()->GetSubsystem<UPHCCitySubsystem>())
		{
			GroundZ = CitySub->GetTerrainHeight(CamPos.X, CamPos.Y);
		}
		const float MinCamZ = GroundZ + 50.f;
		if (CamPos.Z < MinCamZ)
		{
			// Camera is below terrain — clamp controller pitch upward
			FRotator CtrlRot = GetController()->GetControlRotation();
			// Normalize pitch to -180..180 range
			float Pitch = CtrlRot.Pitch;
			if (Pitch > 180.f) Pitch -= 360.f;
			// Positive pitch = looking down; reduce it to pull camera up
			if (Pitch > -10.f)
			{
				Pitch = FMath::FInterpTo(Pitch, -10.f, DeltaTime, 8.f);
				CtrlRot.Pitch = Pitch;
				GetController()->SetControlRotation(CtrlRot);
			}
		}
	}

	// =====================================================================
	//  Simulated proxy (remote player): skip all local movement, ground
	//  following, and input.  Position comes from UE's movement replication.
	// =====================================================================
	const bool bIsSimProxy = !IsLocallyControlled() && !HasAuthority();

	// MoveDir: captures the local movement direction for the animation section.
	// For simulated proxies, stays zero (movement comes from replication).
	FVector MoveDir = FVector::ZeroVector;
	// Speed: used by walk animation momentum lean. Computed for locals below,
	// estimated from position delta for sim proxies.
	float Speed = 0.f;

	// ---- Local-only movement, input, and ground following ---------------
	if (!bIsSimProxy)
	{

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
			float TLX = TouchLookX;
			float TLY = TouchLookY;

			// Apply ADS sensitivity to touch look (same as HandleLook)
			if (CurrentWeaponType == 6 && ADSAlpha > 0.01f)
			{
				const float ZoomRatio = SniperZoomFOV / 90.f;
				const float SniperSens = FMath::Lerp(0.15f, 0.04f, FMath::Clamp(1.f - ZoomRatio, 0.f, 1.f));
				const float SM = FMath::Lerp(1.f, SniperSens, ADSAlpha);
				TLX *= SM;
				TLY *= SM;
			}
			else if (ADSAlpha > 0.01f)
			{
				const float SM = FMath::Lerp(1.f, ADSSensitivityMult, ADSAlpha);
				TLX *= SM;
				TLY *= SM;
			}

			// Sniper scoped: deadzone + heavy smoothing on touch
			if (CurrentWeaponType == 6 && ADSAlpha > 0.3f)
			{
				const float Mag = FMath::Sqrt(TLX * TLX + TLY * TLY);
				if (Mag < 0.003f) { TLX = 0.f; TLY = 0.f; }

				const float SmoothSpd = FMath::Lerp(10.f, 2.5f, ADSAlpha);
				ScopedLookSmooth = FMath::Vector2DInterpTo(ScopedLookSmooth,
					FVector2D(TLX, TLY), DeltaTime, SmoothSpd);
				TLX = ScopedLookSmooth.X;
				TLY = ScopedLookSmooth.Y;
			}

			AddControllerYawInput(TLX);
			AddControllerPitchInput(TLY);
		}
	}

	// ---- Build horizontal movement direction ----------------------------
	MoveDir = PendingMoveInput;
	MoveDir.Z = 0.f;
	if (!MoveDir.IsNearlyZero())
	{
		MoveDir = MoveDir.GetClampedToMaxSize(1.f);
	}
	PendingMoveInput = FVector::ZeroVector;

	Speed = bJetpackEquipped ? 3576.f : bClimbingGearEquipped ? 1500.f : bRollerBladesEquipped ? (WalkSpeed * 3.f) : (bIsSprinting ? SprintSpeed : WalkSpeed);

	// ---- Smooth acceleration only (instant stop) ------------------------
	if (!MoveDir.IsNearlyZero())
	{
		MoveSpeedAlpha = FMath::FInterpConstantTo(MoveSpeedAlpha, 1.f, DeltaTime, 5.f);
		Speed *= MoveSpeedAlpha;
	}
	else
	{
		MoveSpeedAlpha = 0.f; // instant stop — no lingering traces
	}

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
	else if (bClimbingGearEquipped)
	{
		// ---- Climbing gear mode ------------------------------------------------
		const float ClimbSpeed = 1200.f;
		const float ClimbHorizSpeed = 1500.f;

		FVector Pos = GetActorLocation();
		FCollisionQueryParams ClimbParams;
		ClimbParams.AddIgnoredActor(this);

		// 8-directional horizontal ray cast for omni-directional surface detection
		const float TraceLen = 1000.f;
		const float ChestZ = Pos.Z + 80.f;
		const FVector TraceOrigin(Pos.X, Pos.Y, ChestZ);

		FHitResult BestHit;
		bool bFoundSurface = false;
		float BestDist = TraceLen;

		for (int32 D = 0; D < 8; ++D)
		{
			const float Angle = D * 45.f;
			const float AR = FMath::DegreesToRadians(Angle);
			const FVector Dir(FMath::Cos(AR), FMath::Sin(AR), 0.f);
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, TraceOrigin,
				TraceOrigin + Dir * TraceLen, ECC_WorldStatic, ClimbParams))
			{
				if (!Hit.bStartPenetrating && Hit.Distance < BestDist)
				{
					BestDist = Hit.Distance;
					BestHit = Hit;
					bFoundSurface = true;
				}
			}
		}

		// Check floor below — extend well past one platform gap (400 UU spacing)
		{
			FHitResult FloorHit;
			if (GetWorld()->LineTraceSingleByChannel(FloorHit,
				FVector(Pos.X, Pos.Y, Pos.Z + 20.f),
				FVector(Pos.X, Pos.Y, Pos.Z - 600.f),
				ECC_WorldStatic, ClimbParams))
			{
				if (!FloorHit.bStartPenetrating)
					bFoundSurface = true;
			}
		}

		// Check ceiling above — detect platforms above while climbing between them
		{
			FHitResult CeilHit;
			if (GetWorld()->LineTraceSingleByChannel(CeilHit,
				FVector(Pos.X, Pos.Y, Pos.Z + 100.f),
				FVector(Pos.X, Pos.Y, Pos.Z + 600.f),
				ECC_WorldStatic, ClimbParams))
			{
				if (!CeilHit.bStartPenetrating)
					bFoundSurface = true;
			}
		}

		bIsClinging = bFoundSurface;

		if (bIsClinging)
		{
			// Mobile: use look-Y for altitude
			float VertInput = ClimbingVerticalInput;
			if (bHUDTouchActive && FMath::Abs(TouchLookY) > 0.01f)
			{
				VertInput = FMath::Clamp(-TouchLookY * 3.f, -1.f, 1.f);
			}

			// Vertical movement
			float NewZ = Pos.Z + VertInput * ClimbSpeed * DeltaTime;
			if (NewZ < 58.f) NewZ = 58.f;

			// Horizontal movement — slide along surface
			if (!MoveDir.IsNearlyZero())
			{
				FVector HorizDelta = MoveDir * ClimbHorizSpeed * DeltaTime;
				if (BestDist < 800.f && !BestHit.Normal.IsNearlyZero())
				{
					HorizDelta = FVector::VectorPlaneProject(HorizDelta, BestHit.Normal);
				}
				AddActorWorldOffset(FVector(HorizDelta.X, HorizDelta.Y, 0.f), false);
			}

			// Push toward wall to maintain contact
			if (BestDist < 800.f && BestDist > 40.f)
			{
				const FVector ToWall = -BestHit.Normal * 80.f * DeltaTime;
				AddActorWorldOffset(FVector(ToWall.X, ToWall.Y, 0.f), false);
			}

			SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ));
			ManualVelocityZ = 0.f;
			bHasBeenGrounded = true;
		}
		else
		{
			// Not near surface — normal gravity
			if (!MoveDir.IsNearlyZero())
			{
				const FVector Delta = MoveDir * WalkSpeed * DeltaTime;
				AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
			}

			ManualVelocityZ -= 980.f * DeltaTime;
			float NewZ = Pos.Z + ManualVelocityZ * DeltaTime;
			if (NewZ <= 58.f) { NewZ = 58.f; ManualVelocityZ = 0.f; bHasBeenGrounded = true; }
			SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ));
		}
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
			const float TerrainInterp = bRollerBladesEquipped ? 60.f : 50.f;
			const float NewZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, TerrainInterp);
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
					if (!bHasBeenGrounded)
					{
						// In-air after jump — apply gravity and move vertically
						ManualVelocityZ -= 980.f * DeltaTime;
						float NewZ = Pos.Z + ManualVelocityZ * DeltaTime;
						if (NewZ <= BestZ)
						{
							// Landed
							NewZ = BestZ;
							ManualVelocityZ = 0.f;
							bHasBeenGrounded = true;
						}
						SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
					}
					else if (ManualVelocityZ > 10.f)
					{
						// Jump just started — launch upward
						bHasBeenGrounded = false;
						ManualVelocityZ -= 980.f * DeltaTime;
						float NewZ = Pos.Z + ManualVelocityZ * DeltaTime;
						SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
					}
					else
					{
						// On ground — snap to terrain
						bHasBeenGrounded = true;
						ManualVelocityZ = 0.f;
						const float GndInterp = bRollerBladesEquipped ? 60.f : 50.f;
						const float NewZ = FMath::FInterpTo(Pos.Z, BestZ, DeltaTime, GndInterp);
						SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
					}
				}
			}
			if (!bFoundSurface)
			{
				// No surface — always apply gravity (whether jumping or falling off edge)
				ManualVelocityZ -= 980.f * DeltaTime;
				float NewZ = Pos.Z + ManualVelocityZ * DeltaTime;
				if (NewZ <= FootOffset) { NewZ = FootOffset; ManualVelocityZ = 0.f; bHasBeenGrounded = true; }
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

	} // end if (!bIsSimProxy) — local movement/ground following

	// =====================================================================
	//  Walk Animation — natural human gait with spring-damped physics
	//  Features: harmonic sine, torso twist, hip sway, head stabilization,
	//  momentum lean, idle animation, hand/hair spring lag, breathing
	// =====================================================================
	{
		// For simulated proxies: detect movement from replicated position deltas.
		bool bMoving;
		if (bIsSimProxy)
		{
			const FVector CurPos = GetActorLocation();
			const FVector PosDelta = CurPos - PrevTickLocation;
			const FVector HorizDelta(PosDelta.X, PosDelta.Y, 0.f);
			const float HorizSpeed = HorizDelta.Size() / FMath::Max(DeltaTime, 0.001f);
			bMoving = HorizSpeed > 50.f;
			Speed = HorizSpeed;
			MoveDir = HorizDelta.GetSafeNormal();

			// Orient remote character body toward movement direction.
			if (bMoving && !MoveDir.IsNearlyZero())
			{
				const FRotator DesiredRot = MoveDir.Rotation();
				const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, 10.f);
				SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
			}
		}
		else
		{
			bMoving = !MoveDir.IsNearlyZero() && !bJetpackEquipped;
		}
		const float TargetBlend = bMoving ? 1.f : 0.f;
		WalkAnimBlend = bJetpackEquipped ? 0.f : FMath::FInterpTo(WalkAnimBlend, TargetBlend, DeltaTime, 8.f);

		// Mixamo animation state machine
		// Mixamo animation state machine
		if (CharacterSkelMesh && CharacterSkelMesh->GetSkeletalMeshAsset())
		{
			int32 NewState = 0;
			if (ManualVelocityZ > 50.f && JumpAnim) NewState = 3;
			else if (bMoving && bRollerBladesEquipped && RollerBladeAnim) NewState = 4;
			else if (bMoving && RunAnim) NewState = 2;

			if (NewState != CurrentAnimState)
			{
				CurrentAnimState = NewState;
				if (NewState == 0 && IdleAnim)
					CharacterSkelMesh->PlayAnimation(IdleAnim, true);
				else if (NewState == 4 && RollerBladeAnim)
				{
					CharacterSkelMesh->PlayAnimation(RollerBladeAnim, true);
					CharacterSkelMesh->SetPlayRate(0.5f); // slow walking pace
				}
				else if (NewState == 2 && RunAnim)
				{
					CharacterSkelMesh->PlayAnimation(RunAnim, true);
					CharacterSkelMesh->SetPlayRate(1.0f);
				}
				else if (NewState == 3 && JumpAnim)
				{
					CharacterSkelMesh->PlayAnimation(JumpAnim, false);
					CharacterSkelMesh->SetPlayRate(1.0f);
				}
			}

			// Prevent the snap: when near the end of the run anim,
			// smoothly wind back to the beginning by advancing past the end
			// The engine's looping wraps automatically, but the snap happens
			// because the last frame pose != first frame pose.
			// Fix: when we're in the last 10% of the anim, speed up playback
			// so the transition through the snap point is too fast to see
			if (CurrentAnimState == 2 && RunAnim && CharacterSkelMesh->IsPlaying())
			{
				float Pos = CharacterSkelMesh->GetPosition();
				float Length = RunAnim->GetPlayLength();
				float NormPos = Pos / FMath::Max(Length, 0.01f);
				// In the last 8% of the animation, speed through the snap
				if (NormPos > 0.92f)
					CharacterSkelMesh->SetPlayRate(4.0f);
				else
					CharacterSkelMesh->SetPlayRate(1.0f);
			}
		}

		if (bMoving)
		{
			const float CycleSpeed = bRollerBladesEquipped ? 1.f : (bIsSprinting ? 16.f : 15.f);
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

		const float RollerScale = bRollerBladesEquipped ? 0.1f : 1.f; // 90% reduction
		const float ThighAmp    = (bIsSprinting ? 45.f : 42.f) * RollerScale;
		const float UpperArmAmp = (bIsSprinting ? 40.f : 36.f) * RollerScale;
		const float KneeBendMax = (bIsSprinting ? 55.f : 48.f) * RollerScale;
		const float ElbowBendMax= (bIsSprinting ? 50.f : 42.f) * RollerScale;
		const float LeanAmp     = (bIsSprinting ? 10.f :  8.f) * RollerScale;
		const float BobAmp      = (bIsSprinting ?  3.f :  2.5f) * RollerScale;

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
		const float ShPivZ    = 50.f;
		const float HipPivZ   =  6.f;
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
				RightShoulderMesh, 19.f, RAimPitch, 15.f);
			// Left arm: support grip — forward 75 deg, more elbow bend to reach under barrel
			PoseArm(LeftUpperArmMesh, LeftForearmMesh, LeftElbowMesh, LeftHandMesh,
				LeftShoulderMesh, -19.f, 75.f, 25.f);
		}
		else
		{
			// Normal walk swing
			AnimateArm(LeftUpperArmMesh, LeftForearmMesh, LeftElbowMesh, LeftHandMesh,
				LeftShoulderMesh, -19.f, LArmSwing, LElbowBend);
			AnimateArm(RightUpperArmMesh, RightForearmMesh, RightElbowMesh, RightHandMesh,
				RightShoulderMesh, 19.f, RArmSwing, RElbowBend);
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
			ChestMesh->SetRelativeLocation(FVector(0.f, HipSway, 40.f + VertBounce));
			ChestMesh->SetRelativeRotation(FRotator(
				TorsoLean + MomentumPitch, ChestYaw, MomentumRoll));
		}
		if (AbsMesh)
		{
			AbsMesh->SetRelativeLocation(FVector(0.f, HipSway, 22.f + VertBounce * 0.8f));
			AbsMesh->SetRelativeRotation(FRotator(
				TorsoLean * 0.6f + MomentumPitch * 0.5f,
				(ChestYaw + PelvisYaw) * 0.5f,
				MomentumRoll * 0.5f));
		}
		if (PelvisMesh)
		{
			PelvisMesh->SetRelativeLocation(FVector(0.f,
				HipSway, 8.f + VertBounce * 0.4f));
			PelvisMesh->SetRelativeRotation(FRotator(0.f, PelvisYaw, 0.f));
		}

		// Clothing follows torso
		if (CollarMesh) CollarMesh->SetRelativeLocation(FVector(2.f, HipSway, 52.f + VertBounce));
		if (BeltMesh)   BeltMesh->SetRelativeLocation(FVector(0.f, HipSway, 20.f + VertBounce * 0.5f));
		if (BeltBuckleMesh) BeltBuckleMesh->SetRelativeLocation(FVector(8.f, HipSway, 20.f + VertBounce * 0.5f));

		// Neck
		if (NeckMesh) NeckMesh->SetRelativeLocation(FVector(0.f, HipSway * 0.5f, 56.f + VertBounce * 0.7f));

		// ---- Head: stabilized + momentum + idle micro-movement ----
		const float HeadYaw = -ChestYaw * 0.3f + IdleHeadYaw;
		const FVector HeadPos(0.f, HipSway * 0.2f + IdleWeightShift * 0.3f,
			72.f + VertBounce * 0.3f);
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
		const FVector FPCamOrigin(0.f, 0.f, 96.f); // matches CameraBoom SocketOffset
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

		// ---- Lightsaber: held in right hand, swing animation ----
		if (SaberHiltMesh && SaberBladeMesh && SaberCoreMesh)
		{
			const bool bShowSaber = (CurrentWeaponType == 2);
			SaberHiltMesh->SetVisibility(bShowSaber);
			SaberEmitterMesh->SetVisibility(bShowSaber);
			SaberPommelMesh->SetVisibility(bShowSaber);
			SaberBladeMesh->SetVisibility(bShowSaber);
			SaberCoreMesh->SetVisibility(bShowSaber);
			if (SaberGlow) SaberGlow->SetVisibility(bShowSaber);

			if (bShowSaber)
			{
				const FVector RHand = bFirstPersonMode
					? FPCamOrigin + FPPitchRot.RotateVector(FVector(25.f, 12.f, -18.f) + FPWeaponOffset)
					: (RightHandMesh
						? RightHandMesh->GetRelativeLocation()
						: FVector(0.f, 26.f, 6.f));

				float HoldPitch = -15.f;

				// Swing animation
				if (SaberSwingTimer > 0.f)
				{
					SaberSwingTimer -= DeltaTime;
					const float MaxSwingTime = 0.35f;
					float Progress = 1.f - FMath::Max(SaberSwingTimer, 0.f) / MaxSwingTime;
					Progress = FMath::InterpEaseOut(0.f, 1.f, Progress, 2.5f);
					HoldPitch = FMath::Lerp(-15.f, -220.f, Progress);
				}

				const FRotator HRot(HoldPitch, 0.f, 0.f);

				// Hilt at hand
				const FVector HiltOff = HRot.RotateVector(FVector(0.f, 0.f, 11.f));
				SaberHiltMesh->SetRelativeLocation(RHand + HiltOff);
				SaberHiltMesh->SetRelativeRotation(HRot);

				// Emitter ring at top of hilt
				const FVector EmitOff = HRot.RotateVector(FVector(0.f, 0.f, 22.f));
				SaberEmitterMesh->SetRelativeLocation(RHand + EmitOff);
				SaberEmitterMesh->SetRelativeRotation(HRot);

				// Pommel at bottom of hilt
				const FVector PomOff = HRot.RotateVector(FVector(0.f, 0.f, -1.f));
				SaberPommelMesh->SetRelativeLocation(RHand + PomOff);
				SaberPommelMesh->SetRelativeRotation(HRot);

				// Blade extends from emitter
				const FVector BladeOff = HRot.RotateVector(FVector(0.f, 0.f, 60.f));
				SaberBladeMesh->SetRelativeLocation(RHand + BladeOff);
				SaberBladeMesh->SetRelativeRotation(HRot);

				// Core (white-hot center) same position as blade
				SaberCoreMesh->SetRelativeLocation(RHand + BladeOff);
				SaberCoreMesh->SetRelativeRotation(HRot);

				// Glow light follows blade center
				if (SaberGlow)
				{
					SaberGlow->SetRelativeLocation(RHand + BladeOff);
					// Subtle flicker
					float Flicker = 4500.f + FMath::Sin(GetWorld()->GetTimeSeconds() * 15.f) * 500.f;
					SaberGlow->SetIntensity(Flicker);
				}
			}
		}

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

		// ---- 50 Cal Sniper Rifle visibility + positioning ----
		if (SniperBodyMesh)
		{
			const bool bShowSniper = (CurrentWeaponType == 6);
			const bool bScoped = bShowSniper && bFirstPersonMode && ADSAlpha > 0.5f;
			const bool bShowMeshes = bShowSniper && !bScoped;
			SniperBodyMesh->SetVisibility(bShowMeshes);
			SniperBarrelMesh->SetVisibility(bShowMeshes);
			SniperScopeMesh->SetVisibility(bShowMeshes);
			SniperStockMesh->SetVisibility(bShowMeshes);
			SniperMagMesh->SetVisibility(bShowMeshes);
			SniperMuzzleMesh->SetVisibility(bShowMeshes && SniperFireTimer > 1.6f);

			if (bShowSniper)
			{
				if (SniperFireTimer > 0.f) SniperFireTimer -= DeltaTime;

				const FVector SniperHip(35.f, 10.f, -22.f);
				const FVector SniperADS(25.f, 0.f, -14.f);
				const FVector GripPt = bFirstPersonMode
					? FPCamOrigin + FPPitchRot.RotateVector(FMath::Lerp(SniperHip, SniperADS, ADSAlpha) + FPWeaponOffset)
					: (RightHandMesh
						? RightHandMesh->GetRelativeLocation()
						: FVector(0.f, 26.f, 6.f));

				const float AimPitch = FPCamPitch;
				const FRotator AimRot(-AimPitch, 0.f, 0.f);
				const FRotator BarrelRot(90.f - AimPitch, 0.f, 0.f);

				// Body/receiver
				SniperBodyMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(10.f, 0.f, 0.f)));
				SniperBodyMesh->SetRelativeRotation(AimRot);

				// Long barrel
				SniperBarrelMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(55.f, 0.f, 0.f)));
				SniperBarrelMesh->SetRelativeRotation(BarrelRot);

				// Scope mounted on top of body
				SniperScopeMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(15.f, 0.f, 6.f)));
				SniperScopeMesh->SetRelativeRotation(BarrelRot);

				// Stock extends behind
				SniperStockMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(-26.f, 0.f, -1.f)));
				SniperStockMesh->SetRelativeRotation(AimRot);

				// Magazine below body
				SniperMagMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(4.f, 0.f, -12.f)));
				SniperMagMesh->SetRelativeRotation(FRotator(5.f - AimPitch, 0.f, 0.f));

				// Muzzle flash at barrel tip
				SniperMuzzleMesh->SetRelativeLocation(GripPt + AimRot.RotateVector(FVector(100.f, 0.f, 0.f)));

				// Muzzle flash emissive
				if (SniperFireTimer > 1.6f && SniperMuzzleMesh->GetNumMaterials() > 0)
				{
					UMaterialInstanceDynamic* MuzzleMID = Cast<UMaterialInstanceDynamic>(
						SniperMuzzleMesh->GetMaterial(0));
					if (MuzzleMID)
					{
						const float Flicker = FMath::RandRange(0.6f, 1.0f);
						MuzzleMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							FLinearColor(12.f * Flicker, 8.f * Flicker, 2.f * Flicker));
					}
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
				else if (CurrentWeaponType == 6 && SniperMuzzleMesh)
					MuzzleFlashLight->SetRelativeLocation(SniperMuzzleMesh->GetRelativeLocation());
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
	//  Flashlight — toggle visibility, aim forward with character
	// =====================================================================
	if (FlashlightSpot)
	{
		FlashlightSpot->SetVisibility(bFlashlightOn);
	}

	// =====================================================================
	//  XP / Kill Streak / Level-up timers
	// =====================================================================
	if (IsLocallyControlled())
	{
		// Passive XP: 1 XP per second while walking
		if (PendingMoveInput.SizeSquared() > 0.01f)
		{
			XPAccumTimer += DeltaTime;
			if (XPAccumTimer >= 1.f)
			{
				AddXP(1);
				XPAccumTimer -= 1.f;
			}
		}

		// Kill streak decay
		if (KillStreakTimer > 0.f)
		{
			KillStreakTimer -= DeltaTime;
			if (KillStreakTimer <= 0.f)
			{
				KillStreakCount = 0;
			}
		}

		// Level-up flash timer decay
		if (LevelUpFlashTimer > 0.f)
		{
			LevelUpFlashTimer -= DeltaTime;
		}

		// Achievement popup timer
		if (AchievementPopupTimer > 0.f)
		{
			AchievementPopupTimer -= DeltaTime;
		}

		// Distance tracking for achievements
		if (PrevTickLocation != FVector::ZeroVector)
		{
			TotalDistanceWalked += FVector::Dist(GetActorLocation(), PrevTickLocation);
		}
		PrevTickLocation = GetActorLocation();

		// Loot crate spin animation
		if (bLootCrateActive)
		{
			LootCrateTimer += DeltaTime;
			LootCrateSpinAngle += DeltaTime * 720.f; // fast spin
			if (LootCrateSpinAngle >= 360.f) LootCrateSpinAngle -= 360.f;

			if (LootCrateTimer >= 2.f) // spin complete after 2s
			{
				bLootCrateActive = false;
				// Award the reward
				AddXP(LootCrateXPReward);
				TotalCoinsCollected += LootCrateReward;
				WeeklyCoins += LootCrateReward;
				if (UWorld* W = GetWorld())
				{
					if (UPHCGameInstance* GI = Cast<UPHCGameInstance>(W->GetGameInstance()))
					{
						GI->LocalPlayerProfile.Coins += LootCrateReward;
						GI->SaveCoinsToBackend();
					}
				}
				CheckAchievements();
			}
		}

		// Limited-time event rotation
		if (CurrentEvent != EActiveEvent::None)
		{
			EventTimer -= DeltaTime;
			if (EventTimer <= 0.f)
			{
				CurrentEvent = EActiveEvent::None;
				EventCooldown = 120.f; // 2 min gap
			}
		}
		else
		{
			EventCooldown -= DeltaTime;
			if (EventCooldown <= 0.f)
			{
				// Pick a random event
				const int32 Pick = FMath::RandRange(0, 2);
				if (Pick == 0) CurrentEvent = EActiveEvent::GoldenHour;
				else if (Pick == 1) CurrentEvent = EActiveEvent::CrabSwarm;
				else CurrentEvent = EActiveEvent::NightHunt;
				EventTimer = 600.f; // 10 min event
			}
		}

		// Periodically check achievements
		CheckAchievements();
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

	// Sensitivity: sniper scoped gets much slower in both 1P and 3P
	float SensMult = 1.f;
	if (CurrentWeaponType == 6 && ADSAlpha > 0.01f)
	{
		// Scale sensitivity with zoom level — more zoom = slower
		const float ZoomRatio = SniperZoomFOV / 90.f; // 10/90 = 0.11 at max zoom
		const float SniperSens = FMath::Lerp(0.15f, 0.04f, FMath::Clamp(1.f - ZoomRatio, 0.f, 1.f));
		SensMult = FMath::Lerp(1.f, SniperSens, ADSAlpha);
	}
	else if (ADSAlpha > 0.01f)
	{
		SensMult = FMath::Lerp(1.f, ADSSensitivityMult, ADSAlpha);
	}

	float YawIn = LookVector.X * SensMult;
	float PitchIn = LookVector.Y * SensMult;

	// Sniper scoped: heavy smoothing + deadzone to kill micro-jitter
	if (CurrentWeaponType == 6 && ADSAlpha > 0.3f)
	{
		// Deadzone: ignore very small inputs (touch jitter)
		const float InputMag = FMath::Sqrt(YawIn * YawIn + PitchIn * PitchIn);
		if (InputMag < 0.005f)
		{
			YawIn = 0.f;
			PitchIn = 0.f;
		}

		// Heavy smoothing — interp speed 3.0 for buttery scope movement
		const float SmoothSpeed = FMath::Lerp(8.f, 3.0f, ADSAlpha);
		ScopedLookSmooth = FMath::Vector2DInterpTo(ScopedLookSmooth, FVector2D(YawIn, PitchIn),
			GetWorld()->GetDeltaSeconds(), SmoothSpeed);
		YawIn = ScopedLookSmooth.X;
		PitchIn = ScopedLookSmooth.Y;
	}
	else
	{
		ScopedLookSmooth = FVector2D::ZeroVector;
	}

	// --- Aim assist (Fortnite-style target magnetism) ---
	// Only active when holding a ranged weapon
	const bool bRanged = (CurrentWeaponType == 0 || CurrentWeaponType == 5 || CurrentWeaponType == 6);
	if (bRanged && GetWorld() && FollowCamera)
	{
		const FVector CamLoc = FollowCamera->GetComponentLocation();
		const FVector CamFwd = FollowCamera->GetForwardVector();
		float BestDot = -1.f;
		FVector BestTargetLoc = FVector::ZeroVector;
		bool bFoundTarget = false;

		// Check all actors with NPCPlayer or enemy tags within range
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Target = *It;
			if (Target == this || Target->IsHidden()) continue;
			if (!Target->Tags.Contains(FName("NPCPlayer")) &&
				!Target->Tags.Contains(FName("GiantCrab"))) continue;

			const FVector TargetLoc = Target->GetActorLocation() + FVector(0.f, 0.f, 50.f);
			const float Dist = FVector::Dist(CamLoc, TargetLoc);
			if (Dist > AimAssistMaxRange || Dist < 100.f) continue;

			const FVector ToTarget = (TargetLoc - CamLoc).GetSafeNormal();
			const float Dot = FVector::DotProduct(CamFwd, ToTarget);
			if (Dot > 0.85f && Dot > BestDot)
			{
				BestDot = Dot;
				BestTargetLoc = TargetLoc;
				bFoundTarget = true;
			}
		}

		if (bFoundTarget)
		{
			// Slowdown: reduce look speed when near a target
			const float SlowMix = FMath::GetMappedRangeValueClamped(
				FVector2D(0.9f, 0.99f), FVector2D(0.f, 1.f), BestDot);
			const float SlowFactor = FMath::Lerp(1.f, AimAssistSlowdown, SlowMix);
			YawIn *= SlowFactor;
			PitchIn *= SlowFactor;

			// Pull: gently rotate toward target
			const float DT = GetWorld()->GetDeltaSeconds();
			const FVector ToTarget = (BestTargetLoc - CamLoc).GetSafeNormal();
			const FRotator CamRot = GetControlRotation();
			const FRotator TargetRot = ToTarget.Rotation();
			const float YawDelta = FRotator::NormalizeAxis(TargetRot.Yaw - CamRot.Yaw);
			const float PitchDelta = FRotator::NormalizeAxis(TargetRot.Pitch - CamRot.Pitch);
			const float PullScale = SlowMix * AimAssistPullStrength * DT;
			YawIn += FMath::Clamp(YawDelta, -3.f, 3.f) * PullScale;
			PitchIn += FMath::Clamp(PitchDelta, -3.f, 3.f) * PullScale;
		}
	}

	AddControllerYawInput(YawIn);
	AddControllerPitchInput(PitchIn);
}

void APHCCityCharacter::HandleJumpStarted(const FInputActionValue& /*Value*/)
{
	if (bClimbingGearEquipped)
	{
		ClimbingVerticalInput = 1.f;
		return;
	}
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
	if (bClimbingGearEquipped) ClimbingVerticalInput = 0.f;
	if (bJetpackEquipped) JetpackVerticalInput = 0.f;
}

void APHCCityCharacter::HandleSprintStarted(const FInputActionValue& /*Value*/)
{
	if (bClimbingGearEquipped)
	{
		ClimbingVerticalInput = -1.f;
		return;
	}
	if (bJetpackEquipped)
	{
		JetpackVerticalInput = -1.f; // descend
		return;
	}
	bIsSprinting = true;
}

void APHCCityCharacter::HandleSprintCompleted(const FInputActionValue& /*Value*/)
{
	if (bClimbingGearEquipped) { ClimbingVerticalInput = 0.f; return; }
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
	else if (CurrentWeaponType == 6 && SniperMuzzleMesh)
		SpawnLoc = SniperMuzzleMesh->GetComponentLocation();
	else
		SpawnLoc = GetActorLocation() + AimDir * 40.f + FVector(0.f, 0.f, 60.f);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (CurrentWeaponType == 0 || CurrentWeaponType == 5 || CurrentWeaponType == 6) // Minigun / AK-47 / Sniper — hitscan
	{
		// Sniper: bolt-action cooldown
		if (CurrentWeaponType == 6)
		{
			if (SniperFireTimer > 0.f) return;
			SniperFireTimer = 1.8f;
		}

		// Fortnite-style spread: base + bloom + movement penalty
		const float BaseSpread = FMath::Lerp(SpreadHipfire, SpreadADS, ADSAlpha);
		const float TotalSpread = FMath::Min(BaseSpread + BloomAccum + BloomMovePenalty, 8.f);
		const float SpreadRad = FMath::DegreesToRadians(TotalSpread);
		const FVector ShotDir = FMath::VRandCone(AimDir, SpreadRad);

		// Accumulate bloom per shot
		BloomAccum = FMath::Min(BloomAccum + BloomPerShot, BloomMax);

		// Hitscan line trace
		const FVector TraceStart = FollowCamera
			? FollowCamera->GetComponentLocation()
			: (GetActorLocation() + FVector(0.f, 0.f, 76.f));
		const float TraceRange = (CurrentWeaponType == 6) ? 500000.f : 50000.f;
		const FVector TraceEnd = TraceStart + ShotDir * TraceRange;

		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(this);
		FHitResult HitResult;
		bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, TraceParams);

		if (bHit && HitResult.GetActor() && HitResult.GetActor()->ActorHasTag(FName("GiantCrab")))
		{
			float BulletDamage = (CurrentWeaponType == 6) ? 999.f : (CurrentWeaponType == 5) ? 12.f : 7.f;
			// Headshot: hit point is in upper 30% of target (Z > 70% of actor height)
			const float HitZ = HitResult.ImpactPoint.Z - HitResult.GetActor()->GetActorLocation().Z;
			if (HitZ > 50.f) BulletDamage *= HeadshotMultiplier;
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
							KillStreakCount++; KillStreakTimer = 5.f;
							AddXP(50);
							StartLootCrate();
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

		// Check NPC helicopter hit (either heli 1 or heli 2)
		if (bHit && HitResult.GetActor() && HitResult.GetActor()->ActorHasTag(FName("NPCHelicopter")))
		{
			const float BulletDamage = (CurrentWeaponType == 6) ? 999.f : (CurrentWeaponType == 5) ? 12.f : 7.f;
			UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
			if (CitySub)
			{
				AActor* HitActor = HitResult.GetActor();

				if (HitActor == CitySub->NPCHelicopter && !CitySub->bNPCHeliCrashing)
				{
					CitySub->NPCHeliHealth -= BulletDamage;
					CitySub->NPCHeliDamageFlashTimer = 0.15f;
					HitMarkerTimer = 0.25f;
					if (CitySub->NPCHeliHealth <= 0.f)
					{
						CitySub->OnNPCHeliDestroyed();
					}
				}
				else if (HitActor == CitySub->NPCHelicopter2 && !CitySub->bNPCHeliCrashing2)
				{
					CitySub->NPCHeliHealth2 -= BulletDamage;
					CitySub->NPCHeliDamageFlashTimer2 = 0.15f;
					HitMarkerTimer = 0.25f;
					if (CitySub->NPCHeliHealth2 <= 0.f)
					{
						CitySub->OnNPCHeli2Destroyed();
					}
				}
			}
		}

		// Check player character hit (PvP)
		if (bHit && HitResult.GetActor() && HitResult.GetActor() != this)
		{
			APHCCityCharacter* HitPlayer = Cast<APHCCityCharacter>(HitResult.GetActor());
			if (HitPlayer)
			{
				float BulletDmg = (CurrentWeaponType == 6) ? 999.f : (CurrentWeaponType == 5) ? 12.f : 7.f;
				const float PvPHitZ = HitResult.ImpactPoint.Z - HitPlayer->GetActorLocation().Z;
				if (PvPHitZ > 50.f) BulletDmg *= HeadshotMultiplier;
				ServerInflictDamage(HitPlayer, BulletDmg);
				HitMarkerTimer = 0.25f;
			}
		}

		// Check NPC player hit
		if (bHit && HitResult.GetActor() && HitResult.GetActor()->ActorHasTag(FName("NPCPlayer")))
		{
			float BulletDamage = (CurrentWeaponType == 6) ? 999.f : (CurrentWeaponType == 5) ? 12.f : 7.f;
			const float NPCHitZ = HitResult.ImpactPoint.Z - HitResult.GetActor()->GetActorLocation().Z;
			if (NPCHitZ > 50.f) BulletDamage *= HeadshotMultiplier;
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

		// Check aircraft hit (Raptor, Blackbird, Black Hawk)
		if (bHit && HitResult.GetActor())
		{
			const float AircraftDmg = (CurrentWeaponType == 6) ? 999.f : (CurrentWeaponType == 5) ? 12.f : 7.f;
			AActor* HitActor = HitResult.GetActor();

			// Try each aircraft type
			APHCRaptorPawn* HitRaptor = Cast<APHCRaptorPawn>(HitActor);
			APHCBlackbirdPawn* HitBlackbird = Cast<APHCBlackbirdPawn>(HitActor);
			APHCBlackHawkPawn* HitBlackHawk = Cast<APHCBlackHawkPawn>(HitActor);

			auto DamageAircraft = [&](auto* Aircraft)
			{
				if (!Aircraft || Aircraft->bIsCrashing) return;
				Aircraft->VehicleHealth -= AircraftDmg;
				Aircraft->DamageFlashTimer = 0.15f;
				HitMarkerTimer = 0.25f;
				if (Aircraft->VehicleHealth <= 0.f)
				{
					// Eject player if occupied
					if (AController* AC = Aircraft->GetController())
					{
						if (APHCPlayerController* PC = Cast<APHCPlayerController>(AC))
						{
							PC->OnVehicleExitRequested();
						}
					}
					Aircraft->bIsCrashing = true;
					Aircraft->bIsOccupied = false;
				}
			};

			if (HitRaptor) DamageAircraft(HitRaptor);
			else if (HitBlackbird) DamageAircraft(HitBlackbird);
			else if (HitBlackHawk) DamageAircraft(HitBlackHawk);
		}

		// Check ground vehicle hit (Aventador, Dune Buggy, Dirt Bike, Tank)
		if (bHit && HitResult.GetActor())
		{
			const float VehDmg = (CurrentWeaponType == 6) ? 999.f : (CurrentWeaponType == 5) ? 12.f : 7.f;
			AActor* HitActor = HitResult.GetActor();

			auto DamageGroundVehicle = [&](auto* Vehicle)
			{
				if (!Vehicle || Vehicle->bIsDestroyed) return;
				Vehicle->VehicleHealth -= VehDmg;
				Vehicle->DamageFlashTimer = 0.15f;
				HitMarkerTimer = 0.25f;
				if (Vehicle->VehicleHealth <= 0.f)
				{
					if (AController* AC = Vehicle->GetController())
					{
						if (APHCPlayerController* PC = Cast<APHCPlayerController>(AC))
						{
							PC->OnVehicleExitRequested();
						}
					}
					Vehicle->bIsDestroyed = true;
					Vehicle->bIsOccupied = false;
				}
			};

			if (APHCVehiclePawn* HitCar = Cast<APHCVehiclePawn>(HitActor)) DamageGroundVehicle(HitCar);
			else if (APHCDuneBuggyPawn* HitBuggy = Cast<APHCDuneBuggyPawn>(HitActor)) DamageGroundVehicle(HitBuggy);
			else if (APHCDirtBikePawn* HitBike = Cast<APHCDirtBikePawn>(HitActor)) DamageGroundVehicle(HitBike);
			else if (APHCTankPawn* HitTank = Cast<APHCTankPawn>(HitActor)) DamageGroundVehicle(HitTank);
		}

		// Visual tracer projectile (cosmetic only)
		APHCMissileProjectile* Bullet = World->SpawnActor<APHCMissileProjectile>(
			APHCMissileProjectile::StaticClass(), SpawnLoc, ShotDir.Rotation(), SP);
		if (Bullet)
		{
			Bullet->OwnerVehicle = this;

			// Minigun: lavender (#cfc0ff) bullet color
			if (CurrentWeaponType == 0 && Bullet->ProjectileMesh)
			{
				UMaterialInstanceDynamic* BulletMat = Cast<UMaterialInstanceDynamic>(
					Bullet->ProjectileMesh->GetMaterial(0));
				if (BulletMat)
				{
					BulletMat->SetVectorParameterValue(TEXT("BaseColor"),
						FLinearColor(0.81f, 0.75f, 1.0f));
					BulletMat->SetVectorParameterValue(TEXT("EmissiveColor"),
						FLinearColor(4.05f, 3.75f, 5.0f));
				}
			}

			// Sniper: fast elongated bullet with smoke trail
			if (CurrentWeaponType == 6 && Bullet->ProjectileMesh)
			{
				// Long tapered bullet shape (stretched along forward axis)
				Bullet->ProjectileMesh->SetRelativeScale3D(FVector(8.f, 0.5f, 0.5f));
				Bullet->Speed = 800000.f; // near-instant — crosses map in < 1 frame
				Bullet->SetLifeSpan(2.f);
				UMaterialInstanceDynamic* BulletMat = Cast<UMaterialInstanceDynamic>(
					Bullet->ProjectileMesh->GetMaterial(0));
				if (BulletMat)
				{
					BulletMat->SetVectorParameterValue(TEXT("BaseColor"),
						FLinearColor(0.61f, 0.19f, 1.0f)); // neon purple
					BulletMat->SetVectorParameterValue(TEXT("EmissiveColor"),
						FLinearColor(3.f, 1.f, 5.f)); // neon purple glow
					BulletMat->SetScalarParameterValue(TEXT("Metallic"), 0.9f);
					BulletMat->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
				}
				Bullet->TrailDotScale = 0.3f;
				Bullet->TrailDotLifespan = 0.8f;
				Bullet->TrailDotInterval = 0.005f;
				Bullet->TrailDotRandomSpread = 2.f;
				Bullet->StartTrail(FLinearColor(0.4f, 0.0f, 0.8f));
			}
		}

		// Recoil: per-shot pitch kick + yaw scatter (works in 1P and 3P)
		if (GetController())
		{
			const float RecoilMult = FMath::Lerp(1.f, 0.4f, ADSAlpha);
			// Sniper: less kick when scoped (the bolt-action cooldown is the balancer)
			const float SniperADSRecoil = FMath::Lerp(1.f, 0.25f, ADSAlpha);
			const float PitchKick = (CurrentWeaponType == 6)
				? FMath::RandRange(0.20f, 0.30f) * SniperADSRecoil
				: FMath::RandRange(0.08f, 0.15f) * RecoilMult;
			AddControllerPitchInput(PitchKick);
			RecoilPitchAccum += PitchKick;

			const float YawKick = (CurrentWeaponType == 6)
				? FMath::RandRange(-0.06f, 0.06f) * SniperADSRecoil
				: FMath::RandRange(-0.05f, 0.05f) * RecoilMult;
			AddControllerYawInput(YawKick);
			RecoilYawAccum += YawKick;

			// Viewmodel kick impulse (1P only)
			if (bFirstPersonMode)
			{
				const float KickScale = (CurrentWeaponType == 6) ? 2.5f : (CurrentWeaponType == 5) ? 1.f : 0.5f;
				WeaponKickZ.Vel += 2.f * RecoilMult * KickScale;
				WeaponKickX.Vel += -1.f * RecoilMult * KickScale;
			}

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
	else if (CurrentWeaponType == 2) // Light Saber — 50 damage per hit (2 hits to kill)
	{
		SaberSwingTimer = 0.35f;
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
							KillStreakCount++; KillStreakTimer = 5.f;
							AddXP(50);
							StartLootCrate();
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
							KillStreakCount++; KillStreakTimer = 5.f;
							AddXP(50);
							StartLootCrate();
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
			KillStreakCount++; KillStreakTimer = 5.f;
			AddXP(50);
			StartLootCrate();

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
	DOREPLIFETIME(APHCCityCharacter, bIsTripping);
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
	if (bClimbingGearEquipped) UnequipClimbingGear();
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
// Roller Blades
// ---------------------------------------------------------------------------

void APHCCityCharacter::EquipRollerBlades()
{
	if (bRollerBladesEquipped) return;
	bRollerBladesEquipped = true;
}

void APHCCityCharacter::UnequipRollerBlades()
{
	if (!bRollerBladesEquipped) return;
	bRollerBladesEquipped = false;
}

// ---------------------------------------------------------------------------
// Climbing Gear
// ---------------------------------------------------------------------------

void APHCCityCharacter::EquipClimbingGear()
{
	if (bClimbingGearEquipped) return;
	if (bJetpackEquipped) UnequipJetpack();
	bClimbingGearEquipped = true;

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CylMesh  = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterial* ColorMat   = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));
	if (!CubeMesh || !CylMesh || !ColorMat) return;

	auto MakePart = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& Scale, const FLinearColor& Col, float Metal, float Rough,
		const FName& Name) -> UStaticMeshComponent*
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
		C->SetMaterial(0, Mat);
		C->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		C->RegisterComponent();
		return C;
	};

	const FLinearColor HarnessCol(0.10f, 0.10f, 0.10f);
	const FLinearColor StrapCol(0.35f, 0.18f, 0.02f);
	const FLinearColor CarabinerCol(0.70f, 0.72f, 0.68f);
	const FLinearColor RopeCol(0.55f, 0.50f, 0.35f);

	ClimbHarnessFrontMesh = MakePart(CubeMesh,
		FVector(14.f, 0.f, 8.f), FRotator::ZeroRotator,
		FVector(0.04f, 0.18f, 0.22f), HarnessCol, 0.2f, 0.75f, TEXT("ClimbHarnessFront"));

	ClimbHarnessBackMesh = MakePart(CubeMesh,
		FVector(-14.f, 0.f, 8.f), FRotator::ZeroRotator,
		FVector(0.04f, 0.18f, 0.22f), HarnessCol, 0.2f, 0.75f, TEXT("ClimbHarnessBack"));

	ClimbHarnessStrapL = MakePart(CubeMesh,
		FVector(0.f, -10.f, 25.f), FRotator(15.f, 0.f, 0.f),
		FVector(0.14f, 0.03f, 0.18f), StrapCol, 0.1f, 0.80f, TEXT("ClimbStrapL"));

	ClimbHarnessStrapR = MakePart(CubeMesh,
		FVector(0.f, 10.f, 25.f), FRotator(15.f, 0.f, 0.f),
		FVector(0.14f, 0.03f, 0.18f), StrapCol, 0.1f, 0.80f, TEXT("ClimbStrapR"));

	ClimbCarabinerL = MakePart(CylMesh,
		FVector(6.f, -14.f, -10.f), FRotator(0.f, 0.f, 90.f),
		FVector(0.025f, 0.025f, 0.03f), CarabinerCol, 0.90f, 0.25f, TEXT("ClimbCarabinerL"));

	ClimbCarabinerR = MakePart(CylMesh,
		FVector(6.f, 14.f, -10.f), FRotator(0.f, 0.f, 90.f),
		FVector(0.025f, 0.025f, 0.03f), CarabinerCol, 0.90f, 0.25f, TEXT("ClimbCarabinerR"));

	ClimbRopeCoilL = MakePart(CylMesh,
		FVector(-8.f, -16.f, -6.f), FRotator(0.f, 0.f, 15.f),
		FVector(0.06f, 0.06f, 0.04f), RopeCol, 0.0f, 0.85f, TEXT("ClimbRopeCoilL"));

	ClimbRopeCoilR = MakePart(CylMesh,
		FVector(-8.f, 16.f, -6.f), FRotator(0.f, 0.f, -15.f),
		FVector(0.06f, 0.06f, 0.04f), RopeCol, 0.0f, 0.85f, TEXT("ClimbRopeCoilR"));
}

void APHCCityCharacter::UnequipClimbingGear()
{
	if (!bClimbingGearEquipped) return;
	bClimbingGearEquipped = false;
	bIsClinging = false;
	ClimbingVerticalInput = 0.f;

	auto Destroy = [](TObjectPtr<UStaticMeshComponent>& C)
	{
		if (C) { C->DestroyComponent(); C = nullptr; }
	};

	Destroy(ClimbHarnessFrontMesh);
	Destroy(ClimbHarnessBackMesh);
	Destroy(ClimbHarnessStrapL);
	Destroy(ClimbHarnessStrapR);
	Destroy(ClimbCarabinerL);
	Destroy(ClimbCarabinerR);
	Destroy(ClimbRopeCoilL);
	Destroy(ClimbRopeCoilR);
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
		CameraBoom->SocketOffset = FVector(0.f, 0.f, 96.f);
		CameraBoom->bEnableCameraLag = false;
		CameraBoom->bEnableCameraRotationLag = false;
		bUseControllerRotationYaw = true;
	}
	else
	{
		CameraBoom->TargetArmLength = 320.f;
		CameraBoom->SocketOffset = FVector(0.f, 75.f, 60.f);
		CameraBoom->bEnableCameraLag = true;
		CameraBoom->CameraLagSpeed = 10.f;
		CameraBoom->bEnableCameraRotationLag = true;
		CameraBoom->CameraRotationLagSpeed = 8.f;
		bUseControllerRotationYaw = false;
	}

	// Show/hide Mixamo skeletal mesh based on camera mode
	if (CharacterSkelMesh) CharacterSkelMesh->SetVisibility(!bFirstPerson);

	// Old body meshes always hidden (Mixamo replaces them)
	HideOldBodyMeshes();

	// Jetpack visibility follows camera mode
	const bool bShow = !bFirstPerson;
	auto SetVis = [bShow](UStaticMeshComponent* M) { if (M) M->SetVisibility(bShow); };
	SetVis(JetpackBodyMesh); SetVis(JetpackNozzleL); SetVis(JetpackNozzleR);
	SetVis(JetpackFlameL); SetVis(JetpackFlameR);
	SetVis(JetpackStrapL); SetVis(JetpackStrapR);
}

void APHCCityCharacter::HideOldBodyMeshes()
{
	auto H = [](UStaticMeshComponent* M) { if (M) M->SetVisibility(false); };
	H(HeadMesh); H(JawMesh); H(NeckMesh);
	H(LeftEyeWhiteMesh); H(RightEyeWhiteMesh);
	H(LeftPupilMesh); H(RightPupilMesh);
	H(NoseMesh); H(MouthMesh);
	H(LeftEarMesh); H(RightEarMesh);
	H(LeftBrowMesh); H(RightBrowMesh);
	H(HairTopMesh); H(HairBackMesh);
	H(HairSideLMesh); H(HairSideRMesh);
	H(ChestMesh); H(AbsMesh); H(PelvisMesh);
	H(CollarMesh); H(BeltMesh); H(BeltBuckleMesh);
	H(LeftShoulderMesh); H(LeftUpperArmMesh);
	H(LeftElbowMesh); H(LeftForearmMesh); H(LeftHandMesh);
	H(RightShoulderMesh); H(RightUpperArmMesh);
	H(RightElbowMesh); H(RightForearmMesh); H(RightHandMesh);
	H(LeftThighMesh); H(LeftKneeMesh); H(LeftShinMesh);
	H(LeftFootMesh); H(LeftToeMesh); H(LeftSoleMesh);
	H(RightThighMesh); H(RightKneeMesh); H(RightShinMesh);
	H(RightFootMesh); H(RightToeMesh); H(RightSoleMesh);
}

// ---------------------------------------------------------------------------
// XP / Leveling System
// ---------------------------------------------------------------------------

bool APHCCityCharacter::AddXP(int32 Amount)
{
	if (Amount <= 0) return false;

	// Apply kill streak multiplier
	const float Mult = KillStreakMultiplier();
	Amount = FMath::RoundToInt(Amount * Mult);

	PlayerXP += Amount;
	bool bLeveledUp = false;

	while (PlayerXP >= XPToNextLevel)
	{
		PlayerXP -= XPToNextLevel;
		PlayerLevel++;
		XPToNextLevel = 100 * PlayerLevel;
		bLeveledUp = true;
	}

	if (bLeveledUp)
	{
		LevelUpFlashTimer = 1.5f; // triggers HUD level-up animation

		// Award coins on level-up (10 * level)
		if (UWorld* World = GetWorld())
		{
			if (UPHCGameInstance* GI = Cast<UPHCGameInstance>(World->GetGameInstance()))
			{
				GI->LocalPlayerProfile.Coins += 10 * PlayerLevel;
				GI->SaveCoinsToBackend();
			}
		}
	}

	return bLeveledUp;
}

// ---------------------------------------------------------------------------
// Kill Streak
// ---------------------------------------------------------------------------

float APHCCityCharacter::KillStreakMultiplier() const
{
	if (KillStreakCount < 2) return 1.f;
	if (KillStreakCount < 4) return 1.5f;
	if (KillStreakCount < 7) return 2.f;
	if (KillStreakCount < 10) return 3.f;
	return 5.f;
}

// ---------------------------------------------------------------------------
// Daily Login Reward
// ---------------------------------------------------------------------------

void APHCCityCharacter::CheckDailyLogin()
{
	// Compute a simple Julian day number from real-world date
	const FDateTime Now = FDateTime::UtcNow();
	const int32 TodayJulian = Now.GetYear() * 400 + Now.GetDayOfYear();

	if (TodayJulian != LastLoginJulianDay)
	{
		// Check if streak continues (consecutive day)
		if (TodayJulian == LastLoginJulianDay + 1)
		{
			// Streak continues — don't increment yet, wait for claim
		}
		else if (LastLoginJulianDay == 0)
		{
			// First ever login
			LoginStreak = 0;
		}
		else
		{
			// Streak broken — reset
			LoginStreak = 0;
		}

		LastLoginJulianDay = TodayJulian;
		bDailyRewardPending = true;
		bDailyRewardClaimed = false;
	}
}

int32 APHCCityCharacter::ClaimDailyReward()
{
	if (bDailyRewardClaimed || !bDailyRewardPending) return 0;

	// 7-day escalating rewards: 5, 10, 20, 40, 80, 150, 300
	static const int32 DailyRewards[] = { 5, 10, 20, 40, 80, 150, 300 };
	const int32 RewardIndex = FMath::Clamp(LoginStreak, 0, 6);
	const int32 CoinsReward = DailyRewards[RewardIndex];

	// Advance streak (wraps after day 7)
	LoginStreak = (LoginStreak + 1) % 7;

	bDailyRewardClaimed = true;
	bDailyRewardPending = false;

	// Award coins
	if (UWorld* World = GetWorld())
	{
		if (UPHCGameInstance* GI = Cast<UPHCGameInstance>(World->GetGameInstance()))
		{
			GI->LocalPlayerProfile.Coins += CoinsReward;
			GI->SaveCoinsToBackend();
		}
	}

	// Also award XP for logging in
	AddXP(25);

	return CoinsReward;
}

// ---------------------------------------------------------------------------
// Loot Crate System
// ---------------------------------------------------------------------------

void APHCCityCharacter::StartLootCrate()
{
	if (bLootCrateActive) return;
	bLootCrateActive = true;
	LootCrateTimer = 0.f;
	LootCrateSpinAngle = 0.f;

	// Weighted random reward: common=5/10, uncommon=20/50, rare=100, epic=200
	const float Roll = FMath::FRand();
	if (Roll < 0.30f)       { LootCrateReward = 5;   LootCrateXPReward = 10; }
	else if (Roll < 0.55f)  { LootCrateReward = 10;  LootCrateXPReward = 20; }
	else if (Roll < 0.75f)  { LootCrateReward = 20;  LootCrateXPReward = 30; }
	else if (Roll < 0.90f)  { LootCrateReward = 50;  LootCrateXPReward = 50; }
	else if (Roll < 0.97f)  { LootCrateReward = 100; LootCrateXPReward = 75; }
	else                    { LootCrateReward = 200; LootCrateXPReward = 150; }

	// Apply event multiplier
	const float Mult = EventCoinMultiplier();
	LootCrateReward = FMath::RoundToInt(LootCrateReward * Mult);
}

// ---------------------------------------------------------------------------
// Achievement System
// ---------------------------------------------------------------------------

const APHCCityCharacter::FAchievement APHCCityCharacter::AchievementDefs[NumAchievements] = {
	{ TEXT("First Blood"),        TEXT("Kill 1 crab"),           1 },
	{ TEXT("Crab Hunter"),        TEXT("Kill 10 crabs"),         10 },
	{ TEXT("Crab Slayer"),        TEXT("Kill 50 crabs"),         50 },
	{ TEXT("Exterminator"),       TEXT("Kill 100 crabs"),        100 },
	{ TEXT("Crab Legend"),        TEXT("Kill 500 crabs"),        500 },
	{ TEXT("Coin Collector"),     TEXT("Collect 50 coins"),      50 },
	{ TEXT("Gold Rush"),          TEXT("Collect 200 coins"),     200 },
	{ TEXT("Treasure Hunter"),    TEXT("Collect 500 coins"),     500 },
	{ TEXT("Leveling Up"),        TEXT("Reach Level 5"),         5 },
	{ TEXT("Getting Serious"),    TEXT("Reach Level 10"),        10 },
	{ TEXT("Veteran"),            TEXT("Reach Level 25"),        25 },
	{ TEXT("Elite"),              TEXT("Reach Level 50"),        50 },
	{ TEXT("Road Warrior"),       TEXT("Drive 3 vehicle types"), 3 },
	{ TEXT("Gearhead"),           TEXT("Drive all 7 vehicles"),  7 },
	{ TEXT("Marathon"),           TEXT("Walk 50,000 UU"),        50000 },
	{ TEXT("Explorer"),           TEXT("Walk 200,000 UU"),       200000 },
	{ TEXT("Globetrotter"),       TEXT("Walk 1,000,000 UU"),     1000000 },
	{ TEXT("Survivor"),           TEXT("Die 10 times"),          10 },
	{ TEXT("Kill Streak x5"),     TEXT("Get a 5-kill streak"),   5 },
	{ TEXT("Unstoppable"),        TEXT("Get a 10-kill streak"),  10 },
};

void APHCCityCharacter::CheckAchievements()
{
	auto TryUnlock = [&](int32 Idx, int32 CurrentVal)
	{
		if (!AchievementUnlocked[Idx] && CurrentVal >= AchievementDefs[Idx].Target)
		{
			AchievementUnlocked[Idx] = true;
			LastUnlockedAchievement = Idx;
			AchievementPopupTimer = 3.f;
			AddXP(50 + Idx * 10); // escalating XP reward

			// Award coins for achievement
			if (UWorld* W = GetWorld())
			{
				if (UPHCGameInstance* GI = Cast<UPHCGameInstance>(W->GetGameInstance()))
				{
					GI->LocalPlayerProfile.Coins += 25 + Idx * 5;
					GI->SaveCoinsToBackend();
				}
			}
		}
	};

	// Count bits set in VehiclesUsed
	int32 VehicleCount = 0;
	for (int32 B = 0; B < 7; ++B)
		if (VehiclesUsed & (1 << B)) VehicleCount++;

	TryUnlock(0, CrabKills);       // 1 kill
	TryUnlock(1, CrabKills);       // 10 kills
	TryUnlock(2, CrabKills);       // 50 kills
	TryUnlock(3, CrabKills);       // 100 kills
	TryUnlock(4, CrabKills);       // 500 kills
	TryUnlock(5, TotalCoinsCollected);  // 50 coins
	TryUnlock(6, TotalCoinsCollected);  // 200 coins
	TryUnlock(7, TotalCoinsCollected);  // 500 coins
	TryUnlock(8, PlayerLevel);     // Level 5
	TryUnlock(9, PlayerLevel);     // Level 10
	TryUnlock(10, PlayerLevel);    // Level 25
	TryUnlock(11, PlayerLevel);    // Level 50
	TryUnlock(12, VehicleCount);   // 3 vehicles
	TryUnlock(13, VehicleCount);   // 7 vehicles
	TryUnlock(14, (int32)TotalDistanceWalked);
	TryUnlock(15, (int32)TotalDistanceWalked);
	TryUnlock(16, (int32)TotalDistanceWalked);
	TryUnlock(17, Deaths);         // 10 deaths
	TryUnlock(18, KillStreakCount); // 5 streak
	TryUnlock(19, KillStreakCount); // 10 streak
}

// ---------------------------------------------------------------------------
// Limited-Time Events
// ---------------------------------------------------------------------------

float APHCCityCharacter::EventCoinMultiplier() const
{
	return (CurrentEvent == EActiveEvent::GoldenHour) ? 3.f : 1.f;
}

// ---------------------------------------------------------------------------
// Vehicle Unlock Progression
// ---------------------------------------------------------------------------

int32 APHCCityCharacter::VehicleUnlockLevel(int32 VehicleType)
{
	switch (VehicleType)
	{
	case 0: return 1;   // Aventador
	case 1: return 3;   // Dune Buggy
	case 4: return 5;   // Dirt Bike
	case 3: return 8;   // Tank
	case 2: return 12;  // Raptor
	case 5: return 18;  // Black Hawk
	case 6: return 25;  // Blackbird (SR-71 uses type 6 in HUD mapping? we'll use index)
	case 7: return 2;   // Roller Blades
	default: return 1;
	}
}

bool APHCCityCharacter::IsVehicleUnlocked(int32 VehicleType) const
{
	// Dev account bypasses level requirements
	if (UWorld* W = GetWorld())
	{
		if (UPHCGameInstance* GI = Cast<UPHCGameInstance>(W->GetGameInstance()))
		{
			if (GI->LocalPlayerProfile.Username == TEXT("Dev")
				|| GI->LocalPlayerProfile.Username == TEXT("Walker"))
				return true;
		}
	}
	return PlayerLevel >= VehicleUnlockLevel(VehicleType);
}

// ---------------------------------------------------------------------------
// Weekly Stats Reset
// ---------------------------------------------------------------------------

void APHCCityCharacter::CheckWeeklyReset()
{
	const FDateTime Now = FDateTime::UtcNow();
	// Simple week key: year * 100 + ISO week
	int32 Year, Month, Day;
	Now.GetDate(Year, Month, Day);
	const int32 DayOfYear = Now.GetDayOfYear();
	const int32 WeekNum = (DayOfYear - 1) / 7;
	const int32 ThisWeek = Year * 100 + WeekNum;

	if (ThisWeek != LastResetWeek)
	{
		WeeklyXP = 0;
		WeeklyKills = 0;
		WeeklyCoins = 0;
		LastResetWeek = ThisWeek;
	}
}
