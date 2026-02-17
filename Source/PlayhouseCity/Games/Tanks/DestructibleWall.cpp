// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Tanks/DestructibleWall.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

ADestructibleWall::ADestructibleWall()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	BoxCollision->SetBoxExtent(FVector(BlockSize * 0.5f));
	BoxCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BoxCollision->SetSimulatePhysics(false);
	SetRootComponent(BoxCollision);

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetupAttachment(BoxCollision);
	WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallMesh->SetRelativeScale3D(FVector(BlockSize / 100.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		WallMesh->SetStaticMesh(CubeMesh.Object);
	}
}

// ---------------------------------------------------------------------------

void ADestructibleWall::ApplyDamage(int32 DamageAmount)
{
	if (HP <= 0)
	{
		return;
	}

	HP -= DamageAmount;

	if (HP <= 0)
	{
		HP = 0;
		OnWallDestroyed.Broadcast(this);

		// Visual feedback: could play particle effect here
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);

		// Deferred destroy to allow delegates to fire
		SetLifeSpan(0.1f);
	}
	else
	{
		// Damage feedback: darken colour proportionally
		if (WallMesh && WallMesh->GetNumMaterials() > 0)
		{
			UMaterialInstanceDynamic* DynMat = WallMesh->CreateDynamicMaterialInstance(0);
			if (DynMat)
			{
				const float HealthFrac = static_cast<float>(HP) / static_cast<float>(MaxHP);
				const FLinearColor DamagedColor = FMath::Lerp(
					FLinearColor(0.3f, 0.15f, 0.05f, 1.f),  // heavily damaged
					FLinearColor(0.6f, 0.5f, 0.35f, 1.f),   // full health
					HealthFrac);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), DamagedColor);
			}
		}
	}
}
