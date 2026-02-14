// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/CelDamage/CelDamageProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ACelDamageProjectile::ACelDamageProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(8.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionSphere->SetSimulatePhysics(false);
	CollisionSphere->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(CollisionSphere);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetRelativeScale3D(FVector(0.15f));

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
	ProjectileMovement->ProjectileGravityScale = 0.f;

	CollisionSphere->OnComponentHit.AddDynamic(this, &ACelDamageProjectile::OnHit);
}

void ACelDamageProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;

		if (bIsHoming && HomingTarget)
		{
			ProjectileMovement->bIsHomingProjectile = true;
			ProjectileMovement->HomingTargetComponent = HomingTarget->GetRootComponent();
			ProjectileMovement->HomingAccelerationMagnitude = 3000.f;
		}
	}

	SetLifeSpan(LifeSpan);
}

void ACelDamageProjectile::OnHit(
	UPrimitiveComponent* /*HitComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/,
	const FHitResult& /*Hit*/)
{
	if (!OtherActor || OtherActor == OwnerVehicle)
	{
		return;
	}

	if (SplashRadius > 0.f)
	{
		// Splash damage: overlap sphere
		TArray<FOverlapResult> Overlaps;
		FCollisionShape Shape;
		Shape.SetSphere(SplashRadius);
		if (GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(),
			FQuat::Identity, ECC_Pawn, Shape))
		{
			for (const FOverlapResult& Overlap : Overlaps)
			{
				AActor* HitActor = Overlap.GetActor();
				if (HitActor && HitActor != OwnerVehicle)
				{
					FDamageEvent DmgEvent;
					HitActor->TakeDamage(Damage, DmgEvent, nullptr, this);
				}
			}
		}
	}
	else
	{
		FDamageEvent DmgEvent;
		OtherActor->TakeDamage(Damage, DmgEvent, nullptr, this);
	}

	Destroy();
}
