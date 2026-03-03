// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCMissileProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

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
	CollisionSphere->SetBoundsScale(200.f);
	CollisionSphere->bNeverDistanceCull = true;
	SetRootComponent(CollisionSphere);

	// Elongated sphere — bullet/tracer round shape
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetRelativeScale3D(FVector(1.8f, 0.14f, 0.14f));
	ProjectileMesh->SetBoundsScale(200.f);
	ProjectileMesh->bNeverDistanceCull = true;
	ProjectileMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BulletMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (BulletMesh.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(BulletMesh.Object);
	}

	// No ProjectileMovementComponent — manual velocity in Tick is far cheaper
	// than 50+ PMC sweep tests per frame at machine-gun fire rates.
	// (ProjectileMovement UPROPERTY stays null; callers null-check it.)
}

void APHCMissileProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Cache M_PHCColor for trail dots and fallback material
	CachedColorMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));

	// Velocity is deferred to first Tick so callers can set Speed after SpawnActor
	bVelocityInitialized = false;

	// Bright emissive material using M_PHCColor (not default engine mat)
	if (ProjectileMesh && CachedColorMat)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(CachedColorMat, this);
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

void APHCMissileProjectile::LifeSpanExpired()
{
	// RPG rockets self-detonate on expiry (GTA V: 5s = 300m range)
	if (bIsRPGRocket)
	{
		SpawnExplosion(GetActorLocation());
	}
	Super::LifeSpanExpired();
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

	// Deferred velocity init — runs on first Tick after callers have set Speed.
	// Skip if caller already set ManualVelocity directly (e.g. tank cannon, RPG).
	if (!bVelocityInitialized)
	{
		bVelocityInitialized = true;
		if (ManualVelocity.IsNearlyZero())
		{
			ManualVelocity = GetActorForwardVector() * Speed;
		}
	}

	// Manual gravity (for bombs)
	if (bUseManualGravity)
	{
		ManualVelocity.Z -= 980.f * ManualGravityScale * DeltaTime;
	}

	const FVector PrevLoc = GetActorLocation();
	const FVector StepDelta = ManualVelocity * DeltaTime;

	// Move forward — simple SetActorLocation, no sweep, no PMC overhead
	SetActorLocation(PrevLoc + StepDelta);

	// RPG: explode on ground impact
	if (bIsRPGRocket && GetActorLocation().Z <= 5.f)
	{
		SpawnExplosion(GetActorLocation());
		Destroy();
		return;
	}

	// Altitude detonation (for gravity bombs hitting ground)
	if (DetonateAtAltitude >= 0.f && GetActorLocation().Z <= DetonateAtAltitude)
	{
		if (bIsRPGRocket) SpawnExplosion(GetActorLocation());
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
	TrailTimer = TrailDotInterval;

	SpawnTrailDot();
}

void APHCMissileProjectile::SpawnTrailDot()
{
	UWorld* World = GetWorld();
	if (!World || !TrailSphereMesh) return;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Optional random spread for organic smoke look
	FVector SpawnLoc = GetActorLocation();
	if (TrailDotRandomSpread > 0.f)
	{
		SpawnLoc += FVector(
			FMath::RandRange(-TrailDotRandomSpread, TrailDotRandomSpread),
			FMath::RandRange(-TrailDotRandomSpread, TrailDotRandomSpread),
			FMath::RandRange(0.f, TrailDotRandomSpread * 0.5f)); // bias upward
	}

	AActor* Dot = World->SpawnActor<AActor>(
		AActor::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SP);
	if (!Dot) return;

	Dot->SetReplicates(false);
	Dot->SetLifeSpan(TrailDotLifespan);

	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(
		Dot, UStaticMeshComponent::StaticClass(), TEXT("TrailMesh"));
	Mesh->SetStaticMesh(TrailSphereMesh);
	Mesh->SetRelativeScale3D(FVector(TrailDotScale));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetBoundsScale(200.f);
	Mesh->bNeverDistanceCull = true;
	Mesh->SetCastShadow(false);
	Dot->SetRootComponent(Mesh);
	Mesh->RegisterComponent();

	if (CachedColorMat)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(CachedColorMat, Dot);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), TrailColor);
			Mat->SetVectorParameterValue(TEXT("EmissiveColor"), TrailColor);
			Mat->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
			Mesh->SetMaterial(0, Mat);
		}
	}
}

// ---------------------------------------------------------------------------
// RPG Explosion (GTA V style)
// ---------------------------------------------------------------------------

