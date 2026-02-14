// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Pool/PoolTable.h"
#include "Games/Pool/PoolBall.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

APoolTable::APoolTable()
{
	PrimaryActorTick.bCanEverTick = false;

	// Playing surface
	TableSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableSurface"));
	SetRootComponent(TableSurface);
	TableSurface->SetCollisionProfileName(TEXT("BlockAll"));
	TableSurface->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		TableSurface->SetStaticMesh(CubeMesh.Object);
		// Scale cube to a flat playing surface
		TableSurface->SetRelativeScale3D(FVector(TableLength / 100.f, TableWidth / 100.f, 0.02f));
	}

	// Cushions
	auto CreateCushion = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(TableSurface);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetSimulatePhysics(false);
		if (CubeMesh.Succeeded())
		{
			C->SetStaticMesh(CubeMesh.Object);
		}
		return C;
	};

	CushionTop = CreateCushion(TEXT("CushionTop"));
	CushionBottom = CreateCushion(TEXT("CushionBottom"));
	CushionLeft = CreateCushion(TEXT("CushionLeft"));
	CushionRight = CreateCushion(TEXT("CushionRight"));

	BuildCushions();
	BuildPockets();
}

// ---------------------------------------------------------------------------

void APoolTable::BeginPlay()
{
	Super::BeginPlay();

	ConfigureCushionPhysics(CushionTop);
	ConfigureCushionPhysics(CushionBottom);
	ConfigureCushionPhysics(CushionLeft);
	ConfigureCushionPhysics(CushionRight);
}

// ---------------------------------------------------------------------------

void APoolTable::BuildCushions()
{
	const float HalfLen = TableLength * 0.5f;
	const float HalfWid = TableWidth * 0.5f;
	const float CT = CushionThickness;
	const float CH = CushionHeight;

	// Cushion scales (cube is 100x100x100 units)
	// Top (along +Y)
	CushionTop->SetRelativeLocation(FVector(0.f, HalfWid + CT * 0.5f, CH * 0.5f));
	CushionTop->SetRelativeScale3D(FVector(TableLength / 100.f, CT / 100.f, CH / 100.f));

	// Bottom (along -Y)
	CushionBottom->SetRelativeLocation(FVector(0.f, -(HalfWid + CT * 0.5f), CH * 0.5f));
	CushionBottom->SetRelativeScale3D(FVector(TableLength / 100.f, CT / 100.f, CH / 100.f));

	// Left (along -X)
	CushionLeft->SetRelativeLocation(FVector(-(HalfLen + CT * 0.5f), 0.f, CH * 0.5f));
	CushionLeft->SetRelativeScale3D(FVector(CT / 100.f, TableWidth / 100.f, CH / 100.f));

	// Right (along +X)
	CushionRight->SetRelativeLocation(FVector(HalfLen + CT * 0.5f, 0.f, CH * 0.5f));
	CushionRight->SetRelativeScale3D(FVector(CT / 100.f, TableWidth / 100.f, CH / 100.f));
}

// ---------------------------------------------------------------------------

void APoolTable::BuildPockets()
{
	const float HalfLen = TableLength * 0.5f;
	const float HalfWid = TableWidth * 0.5f;
	const float PR = PocketRadius;

	// Six standard pocket positions: 4 corners + 2 side mid-points
	const TArray<FVector> PocketPositions = {
		FVector(-HalfLen, -HalfWid, 0.f),   // Bottom-left corner
		FVector(-HalfLen,  HalfWid, 0.f),   // Top-left corner
		FVector( HalfLen, -HalfWid, 0.f),   // Bottom-right corner
		FVector( HalfLen,  HalfWid, 0.f),   // Top-right corner
		FVector(0.f,      -HalfWid, 0.f),   // Bottom-mid side
		FVector(0.f,       HalfWid, 0.f)    // Top-mid side
	};

	for (int32 i = 0; i < PocketPositions.Num(); ++i)
	{
		FName PocketName = *FString::Printf(TEXT("Pocket_%d"), i);
		UBoxComponent* Pocket = CreateDefaultSubobject<UBoxComponent>(PocketName);
		Pocket->SetupAttachment(TableSurface);
		Pocket->SetRelativeLocation(PocketPositions[i]);
		Pocket->SetBoxExtent(FVector(PR, PR, PR));
		Pocket->SetCollisionProfileName(TEXT("OverlapAll"));
		Pocket->SetGenerateOverlapEvents(true);
		Pocket->OnComponentBeginOverlap.AddDynamic(this, &APoolTable::OnPocketOverlap);
		Pockets.Add(Pocket);
	}
}

// ---------------------------------------------------------------------------

void APoolTable::ConfigureCushionPhysics(UStaticMeshComponent* Cushion)
{
	if (!Cushion)
	{
		return;
	}

	UPhysicalMaterial* CushionMat = NewObject<UPhysicalMaterial>(this);
	CushionMat->Friction = 0.1f;
	CushionMat->Restitution = 0.9f;
	CushionMat->RestitutionCombineMode = EFrictionCombineMode::Max;
	Cushion->BodyInstance.SetPhysMaterialOverride(CushionMat);
}

// ---------------------------------------------------------------------------

void APoolTable::OnPocketOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	APoolBall* Ball = Cast<APoolBall>(OtherActor);
	if (Ball && !Ball->bPotted)
	{
		OnBallPocketed.Broadcast(Ball);
	}
}
