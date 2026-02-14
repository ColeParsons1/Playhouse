// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/CelDamage/CelDamageVehicle.h"
#include "Games/CelDamage/CelDamageProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------

ACelDamageVehicle::ACelDamageVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	// Collision box — root
	VehicleCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	VehicleCollision->SetBoxExtent(FVector(120.f, 60.f, 40.f));
	VehicleCollision->SetCollisionProfileName(TEXT("Pawn"));
	VehicleCollision->SetSimulatePhysics(false);
	SetRootComponent(VehicleCollision);

	// Body mesh
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(VehicleCollision);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(2.4f, 1.2f, 0.8f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;

	if (Cube)
	{
		BodyMesh->SetStaticMesh(Cube);
	}

	// Wheels (4 cubes at corners)
	auto MakeWheel = [&](const TCHAR* Name, const FVector& RelPos) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		W->SetupAttachment(VehicleCollision);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetRelativeLocation(RelPos);
		W->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));
		if (Cube)
		{
			W->SetStaticMesh(Cube);
		}
		return W;
	};

	WheelFL = MakeWheel(TEXT("WheelFL"), FVector(80.f, -50.f, -30.f));
	WheelFR = MakeWheel(TEXT("WheelFR"), FVector(80.f, 50.f, -30.f));
	WheelRL = MakeWheel(TEXT("WheelRL"), FVector(-80.f, -50.f, -30.f));
	WheelRR = MakeWheel(TEXT("WheelRR"), FVector(-80.f, 50.f, -30.f));

	// Weapon mesh on top
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(VehicleCollision);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetRelativeLocation(FVector(60.f, 0.f, 40.f));
	WeaponMesh->SetRelativeScale3D(FVector(0.6f, 0.3f, 0.3f));
	if (Cube)
	{
		WeaponMesh->SetStaticMesh(Cube);
	}

	// Camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleCollision);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	// Camera
	VehicleCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VehicleCamera"));
	VehicleCamera->SetupAttachment(CameraBoom);
}

// ---------------------------------------------------------------------------

void ACelDamageVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Health <= 0.f)
	{
		return;
	}

	// Invincibility countdown
	if (bIsInvincible)
	{
		InvincibilityTimer -= DeltaTime;
		if (InvincibilityTimer <= 0.f)
		{
			bIsInvincible = false;
		}
	}

	// Fire cooldown
	if (FireTimer > 0.f)
	{
		FireTimer -= DeltaTime;
	}

	// AI
	if (bIsCPU)
	{
		CPUThink(DeltaTime);
	}

	UpdateMovement(DeltaTime);
	UpdateWheelSpin(DeltaTime);
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

void ACelDamageVehicle::SetThrottle(float Value)
{
	ThrottleInput = FMath::Clamp(Value, -1.f, 1.f);
}

void ACelDamageVehicle::SetSteering(float Value)
{
	SteeringInput = FMath::Clamp(Value, -1.f, 1.f);
}

void ACelDamageVehicle::UpdateMovement(float DT)
{
	// Acceleration / braking
	if (FMath::Abs(ThrottleInput) > 0.01f)
	{
		const bool bBraking = (ThrottleInput < 0.f && CurrentSpeed > 0.f) ||
							  (ThrottleInput > 0.f && CurrentSpeed < 0.f);
		if (bBraking)
		{
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.f, DT, BrakeRate / MaxSpeed);
		}
		else
		{
			CurrentSpeed += ThrottleInput * AccelerationRate * DT;
		}
	}
	else
	{
		// Natural deceleration
		if (CurrentSpeed > 0.f)
		{
			CurrentSpeed = FMath::Max(0.f, CurrentSpeed - NaturalDecel * DT);
		}
		else if (CurrentSpeed < 0.f)
		{
			CurrentSpeed = FMath::Min(0.f, CurrentSpeed + NaturalDecel * DT);
		}
	}

	CurrentSpeed = FMath::Clamp(CurrentSpeed, -MaxSpeed * 0.4f, MaxSpeed);

	// Steering (only when moving)
	if (FMath::Abs(CurrentSpeed) > 10.f)
	{
		const float SpeedFraction = FMath::Abs(CurrentSpeed) / MaxSpeed;
		const float TurnRate = SteeringSpeed * SpeedFraction;
		FRotator Rot = GetActorRotation();
		Rot.Yaw += SteeringInput * TurnRate * DT;
		SetActorRotation(Rot);
	}

	// Move forward
	const FVector Forward = GetActorForwardVector();
	FVector NewLoc = GetActorLocation() + Forward * CurrentSpeed * DT;

	// Clamp to arena bounds
	NewLoc.X = FMath::Clamp(NewLoc.X, ArenaCenter.X - ArenaHalfSize, ArenaCenter.X + ArenaHalfSize);
	NewLoc.Y = FMath::Clamp(NewLoc.Y, ArenaCenter.Y - ArenaHalfSize, ArenaCenter.Y + ArenaHalfSize);

	SetActorLocation(NewLoc);
}

