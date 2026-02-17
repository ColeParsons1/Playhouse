// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Tanks/TankPawn.h"
#include "Games/Tanks/TankProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

// ---------------------------------------------------------------------------

ATankPawn::ATankPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Body collision
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(FVector(30.f, 20.f, 12.f));
	Body->SetCollisionProfileName(TEXT("Pawn"));
	Body->SetSimulatePhysics(false);
	SetRootComponent(Body);

	// Tank body mesh
	TankMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TankMesh"));
	TankMesh->SetupAttachment(Body);
	TankMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TankMesh->SetRelativeScale3D(FVector(0.6f, 0.4f, 0.2f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		TankMesh->SetStaticMesh(CubeMesh.Object);
	}

	// Turret mesh (smaller cube on top, rotates independently)
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(Body);
	TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TurretMesh->SetRelativeLocation(FVector(0.f, 0.f, 14.f));
	TurretMesh->SetRelativeScale3D(FVector(0.45f, 0.2f, 0.12f));
	if (CubeMesh.Succeeded())
	{
		TurretMesh->SetStaticMesh(CubeMesh.Object);
	}

	// Default projectile class
	ProjectileClass = ATankProjectile::StaticClass();
}

// ---------------------------------------------------------------------------

void ATankPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Fire cooldown
	if (FireTimer > 0.f)
	{
		FireTimer -= DeltaTime;
	}

	// CPU AI
	if (bIsCPU)
	{
		CPUThink(DeltaTime);
	}
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

void ATankPawn::MoveForward(float Value)
{
	if (FMath::IsNearlyZero(Value) || HP <= 0)
	{
		return;
	}

	const FVector Forward = GetActorForwardVector();
	const FVector Delta = Forward * Value * MoveSpeed * GetWorld()->GetDeltaSeconds();
	AddActorWorldOffset(Delta, true);
}

// ---------------------------------------------------------------------------

void ATankPawn::Turn(float Value)
{
	if (FMath::IsNearlyZero(Value) || HP <= 0)
	{
		return;
	}

	const float YawDelta = Value * RotationSpeed * GetWorld()->GetDeltaSeconds();
	AddActorWorldRotation(FRotator(0.f, YawDelta, 0.f));
}

// ---------------------------------------------------------------------------

void ATankPawn::AimTurret(FVector2D WorldTarget)
{
	if (HP <= 0 || !TurretMesh)
	{
		return;
	}

	const FVector TurretPos = TurretMesh->GetComponentLocation();
	const FVector TargetPos(WorldTarget.X, WorldTarget.Y, TurretPos.Z);
	const FRotator DesiredRot = UKismetMathLibrary::FindLookAtRotation(TurretPos, TargetPos);

	// Smooth rotation
	const FRotator CurrentRot = TurretMesh->GetComponentRotation();
	const float DT = GetWorld()->GetDeltaSeconds();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DT, TurretRotationSpeed / 30.f);

	// Only rotate yaw (top-down)
	TurretMesh->SetWorldRotation(FRotator(0.f, NewRot.Yaw, 0.f));
}

// ---------------------------------------------------------------------------

ATankProjectile* ATankPawn::Fire()
{
	if (HP <= 0 || FireTimer > 0.f)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World || !ProjectileClass)
	{
		return nullptr;
	}

	FireTimer = FireCooldown;

	const FVector MuzzleLoc = GetMuzzleLocation();
	const FRotator MuzzleRot = TurretMesh
		? FRotator(0.f, TurretMesh->GetComponentRotation().Yaw, 0.f)
		: GetActorRotation();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.Owner = this;

	ATankProjectile* Proj = World->SpawnActor<ATankProjectile>(
		ProjectileClass, MuzzleLoc, MuzzleRot, SP);

	if (Proj)
	{
		Proj->OwnerTank = this;
		// Ignore collision with owner
		if (Proj->CollisionSphere && Body)
		{
			Proj->CollisionSphere->IgnoreActorWhenMoving(this, true);
		}
	}

	return Proj;
}

