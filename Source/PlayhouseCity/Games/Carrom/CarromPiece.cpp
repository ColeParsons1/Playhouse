// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Carrom/CarromPiece.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

ACarromPiece::ACarromPiece()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionDisc = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionDisc"));
	CollisionDisc->InitSphereRadius(CoinRadius);
	CollisionDisc->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionDisc->SetSimulatePhysics(true);
	CollisionDisc->SetLinearDamping(2.5f);
	CollisionDisc->SetAngularDamping(3.0f);
	CollisionDisc->BodyInstance.bNotifyRigidBodyCollision = true;
	CollisionDisc->BodyInstance.bLockZTranslation = true;
	SetRootComponent(CollisionDisc);

	DiscMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiscMesh"));
	DiscMesh->SetupAttachment(CollisionDisc);
	DiscMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		DiscMesh->SetStaticMesh(CylMesh.Object);
		// Scale to a flat disc
		DiscMesh->SetRelativeScale3D(FVector(CoinRadius / 50.f, CoinRadius / 50.f, 0.005f));
	}
}

// ---------------------------------------------------------------------------

void ACarromPiece::BeginPlay()
{
	Super::BeginPlay();
	ConfigurePhysics();
}

// ---------------------------------------------------------------------------

void ACarromPiece::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ConstrainToBoard();
}

// ---------------------------------------------------------------------------

void ACarromPiece::InitPiece(EPHCCarromPieceType InType)
{
	PieceType = InType;
	bPocketed = false;

	// Resize collision for striker vs coin
	const float Radius = (InType == EPHCCarromPieceType::Striker) ? StrikerRadius : CoinRadius;
	if (CollisionDisc)
	{
		CollisionDisc->SetSphereRadius(Radius);
	}
	if (DiscMesh)
	{
		DiscMesh->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.005f));
	}

	// Apply colour
	FLinearColor Color;
	switch (InType)
	{
	case EPHCCarromPieceType::White:   Color = FLinearColor(0.95f, 0.92f, 0.85f, 1.f); break;
	case EPHCCarromPieceType::Black:   Color = FLinearColor(0.1f, 0.1f, 0.1f, 1.f); break;
	case EPHCCarromPieceType::Queen:   Color = FLinearColor(0.8f, 0.15f, 0.15f, 1.f); break;
	case EPHCCarromPieceType::Striker: Color = FLinearColor(0.6f, 0.55f, 0.45f, 1.f); break;
	}

	if (DiscMesh && DiscMesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat = DiscMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}

	ConfigurePhysics();
}

// ---------------------------------------------------------------------------

bool ACarromPiece::IsMoving() const
{
	if (!CollisionDisc || bPocketed)
	{
		return false;
	}
	return CollisionDisc->GetPhysicsLinearVelocity().Size() > StationaryThreshold;
}

// ---------------------------------------------------------------------------

void ACarromPiece::OnPocketed()
{
	bPocketed = true;

	if (CollisionDisc)
	{
		CollisionDisc->SetSimulatePhysics(false);
		CollisionDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

// ---------------------------------------------------------------------------

void ACarromPiece::ResetToPosition(FVector Pos)
{
	bPocketed = false;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorLocation(Pos);

	if (CollisionDisc)
	{
		CollisionDisc->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CollisionDisc->SetSimulatePhysics(true);
		CollisionDisc->SetPhysicsLinearVelocity(FVector::ZeroVector);
		CollisionDisc->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

// ---------------------------------------------------------------------------

void ACarromPiece::ConfigurePhysics()
{
	if (!CollisionDisc)
	{
		return;
	}

	UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(this, TEXT("CarromPhysMat"));

	if (PieceType == EPHCCarromPieceType::Striker)
	{
		PhysMat->Friction = 0.15f;
		PhysMat->Restitution = 0.7f;
		CollisionDisc->SetMassOverrideInKg(NAME_None, 0.015f); // 15 g striker
		CollisionDisc->SetLinearDamping(2.0f);
	}
	else
	{
		PhysMat->Friction = 0.2f;
		PhysMat->Restitution = 0.6f;
		CollisionDisc->SetMassOverrideInKg(NAME_None, 0.005f); // 5 g coin
		CollisionDisc->SetLinearDamping(3.0f);
	}

	PhysMat->RestitutionCombineMode = EFrictionCombineMode::Average;
	PhysMat->FrictionCombineMode = EFrictionCombineMode::Average;
	CollisionDisc->BodyInstance.SetPhysMaterialOverride(PhysMat);
}

// ---------------------------------------------------------------------------

void ACarromPiece::ConstrainToBoard()
{
	if (!CollisionDisc || bPocketed)
	{
		return;
	}

	// Kill any Z velocity to keep the piece flat on the board
	FVector Vel = CollisionDisc->GetPhysicsLinearVelocity();
	if (FMath::Abs(Vel.Z) > 1.f)
	{
		Vel.Z = 0.f;
		CollisionDisc->SetPhysicsLinearVelocity(Vel);
	}
}
