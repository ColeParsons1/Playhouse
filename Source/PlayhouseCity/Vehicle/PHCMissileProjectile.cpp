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
#include "Kismet/GameplayStatics.h"
#include "Core/PHCCitySubsystem.h"
#include "Player/PHCCityCharacter.h"
#include "Vehicle/PHCNukeExplosion.h"

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
	ProjectileMesh->SetRelativeScale3D(FVector(4.0f, 0.35f, 0.35f));
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

	// --- Proximity damage: hit NPCs, crabs, players within blast radius ---
	if (Damage > 0.f && GetWorld())
	{
		const float HitRadius = 200.f;
		const FVector CurLoc = GetActorLocation();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Target = *It;
			if (!Target || Target == this || Target == OwnerVehicle || Target->IsHidden()) continue;
			if (!Target->Tags.Contains(FName("NPCPlayer")) &&
				!Target->Tags.Contains(FName("GiantCrab")) &&
				!Cast<APHCCityCharacter>(Target)) continue;
			if (Target == Cast<AActor>(OwnerVehicle)) continue;

			const float Dist = FVector::Dist(CurLoc, Target->GetActorLocation() + FVector(0,0,50));
			if (Dist < HitRadius)
			{
				// Apply damage
				if (Target->ActorHasTag(FName("GiantCrab")))
				{
					UPHCCitySubsystem* CS = GetWorld()->GetSubsystem<UPHCCitySubsystem>();
					if (CS)
					{
						for (int32 CI = 0; CI < UPHCCitySubsystem::NumGiantCrabs; ++CI)
						{
							if (CS->Crabs[CI].Actor == Target)
							{
								CS->Crabs[CI].Health -= Damage;
								CS->Crabs[CI].DamageFlashTimer = 0.15f;
								if (CS->Crabs[CI].Health <= 0.f) CS->RespawnCrab(CI);
								break;
							}
						}
					}
				}
				else if (Target->ActorHasTag(FName("NPCPlayer")))
				{
					UPHCCitySubsystem* CS = GetWorld()->GetSubsystem<UPHCCitySubsystem>();
					if (CS)
					{
						for (int32 NI = 0; NI < UPHCCitySubsystem::NumNPCPlayers; ++NI)
						{
							if (CS->NPCPlayers[NI].Actor == Target)
							{
								CS->NPCPlayers[NI].Health -= Damage;
								CS->NPCPlayers[NI].DamageFlashTimer = 0.15f;
								if (CS->NPCPlayers[NI].Health <= 0.f)
								{
									const FVector NPCLoc = Target->GetActorLocation();
									SpawnExplosion(NPCLoc);
									CS->RespawnNPCPlayer(NI);
								}
								break;
							}
						}
					}
				}
				else if (APHCCityCharacter* HitChar = Cast<APHCCityCharacter>(Target))
				{
					HitChar->Health -= Damage;
					if (HitChar->Health <= 0.f)
					{
						HitChar->Deaths++;
						HitChar->Health = HitChar->MaxHealth;
						HitChar->SetActorLocation(FVector(0.f, 0.f, 100.f));
					}
					HitChar->DamageFlashTimer = 0.3f;
				}

				SpawnExplosion(CurLoc);
				Destroy();
				return;
			}
		}
	}

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
// RPG Explosion — uses APHCNukeExplosion (flame pack), matching AC-130 missiles
// ---------------------------------------------------------------------------

void APHCMissileProjectile::SpawnExplosion(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Spawn APHCNukeExplosion at impact — same as AC-130 105mm howitzer.
	// RPG rockets get a 0.5× actor scale so their blast reads as a tactical
	// strike rather than a full nuke; the scale cascades to the NS_NuclearExplosion
	// Niagara (child of the root) via normal transform inheritance.
	const FVector ExplosionScale = bIsRPGRocket ? FVector(0.5f) : FVector(1.f);
	const FTransform SpawnTM(FRotator::ZeroRotator, Location, ExplosionScale);
	APHCNukeExplosion* Exp = World->SpawnActorDeferred<APHCNukeExplosion>(
		APHCNukeExplosion::StaticClass(),
		SpawnTM,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Exp)
	{
		UGameplayStatics::FinishSpawningActor(Exp, SpawnTM);
	}

	// Damage nearby Lurkers (1500 UU radius, 100 damage — matches AC-130 105mm)
	if (UPHCCitySubsystem* Sub = World->GetSubsystem<UPHCCitySubsystem>())
	{
		for (int32 CI = 0; CI < UPHCCitySubsystem::NumGiantCrabs; ++CI)
		{
			if (Sub->Crabs[CI].Actor && Sub->Crabs[CI].Health > 0.f && Sub->Crabs[CI].bVisible)
			{
				const float D = FVector::Dist(Location, Sub->Crabs[CI].Actor->GetActorLocation());
				if (D < 1500.f)
				{
					Sub->Crabs[CI].Health -= 100.f;
					Sub->Crabs[CI].DamageFlashTimer = 0.2f;
					if (Sub->Crabs[CI].Health <= 0.f)
					{
						Sub->RespawnCrab(CI);
					}
				}
			}
		}
	}
}