void APHCMissileProjectile::SpawnExplosion(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World) return;

	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterial* ColorMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));
	UMaterial* GlassMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCGlass.M_PHCGlass"));

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto SpawnMeshActor = [&](UStaticMesh* MeshAsset, const FVector& Loc,
		const FVector& Scale, float Life, const FLinearColor& BaseColor,
		const FLinearColor& Emissive, float Metallic, float Roughness,
		bool bUseGlass = false) -> AActor*
	{
		if (!MeshAsset) return nullptr;
		AActor* A = World->SpawnActor<AActor>(
			AActor::StaticClass(), Loc, FRotator::ZeroRotator, SP);
		if (!A) return nullptr;
		A->SetReplicates(false);
		A->SetLifeSpan(Life);

		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(
			A, UStaticMeshComponent::StaticClass(), TEXT("ExpMesh"));
		MC->SetStaticMesh(MeshAsset);
		MC->SetRelativeScale3D(Scale);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->CastShadow = false;
		A->SetRootComponent(MC);
		MC->RegisterComponent();

		UMaterial* BaseMat = bUseGlass ? GlassMat : ColorMat;
		if (BaseMat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(
				(UMaterialInterface*)BaseMat, A);
			if (MID)
			{
				MID->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
				MID->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
				MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
				MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
				if (bUseGlass)
					MID->SetScalarParameterValue(TEXT("Opacity"), 0.6f);
				MC->SetMaterial(0, MID);
			}
		}
		return A;
	};

	// === FIREBALL — bright orange-yellow core (GTA V: 5-8m diameter) ===
	// Inner fireball: intense bright core
	SpawnMeshActor(SphereMesh, Location, FVector(4.5f), 0.35f,
		FLinearColor(1.0f, 0.55f, 0.05f),     // hot orange
		FLinearColor(20.f, 10.f, 2.f),         // intense emissive glow
		0.f, 0.5f);

	// Outer fireball: slightly larger, darker orange
	SpawnMeshActor(SphereMesh, Location + FVector(0.f, 0.f, 30.f), FVector(6.f), 0.5f,
		FLinearColor(0.8f, 0.3f, 0.02f),      // deep orange
		FLinearColor(12.f, 4.f, 0.5f),         // warm glow
		0.f, 0.6f);

	// === SHOCKWAVE RING — expanding flat disc ===
	SpawnMeshActor(CylMesh, Location + FVector(0.f, 0.f, 5.f),
		FVector(8.f, 8.f, 0.03f), 0.3f,       // wide, paper-thin cylinder
		FLinearColor(1.f, 0.8f, 0.4f),        // hot white-yellow
		FLinearColor(6.f, 4.f, 1.5f),
		0.f, 0.5f, true);                     // glass mat for translucency

	// === DARK SMOKE CLOUD — GTA V lingers 3-5 seconds, rises ===
	// Inner smoke puffs (6 around center, close range)
	for (int32 i = 0; i < 6; ++i)
	{
		const float Angle = (float)i * 60.f;
		const float Rad = FMath::DegreesToRadians(Angle);
		const float OffR = FMath::RandRange(40.f, 120.f);
		const FVector Offset(
			FMath::Cos(Rad) * OffR,
			FMath::Sin(Rad) * OffR,
			FMath::RandRange(20.f, 80.f));    // slightly elevated
		const float SmokeScale = FMath::RandRange(2.5f, 4.0f);
		const float SmokeLife = FMath::RandRange(2.0f, 3.5f);
		const float Gray = FMath::RandRange(0.04f, 0.12f);

		SpawnMeshActor(SphereMesh, Location + Offset,
			FVector(SmokeScale), SmokeLife,
			FLinearColor(Gray, Gray, Gray),    // dark gray/black smoke
			FLinearColor::Black,               // no glow
			0.f, 0.95f);
	}

	// Outer rising smoke (4 higher, larger, longer lasting — GTA V convection)
	for (int32 i = 0; i < 4; ++i)
	{
		const float Angle = (float)i * 90.f + 45.f;
		const float Rad = FMath::DegreesToRadians(Angle);
		const float OffR = FMath::RandRange(60.f, 160.f);
		const FVector Offset(
			FMath::Cos(Rad) * OffR,
			FMath::Sin(Rad) * OffR,
			FMath::RandRange(120.f, 250.f));  // higher — rising smoke
		const float SmokeScale = FMath::RandRange(3.5f, 5.5f);
		const float SmokeLife = FMath::RandRange(3.5f, 5.0f);
		const float Gray = FMath::RandRange(0.06f, 0.15f);

		SpawnMeshActor(SphereMesh, Location + Offset,
			FVector(SmokeScale), SmokeLife,
			FLinearColor(Gray, Gray, Gray),
			FLinearColor::Black,
			0.f, 0.95f);
	}

	// === EMBERS / DEBRIS — small bright particles scattered outward ===
	for (int32 i = 0; i < 10; ++i)
	{
		const FVector Scatter(
			FMath::RandRange(-200.f, 200.f),
			FMath::RandRange(-200.f, 200.f),
			FMath::RandRange(30.f, 200.f));
		const float EmberScale = FMath::RandRange(0.06f, 0.15f);
		const float EmberLife = FMath::RandRange(0.3f, 0.8f);

		SpawnMeshActor(SphereMesh, Location + Scatter,
			FVector(EmberScale), EmberLife,
			FLinearColor(1.f, 0.6f, 0.1f),    // hot orange ember
			FLinearColor(8.f, 4.f, 1.f),       // bright glow
			0.f, 0.5f);
	}

	// === SCORCH MARK — flat dark disc on ground ===
	SpawnMeshActor(CylMesh, FVector(Location.X, Location.Y, 2.f),
		FVector(5.f, 5.f, 0.02f), 8.f,        // stays 8 seconds
		FLinearColor(0.02f, 0.02f, 0.02f),    // near-black scorch
		FLinearColor::Black,
		0.f, 0.98f);
}
