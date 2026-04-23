// Copyright PlayhouseCity. All Rights Reserved.

#include "PHCTankPawn.h"
#include "Vehicle/PHCNukeExplosion.h"
#include "Audio/PHCAudio.h"
#include "Components/AudioComponent.h"
#include "PlayhouseCity.h"
#include "Net/UnrealNetwork.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Vehicle/PHCMissileProjectile.h"

APHCTankPawn::APHCTankPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 30.f;
	MinNetUpdateFrequency = 15.f;
	bAlwaysRelevant = true;

	// ---- Root collision ----
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(300.f, 180.f, 60.f));
	VehicleCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleCollision->SetCollisionResponseToAllChannels(ECR_Block);
	VehicleCollision->SetSimulatePhysics(false);
	VehicleCollision->SetHiddenInGame(true);
	SetRootComponent(VehicleCollision);

	// ---- Load basic shapes ----
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl  = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Sph  = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// ---- Mesh helper lambdas ----
	auto MakeCube = [&](const TCHAR* Name, const FVector& Loc, const FVector& SizeUU,
		const FRotator& Rot = FRotator::ZeroRotator) -> UStaticMeshComponent*
	{
		auto* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(VehicleCollision);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetStaticMesh(Cube);
		C->SetRelativeScale3D(SizeUU / 100.f);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetCastShadow(true);
		return C;
	};

	auto MakeCyl = [&](const TCHAR* Name, const FVector& Loc, const FVector& Scale,
		const FRotator& Rot = FRotator(0.f, 0.f, 90.f)) -> UStaticMeshComponent*
	{
		auto* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(VehicleCollision);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetStaticMesh(Cyl);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetCastShadow(true);
		return C;
	};

	auto MakeSph = [&](const TCHAR* Name, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		auto* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(VehicleCollision);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetStaticMesh(Sph);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeLocation(Loc);
		C->SetCastShadow(true);
		return C;
	};

	// ================================================================
	//  HULL
	// ================================================================

	// Main hull body — low rectangular box
	HullBodyMesh = MakeCube(TEXT("HullBody"),
		FVector(0.f, 0.f, -10.f), FVector(480.f, 320.f, 80.f));

	// Glacis plate — angled front armor
	GlacisMesh = MakeCube(TEXT("Glacis"),
		FVector(220.f, 0.f, 30.f), FVector(120.f, 310.f, 20.f),
		FRotator(30.f, 0.f, 0.f));

	// Rear plate — angled back
	RearPlateMesh = MakeCube(TEXT("RearPlate"),
		FVector(-240.f, 0.f, 10.f), FVector(30.f, 310.f, 70.f),
		FRotator(-10.f, 0.f, 0.f));

	// Engine deck — flat top rear
	EngineDeckMesh = MakeCube(TEXT("EngineDeck"),
		FVector(-140.f, 0.f, 35.f), FVector(200.f, 300.f, 15.f));

	// Engine grilles — dark vented slots
	EngineGrilleL = MakeCube(TEXT("EngineGrilleL"),
		FVector(-140.f, -60.f, 44.f), FVector(80.f, 40.f, 6.f));
	EngineGrilleR = MakeCube(TEXT("EngineGrilleR"),
		FVector(-140.f, 60.f, 44.f), FVector(80.f, 40.f, 6.f));

	// Hull side skirts
	HullSideL = MakeCube(TEXT("HullSideL"),
		FVector(0.f, -170.f, -25.f), FVector(500.f, 20.f, 60.f));
	HullSideR = MakeCube(TEXT("HullSideR"),
		FVector(0.f, 170.f, -25.f), FVector(500.f, 20.f, 60.f));

	// Fenders — flat overhangs above tracks
	FenderL = MakeCube(TEXT("FenderL"),
		FVector(20.f, -170.f, 30.f), FVector(520.f, 50.f, 8.f));
	FenderR = MakeCube(TEXT("FenderR"),
		FVector(20.f, 170.f, 30.f), FVector(520.f, 50.f, 8.f));

	// ================================================================
	//  TURRET (all attach to TurretPivot for independent rotation)
	// ================================================================

	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(VehicleCollision);
	TurretPivot->SetRelativeLocation(FVector(30.f, 0.f, 40.f));

	// Helper: turret-attached cube
	auto MakeTurretCube = [&](const TCHAR* Name, const FVector& Loc, const FVector& SizeUU,
		const FRotator& Rot = FRotator::ZeroRotator) -> UStaticMeshComponent*
	{
		auto* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(TurretPivot);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetStaticMesh(Cube);
		C->SetRelativeScale3D(SizeUU / 100.f);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetCastShadow(true);
		return C;
	};

	auto MakeTurretCyl = [&](const TCHAR* Name, const FVector& Loc, const FVector& Scale,
		const FRotator& Rot = FRotator(0.f, 0.f, 90.f)) -> UStaticMeshComponent*
	{
		auto* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(TurretPivot);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetStaticMesh(Cyl);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetCastShadow(true);
		return C;
	};

	auto MakeTurretSph = [&](const TCHAR* Name, const FVector& Loc,
		const FVector& Scale) -> UStaticMeshComponent*
	{
		auto* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(TurretPivot);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetStaticMesh(Sph);
		C->SetRelativeScale3D(Scale);
		C->SetRelativeLocation(Loc);
		C->SetCastShadow(true);
		return C;
	};

	// Turret base — squashed sphere for the distinctive M60 rounded shape
	TurretBaseMesh = MakeTurretSph(TEXT("TurretBase"),
		FVector(0.f, 0.f, 30.f), FVector(2.0f, 1.8f, 0.8f));

	// Turret bustle — boxy rear overhang behind turret
	TurretBustleMesh = MakeTurretCube(TEXT("TurretBustle"),
		FVector(-80.f, 0.f, 30.f), FVector(100.f, 150.f, 55.f));

	// Commander's cupola — cylinder on top
	CupolaMesh = MakeTurretCyl(TEXT("Cupola"),
		FVector(-20.f, -30.f, 72.f), FVector(0.35f, 0.35f, 0.15f),
		FRotator::ZeroRotator);

	// Cupola hatch — thin disc
	CupolaHatchMesh = MakeTurretCyl(TEXT("CupolaHatch"),
		FVector(-20.f, -30.f, 87.f), FVector(0.30f, 0.30f, 0.03f),
		FRotator::ZeroRotator);

	// Mantlet — gun shield at front of turret
	MantletMesh = MakeTurretCube(TEXT("Mantlet"),
		FVector(90.f, 0.f, 25.f), FVector(40.f, 80.f, 50.f));

	// ================================================================
	//  MAIN GUN (105mm M68) — attached to TurretPivot
	// ================================================================

	// Gun barrel — long thin cylinder pointing forward
	GunBarrelMesh = MakeTurretCyl(TEXT("GunBarrel"),
		FVector(280.f, 0.f, 28.f), FVector(0.08f, 0.08f, 2.2f),
		FRotator(0.f, 0.f, 0.f));

	// Bore evacuator — wider cylinder bulge partway along barrel
	BoreEvacuatorMesh = MakeTurretCyl(TEXT("BoreEvacuator"),
		FVector(220.f, 0.f, 28.f), FVector(0.14f, 0.14f, 0.20f),
		FRotator(0.f, 0.f, 0.f));

	// Muzzle brake — slightly wider at tip
	MuzzleBrakeMesh = MakeTurretCyl(TEXT("MuzzleBrake"),
		FVector(390.f, 0.f, 28.f), FVector(0.10f, 0.10f, 0.12f),
		FRotator(0.f, 0.f, 0.f));

	// Gun cradle — connects gun to mantlet
	GunCradleMesh = MakeTurretCube(TEXT("GunCradle"),
		FVector(115.f, 0.f, 28.f), FVector(30.f, 25.f, 25.f));

	// Muzzle flash — hidden by default, shown briefly on fire
	MuzzleFlashMesh = MakeTurretSph(TEXT("MuzzleFlash"),
		FVector(410.f, 0.f, 28.f), FVector(0.5f, 0.5f, 0.5f));
	MuzzleFlashMesh->SetVisibility(false);

	// ================================================================
	//  TRACK ASSEMBLY — LEFT SIDE
	// ================================================================

	// Track body — long flat box along side
	TrackBodyL = MakeCube(TEXT("TrackBodyL"),
		FVector(0.f, -175.f, -55.f), FVector(530.f, 30.f, 20.f));

	// Drive sprocket (rear)
	DriveSprocketL = MakeCyl(TEXT("DriveSprocketL"),
		FVector(-230.f, -175.f, -48.f), FVector(0.35f, 0.35f, 0.18f));

	// Idler wheel (front)
	IdlerWheelL = MakeCyl(TEXT("IdlerWheelL"),
		FVector(230.f, -175.f, -48.f), FVector(0.30f, 0.30f, 0.18f));

	// 6 road wheels evenly spaced
	for (int32 i = 0; i < 6; ++i)
	{
		const float X = FMath::Lerp(-180.f, 180.f, i / 5.f);
		FName WName = *FString::Printf(TEXT("RoadWheelL%d"), i);
		auto* W = MakeCyl(*WName.ToString(), FVector(X, -175.f, -58.f), FVector(0.28f, 0.28f, 0.15f));
		RoadWheelsL.Add(W);
	}

	// 3 return rollers on top
	for (int32 i = 0; i < 3; ++i)
	{
		const float X = FMath::Lerp(-120.f, 120.f, i / 2.f);
		FName RName = *FString::Printf(TEXT("ReturnRollerL%d"), i);
		auto* R = MakeCyl(*RName.ToString(), FVector(X, -175.f, -30.f), FVector(0.12f, 0.12f, 0.12f));
		ReturnRollersL.Add(R);
	}

	// ================================================================
	//  TRACK ASSEMBLY — RIGHT SIDE (mirrored)
	// ================================================================

	TrackBodyR = MakeCube(TEXT("TrackBodyR"),
		FVector(0.f, 175.f, -55.f), FVector(530.f, 30.f, 20.f));

	DriveSprocketR = MakeCyl(TEXT("DriveSprocketR"),
		FVector(-230.f, 175.f, -48.f), FVector(0.35f, 0.35f, 0.18f));

	IdlerWheelR = MakeCyl(TEXT("IdlerWheelR"),
		FVector(230.f, 175.f, -48.f), FVector(0.30f, 0.30f, 0.18f));

	for (int32 i = 0; i < 6; ++i)
	{
		const float X = FMath::Lerp(-180.f, 180.f, i / 5.f);
		FName WName = *FString::Printf(TEXT("RoadWheelR%d"), i);
		auto* W = MakeCyl(*WName.ToString(), FVector(X, 175.f, -58.f), FVector(0.28f, 0.28f, 0.15f));
		RoadWheelsR.Add(W);
	}

	for (int32 i = 0; i < 3; ++i)
	{
		const float X = FMath::Lerp(-120.f, 120.f, i / 2.f);
		FName RName = *FString::Printf(TEXT("ReturnRollerR%d"), i);
		auto* R = MakeCyl(*RName.ToString(), FVector(X, 175.f, -30.f), FVector(0.12f, 0.12f, 0.12f));
		ReturnRollersR.Add(R);
	}

	// ================================================================
	//  DETAILS
	// ================================================================

	// Headlights — small spheres on front hull
	HeadlightMeshL = MakeSph(TEXT("HeadlightL"),
		FVector(245.f, -80.f, 20.f), FVector(0.12f, 0.12f, 0.12f));
	HeadlightMeshR = MakeSph(TEXT("HeadlightR"),
		FVector(245.f, 80.f, 20.f), FVector(0.12f, 0.12f, 0.12f));

	// Antenna — thin tall cylinder on turret rear (attached to turret)
	AntennaMesh = MakeTurretCyl(TEXT("Antenna"),
		FVector(-90.f, 40.f, 100.f), FVector(0.01f, 0.01f, 1.0f),
		FRotator::ZeroRotator);

	// Tool stowage — thin box on right hull side
	ToolStowageMesh = MakeCube(TEXT("ToolStowage"),
		FVector(50.f, 185.f, 10.f), FVector(120.f, 8.f, 20.f));

	// Smoke grenade launchers — small cylinders on turret sides
	SmokeLauncherL = MakeTurretCyl(TEXT("SmokeLauncherL"),
		FVector(30.f, -100.f, 40.f), FVector(0.06f, 0.06f, 0.20f),
		FRotator(0.f, 90.f, 0.f));
	SmokeLauncherR = MakeTurretCyl(TEXT("SmokeLauncherR"),
		FVector(30.f, 100.f, 40.f), FVector(0.06f, 0.06f, 0.20f),
		FRotator(0.f, 90.f, 0.f));

	// ================================================================
	//  HEADLIGHT SPOTLIGHTS
	// ================================================================

	HeadlightSpotL = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotL"));
	HeadlightSpotL->SetupAttachment(VehicleCollision);
	HeadlightSpotL->SetRelativeLocation(FVector(260.f, -80.f, 20.f));
	HeadlightSpotL->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
	HeadlightSpotL->SetIntensity(60000.f);
	HeadlightSpotL->SetAttenuationRadius(5000.f);
	HeadlightSpotL->SetOuterConeAngle(28.f);
	HeadlightSpotL->SetInnerConeAngle(16.f);
	HeadlightSpotL->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotL->SetVisibility(false);

	HeadlightSpotR = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightSpotR"));
	HeadlightSpotR->SetupAttachment(VehicleCollision);
	HeadlightSpotR->SetRelativeLocation(FVector(260.f, 80.f, 20.f));
	HeadlightSpotR->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
	HeadlightSpotR->SetIntensity(60000.f);
	HeadlightSpotR->SetAttenuationRadius(5000.f);
	HeadlightSpotR->SetOuterConeAngle(28.f);
	HeadlightSpotR->SetInnerConeAngle(16.f);
	HeadlightSpotR->SetLightColor(FColor(255, 250, 240));
	HeadlightSpotR->SetVisibility(false);

	// ================================================================
	//  CAMERA
	// ================================================================

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleCollision);
	CameraBoom->TargetArmLength = 900.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 250.f);
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
// BeginPlay — materials, input, bounds
// ---------------------------------------------------------------------------

