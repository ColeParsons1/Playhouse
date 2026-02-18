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
// Constructor — Fortnite-quality character from basic shapes
// ---------------------------------------------------------------------------

APHCCityCharacter::APHCCityCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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

	// Inflate bounds to prevent frustum-culling on mobile
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

	// Mark as grounded immediately — ground plane has no collision.
	bHasBeenGrounded = true;

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityCharacter::BeginPlay -- Pawn at %s"),
		*GetActorLocation().ToString());
}

// ---------------------------------------------------------------------------
// Tick — movement, ground trace, animation
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

	// ---- Horizontal movement --------------------------------------------
	const float Speed = bIsSprinting ? SprintSpeed : WalkSpeed;
	if (!MoveDir.IsNearlyZero())
	{
		const FVector Delta = MoveDir * Speed * DeltaTime;
		AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
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
				if (bWasGrounded) ManualVelocityZ = 0.f;
				ManualVelocityZ -= 980.f * DeltaTime;
				float NewZ = Pos.Z + ManualVelocityZ * DeltaTime;
				if (NewZ <= 58.f) { NewZ = 58.f; ManualVelocityZ = 0.f; }
				SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));
			}
			bWasGrounded = bFoundSurface;
		}
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
	//  Walk Animation — natural human gait with spring-damped physics
	//  Features: harmonic sine, torso twist, hip sway, head stabilization,
	//  momentum lean, idle animation, hand/hair spring lag, breathing
	// =====================================================================
	{
		const bool bMoving = !MoveDir.IsNearlyZero();
		const float TargetBlend = bMoving ? 1.f : 0.f;
		WalkAnimBlend = FMath::FInterpTo(WalkAnimBlend, TargetBlend, DeltaTime, 8.f);

		if (bMoving)
		{
			const float CycleSpeed = bIsSprinting ? 16.f : 13.f;
			WalkCyclePhase += CycleSpeed * DeltaTime;
			IdleTimer = 0.f;
		}
		else
		{
			IdleTimer += DeltaTime;
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

		// ---- Scythe: follows right hand, swing animation ----
		if (ScytheHandleMesh && ScytheBladeMesh && ScytheBladeEdgeMesh)
		{
			const bool bShowScythe = (CurrentWeaponType == 3);
			ScytheHandleMesh->SetVisibility(bShowScythe);
			ScytheBladeMesh->SetVisibility(bShowScythe);
			ScytheBladeEdgeMesh->SetVisibility(bShowScythe);

			if (bShowScythe)
			{
				const FVector RHand = RightHandMesh
					? RightHandMesh->GetRelativeLocation()
					: FVector(0.f, 26.f, 6.f);

				// Hold angle: handle extends upward and slightly back
				float HoldPitch = -15.f;

				// Swing arc when attacking
				if (ScytheSwingTimer > 0.f)
				{
					ScytheSwingTimer -= DeltaTime;
					const float MaxSwingTime = 0.45f;
					float Progress = 1.f - FMath::Max(ScytheSwingTimer, 0.f) / MaxSwingTime;
					Progress = FMath::InterpEaseOut(0.f, 1.f, Progress, 2.f);
					HoldPitch = FMath::Lerp(-15.f, 220.f, Progress);
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
				const FVector RHand = RightHandMesh
					? RightHandMesh->GetRelativeLocation()
					: FVector(20.f, 26.f, 40.f);
				const FVector LHand = LeftHandMesh
					? LeftHandMesh->GetRelativeLocation()
					: FVector(22.f, -26.f, 38.f);
				const FVector GripCenter = (RHand + LHand) * 0.5f;

				// Gun points forward (rotated 90 deg so cylinder axis = forward)
				const FRotator GunRot(90.f, 0.f, 0.f);

				// Body: at grip center, extends forward
				MinigunBodyMesh->SetRelativeLocation(GripCenter + FVector(12.f, 0.f, 0.f));
				MinigunBodyMesh->SetRelativeRotation(GunRot);

				// Barrel cluster: in front of body
				MinigunBarrelMesh->SetRelativeLocation(GripCenter + FVector(40.f, 0.f, 0.f));

				// Spin barrels
				if (bMinigunFiring)
				{
					MinigunSpinAngle += 2400.f * DeltaTime;
				}
				else
				{
					MinigunSpinAngle *= FMath::Max(0.f, 1.f - 2.f * DeltaTime);
				}
				MinigunBarrelMesh->SetRelativeRotation(FRotator(90.f, MinigunSpinAngle, 0.f));

				// Muzzle flash at barrel tip
				MinigunMuzzleMesh->SetRelativeLocation(GripCenter + FVector(70.f, 0.f, 0.f));
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
						MinigunFireTimer = 0.06f; // ~16 rounds/sec
						FireWeapon();
					}
				}
				else
				{
					MinigunFireTimer = 0.f;
				}
			}
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
		bHasBeenGrounded = true;
		bWasGrounded = false;
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

	const FVector SpawnLoc = GetActorLocation()
		+ GetActorForwardVector() * 120.f
		+ FVector(0.f, 0.f, 20.f);
	const FRotator SpawnRot = GetControlRotation();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (CurrentWeaponType == 0) // Minigun
	{
		// Random spread for minigun spray
		const float SpreadDeg = 2.5f;
		FRotator SpreadRot = SpawnRot;
		SpreadRot.Yaw   += FMath::RandRange(-SpreadDeg, SpreadDeg);
		SpreadRot.Pitch += FMath::RandRange(-SpreadDeg, SpreadDeg);

		APHCMissileProjectile* Bullet = World->SpawnActor<APHCMissileProjectile>(
			APHCMissileProjectile::StaticClass(), SpawnLoc, SpreadRot, SP);
		if (Bullet)
		{
			Bullet->Speed = 25000.f;
			Bullet->SetLifeSpan(0.8f);
			if (Bullet->ProjectileMesh)
			{
				Bullet->ProjectileMesh->SetRelativeScale3D(FVector(0.6f, 0.08f, 0.08f));
				// Neon purple glow on the round itself
				UMaterial* BulletMat = LoadObject<UMaterial>(nullptr,
					TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));
				if (BulletMat)
				{
					UMaterialInstanceDynamic* BMID = UMaterialInstanceDynamic::Create(BulletMat, Bullet);
					if (BMID)
					{
						BMID->SetVectorParameterValue(TEXT("BaseColor"),
							FLinearColor(0.4f, 0.0f, 0.8f));
						BMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							FLinearColor(3.f, 0.2f, 6.f));
						BMID->SetScalarParameterValue(TEXT("Metallic"), 0.8f);
						BMID->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
						Bullet->ProjectileMesh->SetMaterial(0, BMID);
					}
				}
			}
			Bullet->StartTrail(FLinearColor(0.6f, 0.1f, 1.f)); // neon purple trail
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
	else if (CurrentWeaponType == 3) // Scythe
	{
		// Start swing animation
		ScytheSwingTimer = 0.45f;

		// Wide melee sweep (longer range than sword)
		const FVector SweepStart = GetActorLocation() + FVector(0.f, 0.f, 20.f);
		const FVector SweepEnd = SweepStart + GetActorForwardVector() * 300.f;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, SweepStart, SweepEnd, FQuat::Identity,
			ECC_Pawn, FCollisionShape::MakeSphere(90.f), Params);

		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor())
			{
				UE_LOG(LogPlayhouseCity, Log, TEXT("Scythe hit: %s"), *Hit.GetActor()->GetName());
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
