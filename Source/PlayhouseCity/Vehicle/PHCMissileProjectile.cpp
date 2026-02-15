// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCMissileProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"

APHCMissileProjectile::APHCMissileProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// ---- Multiplayer replication ----
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(12.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	CollisionSphere->SetSimulatePhysics(false);
	CollisionSphere->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(CollisionSphere);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Elongated shape to look like a missile (visible at speed)
	ProjectileMesh->SetRelativeScale3D(FVector(0.8f, 0.35f, 0.35f));

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

	CollisionSphere->OnComponentHit.AddDynamic(this, &APHCMissileProjectile::OnHit);
}

void APHCMissileProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
	}

	// Color the missile bright red with emissive glow
	if (ProjectileMesh)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(
			ProjectileMesh->GetMaterial(0), this);
		if (Mat)
		{
			// Bright red base color (values > 1 drive bloom/emissive)
			Mat->SetVectorParameterValue(TEXT("BaseColor"),
				FLinearColor(3.0f, 0.15f, 0.05f));
			ProjectileMesh->SetMaterial(0, Mat);
		}
	}

	// Red point light for visible glow trail
	UPointLightComponent* Glow = NewObject<UPointLightComponent>(this, TEXT("MissileGlow"));
	if (Glow)
	{
		Glow->SetupAttachment(CollisionSphere);
		Glow->SetRelativeLocation(FVector::ZeroVector);
		Glow->SetIntensity(5000.f);
		Glow->SetAttenuationRadius(400.f);
		Glow->SetLightColor(FLinearColor(1.0f, 0.1f, 0.02f));
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
	}

	// Ignore collision with the vehicle that fired us
	if (OwnerVehicle)
	{
		TArray<UPrimitiveComponent*> OwnerComps;
		OwnerVehicle->GetComponents<UPrimitiveComponent>(OwnerComps);
		for (UPrimitiveComponent* Comp : OwnerComps)
		{
			CollisionSphere->IgnoreComponentWhenMoving(Comp, true);
			Comp->IgnoreComponentWhenMoving(CollisionSphere, true);
		}
	}

	SetLifeSpan(LifeSpan);
}

void APHCMissileProjectile::OnHit(
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

	// Only process damage on the server
	if (HasAuthority())
	{
		FDamageEvent DmgEvent;
		OtherActor->TakeDamage(Damage, DmgEvent, nullptr, this);
		Destroy();
	}
}