// ---------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------

float ATankPawn::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
							AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	HP -= FMath::RoundToInt32(ActualDamage);
	if (HP <= 0)
	{
		HP = 0;
		OnTankDestroyed.Broadcast(this);

		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
	}
	else
	{
		// Damage flash: tint the mesh red briefly
		if (TankMesh && TankMesh->GetNumMaterials() > 0)
		{
			UMaterialInstanceDynamic* DynMat = TankMesh->CreateDynamicMaterialInstance(0);
			if (DynMat)
			{
				const float HealthFrac = static_cast<float>(HP) / static_cast<float>(MaxHP);
				const FLinearColor TintColor = FMath::Lerp(
					FLinearColor(1.f, 0.2f, 0.2f, 1.f),
					bIsCPU ? FLinearColor(0.3f, 0.3f, 0.8f, 1.f) : FLinearColor(0.2f, 0.7f, 0.2f, 1.f),
					HealthFrac);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), TintColor);
			}
		}
	}

	return ActualDamage;
}

// ---------------------------------------------------------------------------
// CPU AI
// ---------------------------------------------------------------------------

void ATankPawn::SetCPUTarget(AActor* InTarget)
{
	CPUTarget = InTarget;
}

// ---------------------------------------------------------------------------

void ATankPawn::CPUThink(float DT)
{
	if (HP <= 0 || !CPUTarget)
	{
		return;
	}

	const FVector MyPos = GetActorLocation();
	const FVector TargetPos = CPUTarget->GetActorLocation();
	const float DistToTarget = FVector::Dist2D(MyPos, TargetPos);

	// Aim turret at player
	AimTurret(FVector2D(TargetPos.X, TargetPos.Y));

	// Movement: approach the player, change direction periodically
	CPUDirectionChangeTimer -= DT;
	if (CPUDirectionChangeTimer <= 0.f)
	{
		CPUDirectionChangeTimer = FMath::FRandRange(1.5f, 4.f);
		CPUCurrentMoveDir = FMath::FRandRange(-1.f, 1.f);
	}

	if (DistToTarget > 300.f)
	{
		// Move toward the player
		const FVector ToTarget = (TargetPos - MyPos).GetSafeNormal2D();
		const FVector Forward = GetActorForwardVector();
		const float Dot = FVector::DotProduct(Forward, ToTarget);

		if (Dot > 0.5f)
		{
			MoveForward(0.7f);
		}
		else
		{
			// Turn toward the target
			const float Cross = FVector::CrossProduct(Forward, ToTarget).Z;
			Turn(FMath::Sign(Cross) * 0.8f);
			MoveForward(0.3f);
		}
	}
	else if (DistToTarget < 150.f)
	{
		// Too close: back away
		MoveForward(-0.5f);
		Turn(CPUCurrentMoveDir * 0.5f);
	}
	else
	{
		// Strafe
		Turn(CPUCurrentMoveDir * 0.6f);
		MoveForward(0.4f);
	}

	// Fire when roughly aimed at target
	CPUFireAccumulator += DT;
	if (CPUFireAccumulator >= FireCooldown + FMath::FRandRange(0.f, 0.5f))
	{
		if (TurretMesh)
		{
			const FVector TurretFwd = TurretMesh->GetForwardVector();
			const FVector ToTarget2D = (TargetPos - MyPos).GetSafeNormal2D();
			const float AimDot = FVector::DotProduct(TurretFwd, ToTarget2D);

			if (AimDot > 0.85f)
			{
				Fire();
				CPUFireAccumulator = 0.f;
			}
		}
	}
}

// ---------------------------------------------------------------------------

FVector ATankPawn::GetMuzzleLocation() const
{
	if (TurretMesh)
	{
		// Muzzle at the front of the turret
		return TurretMesh->GetComponentLocation()
			+ TurretMesh->GetForwardVector() * 35.f;
	}
	return GetActorLocation() + GetActorForwardVector() * 35.f;
}
