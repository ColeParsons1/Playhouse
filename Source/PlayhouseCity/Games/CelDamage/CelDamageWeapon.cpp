// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/CelDamage/CelDamageWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

ACelDamageWeapon::ACelDamageWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	PickupCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PickupCollision"));
	PickupCollision->SetBoxExtent(FVector(40.f));
	PickupCollision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	PickupCollision->SetGenerateOverlapEvents(true);
	SetRootComponent(PickupCollision);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(PickupCollision);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetRelativeScale3D(FVector(0.6f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PickupMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ACelDamageWeapon::BeginPlay()
{
	Super::BeginPlay();
	OriginalLocation = GetActorLocation();
	ApplyWeaponColor();
}

void ACelDamageWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsActive)
	{
		// Bob up and down
		BobPhase += DeltaTime * 2.f;
		const float BobOffset = FMath::Sin(BobPhase) * 30.f;
		FVector Loc = OriginalLocation;
		Loc.Z += BobOffset;
		SetActorLocation(Loc);

		// Rotate slowly
		AddActorWorldRotation(FRotator(0.f, 60.f * DeltaTime, 0.f));

		// Check for overlapping vehicles
		TArray<AActor*> OverlappingActors;
		GetOverlappingActors(OverlappingActors, ACelDamageVehicle::StaticClass());
		for (AActor* Actor : OverlappingActors)
		{
			ACelDamageVehicle* Vehicle = Cast<ACelDamageVehicle>(Actor);
			if (Vehicle && Vehicle->Health > 0.f)
			{
				Collect(Vehicle);
				break;
			}
		}
	}
	else
	{
		// Respawn countdown
		RespawnTimer -= DeltaTime;
		if (RespawnTimer <= 0.f)
		{
			RespawnPickup();
		}
	}
}

void ACelDamageWeapon::Collect(ACelDamageVehicle* Collector)
{
	if (!bIsActive || !Collector)
	{
		return;
	}

	Collector->EquipWeapon(WeaponType);
	bIsActive = false;
	RespawnTimer = RespawnTime;

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ACelDamageWeapon::RespawnPickup()
{
	// Randomize weapon type (exclude Fists)
	const TArray<ECelDamageWeaponType> Pool = {
		ECelDamageWeaponType::Bat,
		ECelDamageWeaponType::FreezeRay,
		ECelDamageWeaponType::Cannon,
		ECelDamageWeaponType::TNT,
		ECelDamageWeaponType::Homing
	};
	WeaponType = Pool[FMath::RandRange(0, Pool.Num() - 1)];

	ApplyWeaponColor();
	SetActorLocation(OriginalLocation);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	bIsActive = true;
}

void ACelDamageWeapon::ApplyWeaponColor()
{
	if (!PickupMesh || PickupMesh->GetNumMaterials() == 0)
	{
		return;
	}

	UMaterialInstanceDynamic* DynMat = PickupMesh->CreateDynamicMaterialInstance(0);
	if (DynMat)
	{
		DynMat->SetVectorParameterValue(TEXT("BaseColor"), GetColorForWeapon(WeaponType));
	}
}

FLinearColor ACelDamageWeapon::GetColorForWeapon(ECelDamageWeaponType Type)
{
	switch (Type)
	{
	case ECelDamageWeaponType::Fists:     return FLinearColor(0.5f, 0.5f, 0.5f, 1.f);
	case ECelDamageWeaponType::Bat:       return FLinearColor(0.6f, 0.35f, 0.1f, 1.f);
	case ECelDamageWeaponType::FreezeRay: return FLinearColor(0.3f, 0.7f, 0.95f, 1.f);
	case ECelDamageWeaponType::Cannon:    return FLinearColor(0.7f, 0.15f, 0.1f, 1.f);
	case ECelDamageWeaponType::TNT:       return FLinearColor(0.95f, 0.5f, 0.1f, 1.f);
	case ECelDamageWeaponType::Homing:    return FLinearColor(0.6f, 0.2f, 0.8f, 1.f);
	default:                              return FLinearColor(0.5f, 0.5f, 0.5f, 1.f);
	}
}