void ACelDamageVehicle::UpdateWheelSpin(float DT)
{
	WheelSpinAngle += CurrentSpeed * DT * 0.5f;

	const FRotator WheelRot(WheelSpinAngle, 0.f, 0.f);
	if (WheelFL) { WheelFL->SetRelativeRotation(WheelRot); }
	if (WheelFR) { WheelFR->SetRelativeRotation(WheelRot); }
	if (WheelRL) { WheelRL->SetRelativeRotation(WheelRot); }
	if (WheelRR) { WheelRR->SetRelativeRotation(WheelRot); }
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

void ACelDamageVehicle::FireWeapon()
{
	if (FireTimer > 0.f || Health <= 0.f)
	{
		return;
	}

	FireTimer = FireCooldown;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector MuzzleLoc = GetMuzzleLocation();
	const FRotator MuzzleRot = GetActorRotation();

	switch (CurrentWeapon)
	{
	case ECelDamageWeaponType::Fists:
	{
		// Melee — trace forward, short range
		TArray<FHitResult> Hits;
		const FVector End = MuzzleLoc + GetActorForwardVector() * 250.f;
		FCollisionShape Shape;
		Shape.SetSphere(60.f);
		if (World->SweepMultiByChannel(Hits, MuzzleLoc, End, FQuat::Identity,
			ECC_Pawn, Shape))
		{
			for (const FHitResult& Hit : Hits)
			{
				ACelDamageVehicle* Other = Cast<ACelDamageVehicle>(Hit.GetActor());
				if (Other && Other != this)
				{
					FDamageEvent DmgEvent;
					Other->TakeDamage(10.f, DmgEvent, nullptr, this);
					OnVehicleHit.Broadcast(this, Other);
					break;
				}
			}
		}
		break;
	}

	case ECelDamageWeaponType::Bat:
	{
		// Melee — larger range
		TArray<FHitResult> Hits;
		const FVector End = MuzzleLoc + GetActorForwardVector() * 350.f;
		FCollisionShape Shape;
		Shape.SetSphere(80.f);
		if (World->SweepMultiByChannel(Hits, MuzzleLoc, End, FQuat::Identity,
			ECC_Pawn, Shape))
		{
			for (const FHitResult& Hit : Hits)
			{
				ACelDamageVehicle* Other = Cast<ACelDamageVehicle>(Hit.GetActor());
				if (Other && Other != this)
				{
					FDamageEvent DmgEvent;
					Other->TakeDamage(25.f, DmgEvent, nullptr, this);
					OnVehicleHit.Broadcast(this, Other);
					break;
				}
			}
		}
		CurrentWeapon = ECelDamageWeaponType::Fists;
		UpdateWeaponMeshColor();
		break;
	}

	case ECelDamageWeaponType::FreezeRay:
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACelDamageProjectile* Proj = World->SpawnActor<ACelDamageProjectile>(
			ACelDamageProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
		if (Proj)
		{
			Proj->Damage = 15.f;
			Proj->Speed = 3500.f;
			Proj->OwnerVehicle = this;
		}
		CurrentWeapon = ECelDamageWeaponType::Fists;
		UpdateWeaponMeshColor();
		break;
	}

	case ECelDamageWeaponType::Cannon:
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACelDamageProjectile* Proj = World->SpawnActor<ACelDamageProjectile>(
			ACelDamageProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
		if (Proj)
		{
			Proj->Damage = 30.f;
			Proj->Speed = 5000.f;
			Proj->OwnerVehicle = this;
		}
		CurrentWeapon = ECelDamageWeaponType::Fists;
		UpdateWeaponMeshColor();
		break;
	}

	case ECelDamageWeaponType::TNT:
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACelDamageProjectile* Proj = World->SpawnActor<ACelDamageProjectile>(
			ACelDamageProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
		if (Proj)
		{
			Proj->Damage = 40.f;
			Proj->Speed = 2500.f;
			Proj->SplashRadius = 300.f;
			Proj->SmackValue = 2;
			Proj->OwnerVehicle = this;
		}
		CurrentWeapon = ECelDamageWeaponType::Fists;
		UpdateWeaponMeshColor();
		break;
	}

	case ECelDamageWeaponType::Homing:
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACelDamageProjectile* Proj = World->SpawnActor<ACelDamageProjectile>(
			ACelDamageProjectile::StaticClass(), MuzzleLoc, MuzzleRot, SP);
		if (Proj)
		{
			Proj->Damage = 35.f;
			Proj->Speed = 4000.f;
			Proj->bIsHoming = true;
			Proj->SmackValue = 2;
			Proj->OwnerVehicle = this;
			Proj->HomingTarget = CPUTarget;
		}
		CurrentWeapon = ECelDamageWeaponType::Fists;
		UpdateWeaponMeshColor();
		break;
	}
	}
}

