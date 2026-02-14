// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Cornhole/BeanBag.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

ABeanBag::ABeanBag()
{
	PrimaryActorTick.bCanEverTick = true;

	// Collision sphere
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(BagRadius);
	Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
	Collision->SetSimulatePhysics(false); // enabled on launch
	Collision->SetLinearDamping(1.2f);    // bags decelerate quickly
	Collision->SetAngularDamping(2.0f);
	Collision->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(Collision);

	// Visual mesh (sphere as approximation of a bean bag)
	BagMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BagMesh"));
	BagMesh->SetupAttachment(Collision);
	BagMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BagMesh->SetRelativeScale3D(FVector(BagRadius * 2.f / 100.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BagMesh->SetStaticMesh(SphereMesh.Object);
	}
}

// ---------------------------------------------------------------------------

void ABeanBag::BeginPlay()
{
	Super::BeginPlay();
	ConfigurePhysics();
	ApplyColor();
}

// ---------------------------------------------------------------------------

void ABeanBag::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bLaunched && !bSettled)
	{
		CheckSettled();
	}
}

// ---------------------------------------------------------------------------

void ABeanBag::Launch(FVector Velocity)
{
	bLaunched = true;
	bSettled = false;
	bScored = false;
	bInHole = false;
	bOnBoard = false;
	ScoreValue = 0;
	SettleAccumulator = 0.f;

	if (Collision)
	{
		Collision->SetSimulatePhysics(true);
		Collision->SetPhysicsLinearVelocity(Velocity);
	}
}

// ---------------------------------------------------------------------------

void ABeanBag::OnEnteredHole()
{
	bInHole = true;
	ScoreValue = 3;

	// Disable physics and hide the bag (it fell through the hole)
	if (Collision)
	{
		Collision->SetSimulatePhysics(false);
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);

	// Force settle immediately
	if (!bSettled)
	{
		bSettled = true;
		OnBagSettled.Broadcast(this);
	}
}

// ---------------------------------------------------------------------------

void ABeanBag::ResetBag(FVector StartPosition)
{
	bLaunched = false;
	bSettled = false;
	bScored = false;
	bInHole = false;
	bOnBoard = false;
	ScoreValue = 0;
	SettleAccumulator = 0.f;

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorLocation(StartPosition);

	if (Collision)
	{
		Collision->SetSimulatePhysics(false);
		Collision->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Collision->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

// ---------------------------------------------------------------------------

void ABeanBag::ConfigurePhysics()
{
	if (!Collision)
	{
		return;
	}

	// Bean bag physics: very low bounce, moderate friction
	UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(this, TEXT("BeanBagPhysMat"));
	PhysMat->Friction = 0.7f;
	PhysMat->Restitution = 0.05f; // bean bags barely bounce
	PhysMat->RestitutionCombineMode = EFrictionCombineMode::Min;
	PhysMat->FrictionCombineMode = EFrictionCombineMode::Average;

	Collision->BodyInstance.SetPhysMaterialOverride(PhysMat);

	// Typical bean bag mass ~450g
	Collision->SetMassOverrideInKg(NAME_None, 0.45f);
}

// ---------------------------------------------------------------------------

void ABeanBag::ApplyColor()
{
	if (BagMesh && BagMesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat =
			BagMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), BagColor);
		}
	}
}

// ---------------------------------------------------------------------------

void ABeanBag::CheckSettled()
{
	if (!Collision || bSettled)
	{
		return;
	}

	const float Speed = Collision->GetPhysicsLinearVelocity().Size();

	if (Speed < SettleSpeedThreshold)
	{
		SettleAccumulator += GetWorld()->GetDeltaSeconds();
		if (SettleAccumulator >= SettleRequiredTime)
		{
			bSettled = true;

			// Determine if the bag is on the board (score = 1)
			// or off (score = 0).  "In hole" is handled by OnEnteredHole().
			if (!bInHole)
			{
				// Check if the bag is above a minimum height (on board surface)
				// The board is angled and elevated; bags on the ground are below ~10 cm
				const float BagZ = GetActorLocation().Z;
				if (BagZ > 10.f) // approximate board surface threshold
				{
					bOnBoard = true;
					ScoreValue = 1;
				}
				else
				{
					bOnBoard = false;
					ScoreValue = 0;
				}
			}

			OnBagSettled.Broadcast(this);
		}
	}
	else
	{
		// Still moving - reset accumulator
		SettleAccumulator = 0.f;
	}
}
