// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Pool/PoolBall.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

APoolBall::APoolBall()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root collision sphere
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(2.85f); // regulation ball ~ 5.7 cm diameter
	CollisionSphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionSphere->SetSimulatePhysics(true);
	CollisionSphere->SetLinearDamping(0.8f);
	CollisionSphere->SetAngularDamping(1.0f);
	CollisionSphere->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(CollisionSphere);

	// Visual mesh
	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	BallMesh->SetupAttachment(CollisionSphere);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetRelativeScale3D(FVector(0.057f)); // scale unit sphere to ~5.7 cm

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BallMesh->SetStaticMesh(SphereMesh.Object);
	}
}

// ---------------------------------------------------------------------------

void APoolBall::BeginPlay()
{
	Super::BeginPlay();
	ConfigurePhysics();
}

// ---------------------------------------------------------------------------

void APoolBall::InitBall(int32 InBallNumber)
{
	BallNumber = FMath::Clamp(InBallNumber, 0, 15);

	if (BallNumber == 0)
	{
		BallType = EPHCPoolBallType::Cue;
	}
	else if (BallNumber >= 1 && BallNumber <= 7)
	{
		BallType = EPHCPoolBallType::Solid;
	}
	else if (BallNumber == 8)
	{
		BallType = EPHCPoolBallType::Eight;
	}
	else
	{
		BallType = EPHCPoolBallType::Stripe;
	}

	BallColor = GetBallColor(BallNumber);
	bPotted = false;

	// Apply colour to dynamic material instance
	if (BallMesh && BallMesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat = BallMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), BallColor);
		}
	}
}

// ---------------------------------------------------------------------------

FLinearColor APoolBall::GetBallColor(int32 Number)
{
	// Standard American-style pool ball colours
	switch (Number)
	{
	case 0:  return FLinearColor::White;                                       // Cue
	case 1:  return FLinearColor(1.0f, 0.85f, 0.0f, 1.f);                     // Yellow
	case 2:  return FLinearColor(0.0f, 0.0f, 0.8f, 1.f);                      // Blue
	case 3:  return FLinearColor(1.0f, 0.0f, 0.0f, 1.f);                      // Red
	case 4:  return FLinearColor(0.3f, 0.0f, 0.5f, 1.f);                      // Purple
	case 5:  return FLinearColor(1.0f, 0.5f, 0.0f, 1.f);                      // Orange
	case 6:  return FLinearColor(0.0f, 0.5f, 0.0f, 1.f);                      // Green
	case 7:  return FLinearColor(0.5f, 0.0f, 0.0f, 1.f);                      // Maroon
	case 8:  return FLinearColor(0.05f, 0.05f, 0.05f, 1.f);                   // Black
	case 9:  return FLinearColor(1.0f, 0.85f, 0.0f, 1.f);                     // Yellow stripe
	case 10: return FLinearColor(0.0f, 0.0f, 0.8f, 1.f);                      // Blue stripe
	case 11: return FLinearColor(1.0f, 0.0f, 0.0f, 1.f);                      // Red stripe
	case 12: return FLinearColor(0.3f, 0.0f, 0.5f, 1.f);                      // Purple stripe
	case 13: return FLinearColor(1.0f, 0.5f, 0.0f, 1.f);                      // Orange stripe
	case 14: return FLinearColor(0.0f, 0.5f, 0.0f, 1.f);                      // Green stripe
	case 15: return FLinearColor(0.5f, 0.0f, 0.0f, 1.f);                      // Maroon stripe
	default: return FLinearColor::White;
	}
}

// ---------------------------------------------------------------------------

void APoolBall::OnPotted()
{
	bPotted = true;

	// Stop physics and hide
	if (CollisionSphere)
	{
		CollisionSphere->SetSimulatePhysics(false);
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

// ---------------------------------------------------------------------------

bool APoolBall::IsMoving() const
{
	if (!CollisionSphere || bPotted)
	{
		return false;
	}
	return CollisionSphere->GetPhysicsLinearVelocity().Size() > StationaryThreshold;
}

// ---------------------------------------------------------------------------

void APoolBall::ConfigurePhysics()
{
	if (!CollisionSphere)
	{
		return;
	}

	// Create and assign a physics material for realistic pool-ball response
	UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(this, TEXT("PoolBallPhysMat"));
	PhysMat->Friction = 0.25f;
	PhysMat->Restitution = 0.85f;
	PhysMat->RestitutionCombineMode = EFrictionCombineMode::Max;
	PhysMat->FrictionCombineMode = EFrictionCombineMode::Average;

	CollisionSphere->BodyInstance.SetPhysMaterialOverride(PhysMat);

	// Clamp maximum velocity for stable simulation
	CollisionSphere->BodyInstance.MaxAngularVelocity = 3000.f;
	CollisionSphere->SetMassOverrideInKg(NAME_None, 0.17f); // ~170 g regulation ball
}
