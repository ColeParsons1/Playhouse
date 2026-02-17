// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Carrom/CarromBoard.h"
#include "Games/Carrom/CarromPiece.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

ACarromBoard::ACarromBoard()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Board surface
	BoardSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardSurface"));
	SetRootComponent(BoardSurface);
	BoardSurface->SetCollisionProfileName(TEXT("BlockAll"));
	BoardSurface->SetSimulatePhysics(false);
	if (CubeMesh.Succeeded())
	{
		BoardSurface->SetStaticMesh(CubeMesh.Object);
		BoardSurface->SetRelativeScale3D(FVector(BoardSize / 100.f, BoardSize / 100.f, 0.02f));
	}

	// Boundary walls
	auto CreateWall = [&](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetupAttachment(BoardSurface);
		W->SetCollisionProfileName(TEXT("BlockAll"));
		W->SetSimulatePhysics(false);
		if (CubeMesh.Succeeded())
		{
			W->SetStaticMesh(CubeMesh.Object);
		}
		return W;
	};

	WallTop = CreateWall(TEXT("WallTop"));
	WallBottom = CreateWall(TEXT("WallBottom"));
	WallLeft = CreateWall(TEXT("WallLeft"));
	WallRight = CreateWall(TEXT("WallRight"));

	BuildWalls();
	BuildPockets();
}

// ---------------------------------------------------------------------------

void ACarromBoard::BeginPlay()
{
	Super::BeginPlay();

	ConfigureWallPhysics(WallTop);
	ConfigureWallPhysics(WallBottom);
	ConfigureWallPhysics(WallLeft);
	ConfigureWallPhysics(WallRight);
}

// ---------------------------------------------------------------------------

void ACarromBoard::BuildWalls()
{
	const float Half = BoardSize * 0.5f;
	const float WT = WallThickness;
	const float WH = WallHeight;

	// Top wall (+Y)
	WallTop->SetRelativeLocation(FVector(0.f, Half + WT * 0.5f, WH * 0.5f));
	WallTop->SetRelativeScale3D(FVector(BoardSize / 100.f, WT / 100.f, WH / 100.f));

	// Bottom wall (-Y)
	WallBottom->SetRelativeLocation(FVector(0.f, -(Half + WT * 0.5f), WH * 0.5f));
	WallBottom->SetRelativeScale3D(FVector(BoardSize / 100.f, WT / 100.f, WH / 100.f));

	// Left wall (-X)
	WallLeft->SetRelativeLocation(FVector(-(Half + WT * 0.5f), 0.f, WH * 0.5f));
	WallLeft->SetRelativeScale3D(FVector(WT / 100.f, BoardSize / 100.f, WH / 100.f));

	// Right wall (+X)
	WallRight->SetRelativeLocation(FVector(Half + WT * 0.5f, 0.f, WH * 0.5f));
	WallRight->SetRelativeScale3D(FVector(WT / 100.f, BoardSize / 100.f, WH / 100.f));
}

// ---------------------------------------------------------------------------

void ACarromBoard::BuildPockets()
{
	const float Half = BoardSize * 0.5f;

	// Four corner pockets
	const TArray<FVector> PocketPositions = {
		FVector(-Half, -Half, 0.f),
		FVector(-Half,  Half, 0.f),
		FVector( Half, -Half, 0.f),
		FVector( Half,  Half, 0.f)
	};

	for (int32 i = 0; i < PocketPositions.Num(); ++i)
	{
		FName PocketName = *FString::Printf(TEXT("Pocket_%d"), i);
		USphereComponent* Pocket = CreateDefaultSubobject<USphereComponent>(PocketName);
		Pocket->SetupAttachment(BoardSurface);
		Pocket->SetRelativeLocation(PocketPositions[i]);
		Pocket->SetSphereRadius(PocketRadius);
		Pocket->SetCollisionProfileName(TEXT("OverlapAll"));
		Pocket->SetGenerateOverlapEvents(true);
		Pocket->OnComponentBeginOverlap.AddDynamic(this, &ACarromBoard::OnPocketOverlap);
		Pockets.Add(Pocket);
	}
}

// ---------------------------------------------------------------------------

void ACarromBoard::ConfigureWallPhysics(UStaticMeshComponent* Wall)
{
	if (!Wall)
	{
		return;
	}

	UPhysicalMaterial* WallMat = NewObject<UPhysicalMaterial>(this);
	WallMat->Friction = 0.15f;
	WallMat->Restitution = 0.75f;
	WallMat->RestitutionCombineMode = EFrictionCombineMode::Max;
	Wall->BodyInstance.SetPhysMaterialOverride(WallMat);
}

// ---------------------------------------------------------------------------

void ACarromBoard::OnPocketOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	ACarromPiece* Piece = Cast<ACarromPiece>(OtherActor);
	if (Piece && !Piece->bPocketed)
	{
		OnPiecePocketed.Broadcast(Piece);
	}
}