void APHCTankPawn::BeginPlay()
{
	Super::BeginPlay();

	EngineAC = PHCAudio::SpawnAttached3DLoop(VehicleCollision,
		TEXT("/Game/Audio/SFX/Vehicles/TankIdle_Loop.TankIdle_Loop"),
		0.f, 0.7f);

	CreateInputAssets();

	// Load materials
	UMaterial* ColorMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));

	auto ApplyMat = [&](UStaticMeshComponent* Comp, const FLinearColor& Color,
		float Metallic, float Roughness,
		const FLinearColor& Emissive = FLinearColor::Black)
	{
		if (!Comp || !ColorMat) return;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create((UMaterialInterface*)ColorMat, Comp);
		if (!M) return;
		M->SetVectorParameterValue(TEXT("BaseColor"), Color);
		M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		if (Emissive.R > 0.001f || Emissive.G > 0.001f || Emissive.B > 0.001f)
			M->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
		Comp->SetMaterial(0, M);
	};

	// Color palette
	const FLinearColor OliveDrab(0.22f, 0.24f, 0.14f);
	const FLinearColor DarkSteel(0.10f, 0.10f, 0.12f);
	const FLinearColor TrackDark(0.06f, 0.06f, 0.06f);
	const FLinearColor RubberBlack(0.08f, 0.08f, 0.08f);
	const FLinearColor GrilleDark(0.04f, 0.04f, 0.04f);
	const FLinearColor HeadlightOff(0.3f, 0.3f, 0.25f);

	// Hull — olive drab matte
	ApplyMat(HullBodyMesh, OliveDrab, 0.3f, 0.85f);
	ApplyMat(GlacisMesh, OliveDrab, 0.3f, 0.85f);
	ApplyMat(RearPlateMesh, OliveDrab, 0.3f, 0.85f);
	ApplyMat(EngineDeckMesh, OliveDrab, 0.3f, 0.85f);
	ApplyMat(HullSideL, OliveDrab, 0.3f, 0.85f);
	ApplyMat(HullSideR, OliveDrab, 0.3f, 0.85f);
	ApplyMat(FenderL, OliveDrab, 0.3f, 0.85f);
	ApplyMat(FenderR, OliveDrab, 0.3f, 0.85f);

	// Engine grilles — dark
	ApplyMat(EngineGrilleL, GrilleDark, 0.5f, 0.6f);
	ApplyMat(EngineGrilleR, GrilleDark, 0.5f, 0.6f);

	// Turret — olive drab
	ApplyMat(TurretBaseMesh, OliveDrab, 0.3f, 0.85f);
	ApplyMat(TurretBustleMesh, OliveDrab, 0.3f, 0.85f);
	ApplyMat(CupolaMesh, OliveDrab, 0.35f, 0.80f);
	ApplyMat(CupolaHatchMesh, OliveDrab, 0.35f, 0.80f);
	ApplyMat(MantletMesh, OliveDrab, 0.4f, 0.75f);

	// Gun — dark steel
	ApplyMat(GunBarrelMesh, DarkSteel, 0.7f, 0.4f);
	ApplyMat(BoreEvacuatorMesh, DarkSteel, 0.65f, 0.45f);
	ApplyMat(MuzzleBrakeMesh, DarkSteel, 0.7f, 0.4f);
	ApplyMat(GunCradleMesh, DarkSteel, 0.6f, 0.5f);

	// Muzzle flash — bright emissive (only visible briefly)
	ApplyMat(MuzzleFlashMesh, FLinearColor(1.f, 0.8f, 0.3f), 0.f, 1.f,
		FLinearColor(8.f, 5.f, 1.f));

	// Tracks — dark metallic
	ApplyMat(TrackBodyL, TrackDark, 0.5f, 0.7f);
	ApplyMat(TrackBodyR, TrackDark, 0.5f, 0.7f);

	// Sprockets and idlers — dark steel
	ApplyMat(DriveSprocketL, DarkSteel, 0.6f, 0.5f);
	ApplyMat(DriveSprocketR, DarkSteel, 0.6f, 0.5f);
	ApplyMat(IdlerWheelL, DarkSteel, 0.6f, 0.5f);
	ApplyMat(IdlerWheelR, DarkSteel, 0.6f, 0.5f);

	// Road wheels — rubber
	for (UStaticMeshComponent* W : RoadWheelsL) ApplyMat(W, RubberBlack, 0.0f, 0.9f);
	for (UStaticMeshComponent* W : RoadWheelsR) ApplyMat(W, RubberBlack, 0.0f, 0.9f);

	// Return rollers — dark steel
	for (UStaticMeshComponent* R : ReturnRollersL) ApplyMat(R, DarkSteel, 0.5f, 0.6f);
	for (UStaticMeshComponent* R : ReturnRollersR) ApplyMat(R, DarkSteel, 0.5f, 0.6f);

	// Details
	ApplyMat(HeadlightMeshL, HeadlightOff, 0.2f, 0.5f);
	ApplyMat(HeadlightMeshR, HeadlightOff, 0.2f, 0.5f);
	ApplyMat(AntennaMesh, DarkSteel, 0.3f, 0.6f);
	ApplyMat(ToolStowageMesh, OliveDrab, 0.2f, 0.9f);
	ApplyMat(SmokeLauncherL, DarkSteel, 0.5f, 0.5f);
	ApplyMat(SmokeLauncherR, DarkSteel, 0.5f, 0.5f);

	// Store olive drab meshes for SetBodyColor
	OliveDrabMeshes = {
		HullBodyMesh, GlacisMesh, RearPlateMesh, EngineDeckMesh,
		HullSideL, HullSideR, FenderL, FenderR,
		TurretBaseMesh, TurretBustleMesh, CupolaMesh, CupolaHatchMesh,
		MantletMesh, ToolStowageMesh
	};

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
// Tick — physics, turret, animation, cannon cooldown
// ---------------------------------------------------------------------------

void APHCTankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (EngineAC)
	{
		const float SpeedRatio = FMath::Clamp(FMath::Abs(CurrentSpeed) / FMath::Max(MaxSpeed, 1.f), 0.f, 1.f);
		EngineAC->SetPitchMultiplier(FMath::Lerp(0.6f, 1.2f, SpeedRatio));
		EngineAC->SetVolumeMultiplier(bIsOccupied ? FMath::Lerp(0.5f, 1.0f, SpeedRatio) : 0.f);
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

	// Cooldowns tick regardless of occupancy
	FireCooldownRemaining = FMath::Max(0.f, FireCooldownRemaining - DeltaTime);

	// Muzzle flash fade
	if (MuzzleFlashTimer > 0.f)
	{
		MuzzleFlashTimer -= DeltaTime;
		if (MuzzleFlashTimer <= 0.f)
		{
			if (MuzzleFlashMesh) MuzzleFlashMesh->SetVisibility(false);
		}
	}

	// Recoil spring-back
	if (FMath::Abs(RecoilOffset) > 0.1f)
	{
		RecoilOffset = FMath::FInterpTo(RecoilOffset, 0.f, DeltaTime, 8.f);
		if (GunBarrelMesh)
			GunBarrelMesh->SetRelativeLocation(FVector(280.f + RecoilOffset, 0.f, 28.f));
		if (BoreEvacuatorMesh)
			BoreEvacuatorMesh->SetRelativeLocation(FVector(220.f + RecoilOffset, 0.f, 28.f));
		if (MuzzleBrakeMesh)
			MuzzleBrakeMesh->SetRelativeLocation(FVector(390.f + RecoilOffset, 0.f, 28.f));
		if (MuzzleFlashMesh)
			MuzzleFlashMesh->SetRelativeLocation(FVector(410.f + RecoilOffset, 0.f, 28.f));
	}

	if (!bIsOccupied) return;

	// ---- Speed update (quadratic acceleration falloff) ----
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

	// ---- Steering (speed-responsive) ----
	if (FMath::Abs(CurrentSpeed) > 10.f)
	{
		const float NormSpeed = FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxSpeed, 0.f, 1.f);
		const float SpeedFactor = FMath::Lerp(1.0f, 0.4f, NormSpeed);
		const float YawDelta = SteeringInput * SteeringSpeed * SpeedFactor * DeltaTime;
		FRotator NewRotation = GetActorRotation();
		NewRotation.Yaw += YawDelta;
		SetActorRotation(NewRotation);
	}

	// ---- Movement ----
	if (FMath::Abs(CurrentSpeed) > 1.f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Delta = Forward * CurrentSpeed * DeltaTime;
		AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.f), false);
	}

	// ---- Turret rotation ----
	if (FMath::Abs(TurretYawInput) > 0.01f)
	{
		TurretYaw += TurretYawInput * TurretRotSpeed * DeltaTime;
	}
	if (TurretPivot)
	{
		TurretPivot->SetRelativeRotation(FRotator(0.f, TurretYaw, 0.f));
	}

	// ---- Ground following ----
	const FVector Pos = GetActorLocation();

	// Tunnel / airport zone check
	const bool bInTunnel = (Pos.X >= 102000.f && Pos.X <= 116000.f &&
		Pos.Y >= 28000.f && Pos.Y <= 32000.f);
	const bool bInAirport = (Pos.X >= 100000.f && Pos.X <= 200000.f &&
		Pos.Y >= -10000.f && Pos.Y <= 70000.f && Pos.Z < 60.f);
	const float FootOffset = 60.f;

	if (bInTunnel || bInAirport)
	{
		float TargetZ;
		if (bInTunnel)
		{
			const float Alpha = (Pos.X - 102000.f) / (116000.f - 102000.f);
			TargetZ = FMath::Lerp(-8000.f + FootOffset, 60.f, Alpha);
			if (Pos.Z < TargetZ - 200.f) TargetZ = -8000.f + FootOffset;
		}
		else
		{
			TargetZ = -8000.f + FootOffset;
		}
		VerticalVelocity = 0.f;
		bHasBeenGrounded = true;
		bWasGrounded = true;
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
				const float TargetZ = Hit.ImpactPoint.Z + FootOffset;
				if (TargetZ >= Pos.Z - 200.f)
				{
					bFoundSurface = true;
					bHasBeenGrounded = true;
					VerticalVelocity = 0.f;
					const float NewZ = FMath::FInterpTo(Pos.Z, TargetZ, DeltaTime, 12.f);
					SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));

					// Slope pitch
					const FVector Fwd2D = FVector(GetActorForwardVector().X,
						GetActorForwardVector().Y, 0.f).GetSafeNormal();
					const float ForwardSlope = -(Hit.ImpactNormal.X * Fwd2D.X +
						Hit.ImpactNormal.Y * Fwd2D.Y) /
						FMath::Max(Hit.ImpactNormal.Z, 0.01f);
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
			if (NewZ <= FootOffset) { NewZ = FootOffset; VerticalVelocity = 0.f; }
			SetActorLocation(FVector(Pos.X, Pos.Y, NewZ));

			FRotator Rot = GetActorRotation();
			Rot.Pitch = FMath::FInterpTo(Rot.Pitch, 0.f, DeltaTime, 3.f);
			SetActorRotation(Rot);
		}
		bWasGrounded = bFoundSurface;
	}

	// ---- Wheel spin animation ----
	WheelSpinAngle += CurrentSpeed * DeltaTime * 0.5f;
	const FRotator WheelRot(WheelSpinAngle, 0.f, 90.f);

	if (DriveSprocketL) DriveSprocketL->SetRelativeRotation(WheelRot);
	if (DriveSprocketR) DriveSprocketR->SetRelativeRotation(WheelRot);
	if (IdlerWheelL) IdlerWheelL->SetRelativeRotation(WheelRot);
	if (IdlerWheelR) IdlerWheelR->SetRelativeRotation(WheelRot);
	for (UStaticMeshComponent* W : RoadWheelsL) if (W) W->SetRelativeRotation(WheelRot);
	for (UStaticMeshComponent* W : RoadWheelsR) if (W) W->SetRelativeRotation(WheelRot);
	for (UStaticMeshComponent* R : ReturnRollersL) if (R) R->SetRelativeRotation(WheelRot);
	for (UStaticMeshComponent* R : ReturnRollersR) if (R) R->SetRelativeRotation(WheelRot);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void APHCTankPawn::CreateInputAssets()
{
	ThrottleAction = NewObject<UInputAction>(this, TEXT("IA_TankThrottle"));
	ThrottleAction->ValueType = EInputActionValueType::Boolean;

	BrakeAction = NewObject<UInputAction>(this, TEXT("IA_TankBrake"));
	BrakeAction->ValueType = EInputActionValueType::Boolean;

	SteerAction = NewObject<UInputAction>(this, TEXT("IA_TankSteer"));
	SteerAction->ValueType = EInputActionValueType::Axis1D;

	ExitVehicleAction = NewObject<UInputAction>(this, TEXT("IA_TankExit"));
	ExitVehicleAction->ValueType = EInputActionValueType::Boolean;

	FireCannonAction = NewObject<UInputAction>(this, TEXT("IA_TankFire"));
	FireCannonAction->ValueType = EInputActionValueType::Boolean;

	TurretLookAction = NewObject<UInputAction>(this, TEXT("IA_TankTurretLook"));
	TurretLookAction->ValueType = EInputActionValueType::Axis1D;

	VehicleMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Tank"));

	// Throttle: W, Up, RT
	VehicleMappingContext->MapKey(ThrottleAction, EKeys::W);
	VehicleMappingContext->MapKey(ThrottleAction, EKeys::Up);

	// Brake: S, Down, LT
	VehicleMappingContext->MapKey(BrakeAction, EKeys::S);
	VehicleMappingContext->MapKey(BrakeAction, EKeys::Down);

	// Steer right: D, Right
	VehicleMappingContext->MapKey(SteerAction, EKeys::D);
	VehicleMappingContext->MapKey(SteerAction, EKeys::Right);

	// Steer left: A, Left (negated)
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

	// Fire cannon: Space
	VehicleMappingContext->MapKey(FireCannonAction, EKeys::SpaceBar);

	// Turret look: Mouse X
	VehicleMappingContext->MapKey(TurretLookAction, EKeys::Mouse2D);

	// Gamepad
	VehicleMappingContext->MapKey(ThrottleAction, EKeys::Gamepad_RightTrigger);
	VehicleMappingContext->MapKey(BrakeAction, EKeys::Gamepad_LeftTrigger);
	VehicleMappingContext->MapKey(SteerAction, EKeys::Gamepad_LeftStick_Right);
	{
		FEnhancedActionKeyMapping& M = VehicleMappingContext->MapKey(SteerAction, EKeys::Gamepad_LeftStick_Left);
		UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(this);
		M.Modifiers.Add(Neg);
	}
	VehicleMappingContext->MapKey(FireCannonAction, EKeys::Gamepad_FaceButton_Bottom);
	VehicleMappingContext->MapKey(ExitVehicleAction, EKeys::Gamepad_FaceButton_Left);
}

void APHCTankPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (VehicleMappingContext) Sub->AddMappingContext(VehicleMappingContext, 1);
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (ThrottleAction)
	{
		EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &APHCTankPawn::HandleThrottle);
		EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &APHCTankPawn::HandleThrottleRelease);
	}
	if (BrakeAction)
	{
		EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &APHCTankPawn::HandleBrake);
		EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &APHCTankPawn::HandleBrakeRelease);
	}
	if (SteerAction)
	{
		EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &APHCTankPawn::HandleSteer);
		EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &APHCTankPawn::HandleSteerRelease);
	}
	if (ExitVehicleAction)
	{
		EIC->BindAction(ExitVehicleAction, ETriggerEvent::Started, this, &APHCTankPawn::HandleExitVehicle);
	}
	if (FireCannonAction)
	{
		EIC->BindAction(FireCannonAction, ETriggerEvent::Started, this, &APHCTankPawn::HandleFireCannon);
	}
	if (TurretLookAction)
	{
		EIC->BindAction(TurretLookAction, ETriggerEvent::Triggered, this, &APHCTankPawn::HandleTurretLook);
	}
}

void APHCTankPawn::HandleThrottle(const FInputActionValue&)
{
	ThrottleInput = 1.f;
	if (!HasAuthority()) ServerSetThrottle(1.f);
}

