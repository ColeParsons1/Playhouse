// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/AngryDucks/DuckProjectile.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Games/AngryDucks/DestructibleBlock.h"
#include "Games/AngryDucks/PigTarget.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// ---------------------------------------------------------------------------

ADuckProjectile::ADuckProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(DuckRadius);
	CollisionSphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionSphere->SetSimulatePhysics(false); // enabled on launch
	CollisionSphere->SetEnableGravity(true);
	CollisionSphere->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(CollisionSphere);

	// Bounce: moderate restitution for a rubber-duck feel.
	CollisionSphere->BodyInstance.bOverrideMass = true;
	CollisionSphere->BodyInstance.SetMassOverride(2.f);

	DuckMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DuckMesh"));
	DuckMesh->SetupAttachment(CollisionSphere);
	DuckMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DuckMesh->SetRelativeScale3D(FVector(DuckRadius / 50.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		DuckMesh->SetStaticMesh(SphereMesh.Object);
	}

	CollisionSphere->OnComponentHit.AddDynamic(this, &ADuckProjectile::OnHit);
}

// ---------------------------------------------------------------------------

void ADuckProjectile::BeginPlay()
{
	Super::BeginPlay();
	ApplyColor();
}

void ADuckProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bLaunched && !bSettled)
	{
		CheckSettled();
	}
}

// ---------------------------------------------------------------------------
// -- API --
// ---------------------------------------------------------------------------

void ADuckProjectile::Launch(FVector Velocity)
{
	if (bLaunched)
	{
		return;
	}

	bLaunched = true;
	bSettled = false;
	SettleAccumulator = 0.f;

	CollisionSphere->SetSimulatePhysics(true);
	CollisionSphere->SetEnableGravity(true);
	CollisionSphere->SetPhysicsLinearVelocity(Velocity);

	// Set restitution for moderate bounce
	if (UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(this))
	{
		PhysMat->Restitution = 0.3f;
		PhysMat->Friction = 0.5f;
		CollisionSphere->BodyInstance.SetPhysMaterialOverride(PhysMat);
	}
}

// ---------------------------------------------------------------------------
// -- Impact handling --
// ---------------------------------------------------------------------------

void ADuckProjectile::OnHit(
	UPrimitiveComponent* /*HitComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector NormalImpulse,
	const FHitResult& /*Hit*/)
{
	if (!OtherActor)
	{
		return;
	}

	// Damage destructible blocks
	ADestructibleBlock* Block = Cast<ADestructibleBlock>(OtherActor);
	if (Block)
	{
		Block->ApplyDamage(ImpactDamage);
	}

	// Damage pig targets
	APigTarget* Pig = Cast<APigTarget>(OtherActor);
	if (Pig)
	{
		Pig->ApplyDamage(ImpactDamage);
	}
}

// ---------------------------------------------------------------------------
// -- Settle detection --
// ---------------------------------------------------------------------------

void ADuckProjectile::CheckSettled()
{
	if (!CollisionSphere)
	{
		return;
	}

	const float Speed = CollisionSphere->GetPhysicsLinearVelocity().Size();

	if (Speed < SettleSpeedThreshold)
	{
		SettleAccumulator += GetWorld()->GetDeltaSeconds();

		if (SettleAccumulator >= SettleRequiredTime)
		{
			bSettled = true;
			OnDuckSettled.Broadcast(this);

			// Auto-destroy after a short delay
			SetLifeSpan(PostSettleLifeSpan);
		}
	}
	else
	{
		SettleAccumulator = 0.f;
	}
}

// ---------------------------------------------------------------------------
// -- Visuals --
// ---------------------------------------------------------------------------

void ADuckProjectile::ApplyColor()
{
	if (DuckMesh && DuckMesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat = DuckMesh->CreateDynamicMaterialInstance(0);
		if (DynMat)
		{
			// Bright yellow for the duck
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.f, 0.85f, 0.f, 1.f));
		}
	}
}