void ACelDamageVehicle::EquipWeapon(ECelDamageWeaponType NewWeapon)
{
	CurrentWeapon = NewWeapon;
	UpdateWeaponMeshColor();
}

void ACelDamageVehicle::UpdateWeaponMeshColor()
{
	if (!WeaponMesh || WeaponMesh->GetNumMaterials() == 0)
	{
		return;
	}

	FLinearColor Color;
	switch (CurrentWeapon)
	{
	case ECelDamageWeaponType::Fists:     Color = FLinearColor(0.5f, 0.5f, 0.5f, 1.f); break;
	case ECelDamageWeaponType::Bat:       Color = FLinearColor(0.6f, 0.35f, 0.1f, 1.f); break;
	case ECelDamageWeaponType::FreezeRay: Color = FLinearColor(0.3f, 0.7f, 0.95f, 1.f); break;
	case ECelDamageWeaponType::Cannon:    Color = FLinearColor(0.7f, 0.15f, 0.1f, 1.f); break;
	case ECelDamageWeaponType::TNT:       Color = FLinearColor(0.95f, 0.5f, 0.1f, 1.f); break;
	case ECelDamageWeaponType::Homing:    Color = FLinearColor(0.6f, 0.2f, 0.8f, 1.f); break;
	default:                              Color = FLinearColor(0.5f, 0.5f, 0.5f, 1.f); break;
	}

	UMaterialInstanceDynamic* DynMat = WeaponMesh->CreateDynamicMaterialInstance(0);
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
	}
}

FVector ACelDamageVehicle::GetMuzzleLocation() const
{
	return GetActorLocation() + GetActorForwardVector() * 150.f;
}

// ---------------------------------------------------------------------------
// Damage & respawn
// ---------------------------------------------------------------------------

float ACelDamageVehicle::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
									AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsInvincible || Health <= 0.f)
	{
		return 0.f;
	}

	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0.f, Health - Actual);

	if (Health <= 0.f)
	{
		// Broadcast destruction — GameMode will handle respawn.
		OnVehicleDestroyed.Broadcast(this);
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		CurrentSpeed = 0.f;
	}

	return Actual;
}

