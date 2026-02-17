// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/AngryDucks/DestructibleBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

ADestructibleBlock::ADestructibleBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	BoxCollision->SetBoxExtent(FVector(BlockHalfExtent));
	BoxCollision->SetCollisionProfileName(TEXT("PhysicsActor"));
	BoxCollision->SetSimulatePhysics(true);
	BoxCollision->SetEnableGravity(true);
	BoxCollision->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(BoxCollision);

	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	BlockMesh->SetupAttachment(BoxCollision);
	BlockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlockMesh->SetRelativeScale3D(FVector(BlockHalfExtent / 50.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BlockMesh->SetStaticMesh(CubeMesh.Object);
	}
}

// ---------------------------------------------------------------------------
// -- API --
// ---------------------------------------------------------------------------

void ADestructibleBlock::SetBlockType(EBlockType NewType)
{
	BlockType = NewType;

	switch (BlockType)
	{
	case EBlockType::Wood:
		MaxHP = 1.f;
		break;
	case EBlockType::Stone:
		MaxHP = 2.f;
		break;
	case EBlockType::Glass:
		MaxHP = 1.f;
		break;
	}

	HP = MaxHP;
	ApplyColor();
}

void ADestructibleBlock::ApplyDamage(float DamageAmount)
{
	if (HP <= 0.f)
	{
		return;
	}

	HP -= DamageAmount;

	if (HP <= 0.f)
	{
		HP = 0.f;
		OnBlockDestroyed.Broadcast(this);

		// Visual feedback -- hide and disable collision immediately.
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);

		// Deferred destroy to allow delegates to fire.
		SetLifeSpan(0.1f);
	}
	else
	{
		// Damage feedback: tint the colour darker proportionally.
		ApplyColor();
	}
}

FLinearColor ADestructibleBlock::GetBlockColor() const
{
	const float HealthFrac = (MaxHP > 0.f)
		? FMath::Clamp(HP / MaxHP, 0.f, 1.f)
		: 1.f;

	FLinearColor FullColor;
	FLinearColor DamagedColor;

	switch (BlockType)
	{
	case EBlockType::Wood:
		FullColor = FLinearColor(0.6f, 0.35f, 0.1f, 1.f);    // warm brown
		DamagedColor = FLinearColor(0.3f, 0.15f, 0.05f, 1.f); // dark brown
		break;
	case EBlockType::Stone:
		FullColor = FLinearColor(0.55f, 0.55f, 0.55f, 1.f);   // medium gray
		DamagedColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.f); // dark gray
		break;
	case EBlockType::Glass:
		FullColor = FLinearColor(0.6f, 0.85f, 0.95f, 1.f);    // light blue
		DamagedColor = FLinearColor(0.3f, 0.5f, 0.6f, 1.f);   // darker blue
		break;
	}

	return FMath::Lerp(DamagedColor, FullColor, HealthFrac);
}

// ---------------------------------------------------------------------------
// -- Visuals --
// ---------------------------------------------------------------------------

void ADestructibleBlock::ApplyColor()
{
	if (BlockMesh && BlockMesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat = BlockMesh->CreateDynamicMaterialInstance(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), GetBlockColor());
		}
	}
}
