// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Cornhole/CornholeBoard.h"
#include "Games/Cornhole/BeanBag.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ACornholeBoard::ACornholeBoard()
{
	PrimaryActorTick.bCanEverTick = false;

	// -- Board collision (flat box representing the playing surface) -------

	BoardCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardCollision"));
	BoardCollision->SetBoxExtent(FVector(BoardLength * 0.5f, BoardWidth * 0.5f, 2.f));
	BoardCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BoardCollision->SetRelativeRotation(FRotator(BoardAngle, 0.f, 0.f));
	SetRootComponent(BoardCollision);

	// -- Board mesh (scaled cube as visual stand-in) ----------------------

	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
	BoardMesh->SetupAttachment(BoardCollision);
	BoardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Scale the default 100-unit cube to match the board dimensions
	BoardMesh->SetRelativeScale3D(FVector(
		BoardLength / 100.f,
		BoardWidth / 100.f,
		4.f / 100.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BoardMesh->SetStaticMesh(CubeMesh.Object);
	}

	// -- Hole trigger (overlap volume centred at the hole position) --------
	// Regulation: hole is ~22 cm diameter, centred 22 cm from the far end.

	HoleTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("HoleTrigger"));
	HoleTrigger->SetupAttachment(BoardCollision);
	HoleTrigger->SetBoxExtent(FVector(HoleRadius, HoleRadius, HoleRadius));
	// Position the hole towards the far (upper) end of the board
	HoleTrigger->SetRelativeLocation(FVector(BoardLength * 0.3f, 0.f, 0.f));
	HoleTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	HoleTrigger->SetGenerateOverlapEvents(true);
	HoleTrigger->OnComponentBeginOverlap.AddDynamic(
		this, &ACornholeBoard::OnHoleOverlap);
}

// ---------------------------------------------------------------------------
// Overlap callback
// ---------------------------------------------------------------------------

void ACornholeBoard::OnHoleOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	ABeanBag* Bag = Cast<ABeanBag>(OtherActor);
	if (Bag && !Bag->bInHole)
	{
		Bag->OnEnteredHole();
	}
}