void ACelDamageVehicle::Respawn(const FVector& SpawnLocation)
{
	Health = MaxHealth;
	CurrentSpeed = 0.f;
	ThrottleInput = 0.f;
	SteeringInput = 0.f;
	CurrentWeapon = ECelDamageWeaponType::Fists;
	FireTimer = 0.f;

	bIsInvincible = true;
	InvincibilityTimer = InvincibilityDuration;

	SetActorLocation(SpawnLocation);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	UpdateWeaponMeshColor();
}

// ---------------------------------------------------------------------------
// CPU AI
// ---------------------------------------------------------------------------

void ACelDamageVehicle::SetCPUTarget(AActor* InTarget)
{
	CPUTarget = InTarget;
}

void ACelDamageVehicle::SetNearestPickup(AActor* InPickup)
{
	CPUPickupTarget = InPickup;
}

void ACelDamageVehicle::CPUThink(float DT)
{
	if (Health <= 0.f || !CPUTarget)
	{
		SetThrottle(0.f);
		SetSteering(0.f);
		return;
	}

	AIStateTimer -= DT;

	const FVector MyLoc = GetActorLocation();
	const FVector TargetLoc = CPUTarget->GetActorLocation();
	const float DistToTarget = FVector::Dist(MyLoc, TargetLoc);

	// State transitions
	if (AIStateTimer <= 0.f)
	{
		if (Health < 30.f && DistToTarget < 2000.f)
		{
			AIState = EAIState::Fleeing;
			AIStateTimer = 2.f;
		}
		else if (CurrentWeapon == ECelDamageWeaponType::Fists && CPUPickupTarget)
		{
			AIState = EAIState::SeekingPickup;
			AIStateTimer = 3.f;
		}
		else if (DistToTarget < 4000.f)
		{
			AIState = EAIState::Attacking;
			AIStateTimer = 2.f;
		}
		else
		{
			AIState = EAIState::Roaming;
			AIStateTimer = FMath::RandRange(1.f, 2.5f);
			AIRoamYaw = FMath::RandRange(-180.f, 180.f);
		}
	}

	// Execute state
	switch (AIState)
	{
	case EAIState::Roaming:
	{
		SetThrottle(0.6f);
		// Steer toward roam yaw
		const float CurrentYaw = GetActorRotation().Yaw;
		const float Delta = FMath::FindDeltaAngleDegrees(CurrentYaw, AIRoamYaw);
		SetSteering(FMath::Clamp(Delta / 45.f, -1.f, 1.f));
		break;
	}

	case EAIState::SeekingPickup:
	{
		if (CPUPickupTarget)
		{
			const FVector Dir = (CPUPickupTarget->GetActorLocation() - MyLoc).GetSafeNormal2D();
			const float DesiredYaw = FMath::Atan2(Dir.Y, Dir.X) * (180.f / PI);
			const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw);
			SetSteering(FMath::Clamp(Delta / 30.f, -1.f, 1.f));
			SetThrottle(0.8f);
		}
		else
		{
			AIState = EAIState::Roaming;
			AIStateTimer = 1.f;
		}
		break;
	}

	case EAIState::Attacking:
	{
		const FVector Dir = (TargetLoc - MyLoc).GetSafeNormal2D();
		const float DesiredYaw = FMath::Atan2(Dir.Y, Dir.X) * (180.f / PI);
		const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw);
		SetSteering(FMath::Clamp(Delta / 30.f, -1.f, 1.f));
		SetThrottle(0.9f);

		// Fire when roughly aimed and in range
		CPUFireAccumulator += DT;
		if (FMath::Abs(Delta) < 20.f && DistToTarget < 2500.f && CPUFireAccumulator > 0.5f)
		{
			FireWeapon();
			CPUFireAccumulator = 0.f;
		}
		break;
	}

	case EAIState::Fleeing:
	{
		const FVector Dir = (MyLoc - TargetLoc).GetSafeNormal2D();
		const float DesiredYaw = FMath::Atan2(Dir.Y, Dir.X) * (180.f / PI);
		const float Delta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw);
		SetSteering(FMath::Clamp(Delta / 30.f, -1.f, 1.f));
		SetThrottle(1.f);
		break;
	}
	}
}
