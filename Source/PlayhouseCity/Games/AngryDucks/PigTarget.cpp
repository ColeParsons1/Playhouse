// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/AngryDucks/PigTarget.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

APigTarget::APigTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(PigRadius);
	CollisionSphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionSphere->SetSimulatePhysics(true);
	CollisionSphere->SetEnableGravity(true);
	CollisionSphere->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(CollisionSphere);

	PigMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PigMesh"));
	PigMesh->SetupAttachment(CollisionSphere);
	PigMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PigMesh->SetRelativeScale3D(FVector(PigRadius / 50.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PigMesh->SetStaticMesh(SphereMesh.Object);
	}
}

// ---------------------------------------------------------------------------

void APigTarget::BeginPlay()
{
	Super::BeginPlay();
	ApplyColor();
}

// ---------------------------------------------------------------------------
// -- API --
// ---------------------------------------------------------------------------

void APigTarget::ApplyDamage(float DamageAmount)
{
	if (bDestroyed)
	{
		return;
	}

	HP -= DamageAmount;

	if (HP <= 0.f)
	{
		HP = 0.f;
		bDestroyed = true;

		OnPigDestroyed.Broadcast(this);

		// Visual feedback -- hide and disable collision.
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);

		// Deferred destroy to allow delegates to fire.
		SetLifeSpan(0.1f);
	}
}

// ---------------------------------------------------------------------------
// -- Visuals --
// ---------------------------------------------------------------------------

void APigTarget::ApplyColor()
{
	if (PigMesh && PigMesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat = PigMesh->CreateDynamicMaterialInstance(0);
		if (DynMat)
		{
			// Bright green for the pig
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.2f, 0.8f, 0.15f, 1.f));
		}
	}
}
