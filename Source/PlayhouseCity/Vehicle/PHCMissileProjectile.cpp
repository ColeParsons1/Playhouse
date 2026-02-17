// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCMissileProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APHCMissileProjectile::APHCMissileProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// No replication — missiles are visual tracers, spawned locally.
	bReplicates = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(10.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionSphere->SetSimulatePhysics(false);
	SetRootComponent(CollisionSphere);

	// Single elongated cube — lightweight bullet/tracer round
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetRelativeScale3D(FVector(2.0f, 0.20f, 0.20f));
	ProjectileMesh->SetBoundsScale(5.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(CubeMesh.Object);
	}

	// No ProjectileMovementComponent — manual velocity in Tick is far cheaper
	// than 50+ PMC sweep tests per frame at machine-gun fire rates.
	// (ProjectileMovement UPROPERTY stays null; callers null-check it.)
}

void APHCMissileProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Store flight velocity from spawn rotation
	ManualVelocity = GetActorForwardVector() * Speed;

	// Bright emissive material — visible without point lights (performance-friendly)
	if (ProjectileMesh && ProjectileMesh->GetStaticMesh())
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(
			ProjectileMesh->GetMaterial(0), this);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"),
				FLinearColor(0.5f, 0.2f, 1.0f));
			Mat->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(4.0f, 1.5f, 8.0f));
			Mat->SetScalarParameterValue(TEXT("Metallic"), 0.8f);
			Mat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
			ProjectileMesh->SetMaterial(0, Mat);
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
}

void APHCMissileProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Manual gravity (for bombs)
	if (bUseManualGravity)
	{
		ManualVelocity.Z -= 980.f * ManualGravityScale * DeltaTime;
	}

	// Move forward — simple SetActorLocation, no sweep, no PMC overhead
	SetActorLocation(GetActorLocation() + ManualVelocity * DeltaTime);

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

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Dot = World->SpawnActor<AActor>(
		AActor::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, SP);
	if (!Dot) return;

	Dot->SetReplicates(false);
	Dot->SetLifeSpan(0.4f);

	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(
		Dot, UStaticMeshComponent::StaticClass(), TEXT("TrailMesh"));
	Mesh->SetStaticMesh(TrailSphereMesh);
	Mesh->SetRelativeScale3D(FVector(0.12f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dot->SetRootComponent(Mesh);
	Mesh->RegisterComponent();

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(
		Mesh->GetMaterial(0), Dot);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("BaseColor"), TrailColor);
		Mat->SetVectorParameterValue(TEXT("EmissiveColor"), TrailColor);
		Mesh->SetMaterial(0, Mat);
	}
}
