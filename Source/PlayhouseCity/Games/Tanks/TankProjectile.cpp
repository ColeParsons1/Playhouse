// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Tanks/TankProjectile.h"
#include "Games/Tanks/DestructibleWall.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// Forward declare - TankPawn is included at cpp level to avoid circular deps
class ATankPawn;

// ---------------------------------------------------------------------------

ATankProjectile::ATankProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(4.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionSphere->SetSimulatePhysics(false);
	CollisionSphere->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(CollisionSphere);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetRelativeScale3D(FVector(0.08f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereMesh.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f; // top-down: no gravity

	CollisionSphere->OnComponentHit.AddDynamic(this, &ATankProjectile::OnHit);
}

// ---------------------------------------------------------------------------

void ATankProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Configure speed at runtime in case it was modified
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
	}

	SetLifeSpan(LifeSpan);
}

// ---------------------------------------------------------------------------

void ATankProjectile::OnHit(
	UPrimitiveComponent* /*HitComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/,
	const FHitResult& /*Hit*/)
{
	if (!OtherActor || OtherActor == OwnerTank)
	{
		return;
	}

	// Damage destructible walls
	ADestructibleWall* Wall = Cast<ADestructibleWall>(OtherActor);
	if (Wall)
	{
		Wall->ApplyDamage(FMath::RoundToInt32(Damage));
	}

	// Damage tanks (via generic interface - we call the method if it exists)
	// TankPawn::TakeDamage is UE's built-in; we use our custom method via interface pattern
	if (OtherActor != OwnerTank)
	{
		// Use UE5 damage system
		FDamageEvent DmgEvent;
		OtherActor->TakeDamage(Damage, DmgEvent, nullptr, this);
	}

	Destroy();
}
