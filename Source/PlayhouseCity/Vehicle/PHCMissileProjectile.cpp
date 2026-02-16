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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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
		if (OnHitSpawnClass)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				FActorSpawnParameters SP;
				SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				World->SpawnActor<AActor>(OnHitSpawnClass, GetActorLocation(), FRotator::ZeroRotator, SP);
			}
		}
		FDamageEvent DmgEvent;
		OtherActor->TakeDamage(Damage, DmgEvent, nullptr, this);
		Destroy();
	}
}

// ---------------------------------------------------------------------------
// Trail system
// ---------------------------------------------------------------------------

void APHCMissileProjectile::StartTrail(const FLinearColor& Color)
{
	bHasTrail = true;
	TrailColor = Color;
	TrailTimer = 0.f;

	TrailSphereMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	SetActorTickEnabled(true);
}

void APHCMissileProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Manual gravity movement (bypasses ProjectileMovementComponent)
	if (bUseManualGravity)
	{
		ManualVelocity.Z -= 980.f * ManualGravityScale * DeltaTime;
		SetActorLocation(GetActorLocation() + ManualVelocity * DeltaTime);
	}

	// Altitude detonation (for gravity bombs hitting ground)
	if (DetonateAtAltitude >= 0.f && GetActorLocation().Z <= DetonateAtAltitude)
	{
		if (OnHitSpawnClass)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				FActorSpawnParameters SP;
				SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				World->SpawnActor<AActor>(OnHitSpawnClass, GetActorLocation(), FRotator::ZeroRotator, SP);
			}
		}
		Destroy();
		return;
	}

	if (!bHasTrail) return;

	TrailTimer -= DeltaTime;
	if (TrailTimer > 0.f) return;
	TrailTimer = TrailInterval;

	SpawnTrailDot();
}

void APHCMissileProjectile::SpawnTrailDot()
{
	UWorld* World = GetWorld();
	if (!World || !TrailSphereMesh) return;

	// Spawn a lightweight non-replicated actor for the trail dot.
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Dot = World->SpawnActor<AActor>(
		AActor::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, SP);
	if (!Dot) return;

	Dot->SetReplicates(false);
	Dot->SetLifeSpan(0.4f);

	// Small glowing sphere
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(
		Dot, UStaticMeshComponent::StaticClass(), TEXT("TrailMesh"));
	Mesh->SetStaticMesh(TrailSphereMesh);
	Mesh->SetRelativeScale3D(FVector(0.12f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dot->SetRootComponent(Mesh);
	Mesh->RegisterComponent();

	// Emissive cyan material (bloom in post-process creates the glow)
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(
		Mesh->GetMaterial(0), Dot);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("BaseColor"), TrailColor);
		Mat->SetVectorParameterValue(TEXT("EmissiveColor"), TrailColor);
		Mesh->SetMaterial(0, Mat);
	}
}