void APHCTankPawn::HandleThrottleRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
	if (!HasAuthority()) ServerSetThrottle(0.f);
}

void APHCTankPawn::HandleBrake(const FInputActionValue&)
{
	ThrottleInput = -1.f;
	if (!HasAuthority()) ServerSetThrottle(-1.f);
}

void APHCTankPawn::HandleBrakeRelease(const FInputActionValue&)
{
	ThrottleInput = 0.f;
	if (!HasAuthority()) ServerSetThrottle(0.f);
}

void APHCTankPawn::HandleSteer(const FInputActionValue& Value)
{
	SteeringInput = Value.Get<float>();
	if (!HasAuthority()) ServerSetSteering(SteeringInput);
}

void APHCTankPawn::HandleSteerRelease(const FInputActionValue&)
{
	SteeringInput = 0.f;
	if (!HasAuthority()) ServerSetSteering(0.f);
}

void APHCTankPawn::HandleExitVehicle(const FInputActionValue&)
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (VehicleMappingContext) Sub->RemoveMappingContext(VehicleMappingContext);
		}

		static const FName ExitFuncName(TEXT("OnVehicleExitRequested"));
		if (UFunction* Func = PC->FindFunction(ExitFuncName))
		{
			PC->ProcessEvent(Func, nullptr);
		}
	}
}

void APHCTankPawn::HandleFireCannon(const FInputActionValue&)
{
	FireCannon();
}

void APHCTankPawn::HandleTurretLook(const FInputActionValue& Value)
{
	const float LookX = Value.Get<float>();
	TurretYawInput = FMath::Clamp(LookX * 0.3f, -1.f, 1.f);
	if (!HasAuthority()) ServerSetTurretYaw(TurretYawInput);
}

// ---------------------------------------------------------------------------
// Cannon
// ---------------------------------------------------------------------------

void APHCTankPawn::FireCannon()
{
	if (FireCooldownRemaining > 0.f) return;
	FireCooldownRemaining = FireCooldown;

	if (!HasAuthority())
	{
		ServerFireCannon();
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// Get muzzle world location from the muzzle brake tip
	FVector MuzzleLoc = GetActorLocation() + FVector(0.f, 0.f, 40.f + 28.f); // turret height + gun height
	if (MuzzleBrakeMesh)
	{
		MuzzleLoc = MuzzleBrakeMesh->GetComponentLocation()
			+ MuzzleBrakeMesh->GetForwardVector() * 20.f;
	}

	// Turret forward direction
	const float WorldTurretYaw = GetActorRotation().Yaw + TurretYaw;
	const FRotator MuzzleRot(0.f, WorldTurretYaw, 0.f);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APHCMissileProjectile* Proj = World->SpawnActor<APHCMissileProjectile>(
		APHCMissileProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
	if (Proj)
	{
		Proj->OwnerVehicle = this;
		// Tank shell velocity — set the manual velocity for the projectile
		const FVector Dir = MuzzleRot.Vector();
		Proj->bUseManualGravity = true;
		Proj->ManualVelocity = Dir * 15000.f;
		Proj->ManualGravityScale = 0.3f; // slight drop over distance
	}

	// Muzzle flash
	MuzzleFlashTimer = 0.15f;
	if (MuzzleFlashMesh) MuzzleFlashMesh->SetVisibility(true);

	// Recoil
	RecoilOffset = -25.f;
}

// ---------------------------------------------------------------------------
// Server RPCs
// ---------------------------------------------------------------------------

void APHCTankPawn::ServerFireCannon_Implementation()
{
	FireCannon();
}

void APHCTankPawn::ServerSetThrottle_Implementation(float Value)
{
	ThrottleInput = Value;
}

void APHCTankPawn::ServerSetSteering_Implementation(float Value)
{
	SteeringInput = Value;
}

void APHCTankPawn::ServerSetTurretYaw_Implementation(float Value)
{
	TurretYawInput = Value;
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

void APHCTankPawn::SetBodyColor(const FLinearColor& Color)
{
	UMaterial* ColorMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));
	if (!ColorMat) return;

	for (UStaticMeshComponent* Comp : OliveDrabMeshes)
	{
		if (!Comp) continue;
		UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
		if (!M)
		{
			M = UMaterialInstanceDynamic::Create((UMaterialInterface*)ColorMat, Comp);
			if (M) Comp->SetMaterial(0, M);
		}
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), Color);
			M->SetScalarParameterValue(TEXT("Metallic"), 0.3f);
			M->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
		}
	}
}

void APHCTankPawn::SetHeadlightsEnabled(bool bEnabled)
{
	if (HeadlightSpotL) HeadlightSpotL->SetVisibility(bEnabled);
	if (HeadlightSpotR) HeadlightSpotR->SetVisibility(bEnabled);

	const FLinearColor Emis = bEnabled ? FLinearColor(3.f, 2.8f, 2.2f) : FLinearColor::Black;
	auto SetEmis = [&](UStaticMeshComponent* Comp)
	{
		if (!Comp) return;
		UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
		if (M) M->SetVectorParameterValue(TEXT("EmissiveColor"), Emis);
	};
	SetEmis(HeadlightMeshL);
	SetEmis(HeadlightMeshR);
}

FVector APHCTankPawn::GetExitLocation() const
{
	return GetActorLocation() + GetActorRightVector() * -350.f + FVector(0.f, 0.f, 100.f);
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCTankPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APHCTankPawn, bIsOccupied);
}
