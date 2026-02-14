// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Jenga/JengaBlock.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

AJengaBlock::AJengaBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	// Box collision sized to Jenga block proportions
	BlockCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockCollision"));
	BlockCollision->SetBoxExtent(FVector(BlockLength * 0.5f, BlockWidth * 0.5f, BlockHeight * 0.5f));
	BlockCollision->SetCollisionProfileName(TEXT("PhysicsActor"));
	BlockCollision->SetSimulatePhysics(true);
	BlockCollision->SetLinearDamping(0.4f);
	BlockCollision->SetAngularDamping(0.6f);
	BlockCollision->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(BlockCollision);

	// Visual mesh
	BlockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	BlockMesh->SetupAttachment(BlockCollision);
	BlockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Scale a unit cube to match Jenga block dimensions
	// Default UE5 cube is 100x100x100 cm
	BlockMesh->SetRelativeScale3D(FVector(
		BlockLength / 100.f,
		BlockWidth / 100.f,
		BlockHeight / 100.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BlockMesh->SetStaticMesh(CubeMesh.Object);
	}
}

// ---------------------------------------------------------------------------

void AJengaBlock::BeginPlay()
{
	Super::BeginPlay();
	ConfigurePhysics();

	// Apply wood colour with slight random variation
	if (BlockMesh && BlockMesh->GetNumMaterials() > 0)
	{
		DynMaterial = BlockMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMaterial)
		{
			// Add slight random tint variation per block
			const float Variation = FMath::FRandRange(-0.06f, 0.06f);
			const FLinearColor Tinted(
				FMath::Clamp(BlockColor.R + Variation, 0.f, 1.f),
				FMath::Clamp(BlockColor.G + Variation * 0.8f, 0.f, 1.f),
				FMath::Clamp(BlockColor.B + Variation * 0.5f, 0.f, 1.f),
				1.f);

			BlockColor = Tinted;
			DynMaterial->SetVectorParameterValue(TEXT("BaseColor"), Tinted);
		}
	}
}

// ---------------------------------------------------------------------------

void AJengaBlock::SetHighlighted(bool bHighlight)
{
	if (!DynMaterial)
	{
		return;
	}

	if (bHighlight)
	{
		// Brighten the block to indicate selection
		const FLinearColor HighlightColor(
			FMath::Min(BlockColor.R + 0.3f, 1.f),
			FMath::Min(BlockColor.G + 0.3f, 1.f),
			FMath::Min(BlockColor.B + 0.15f, 1.f),
			1.f);
		DynMaterial->SetVectorParameterValue(TEXT("BaseColor"), HighlightColor);
	}
	else
	{
		DynMaterial->SetVectorParameterValue(TEXT("BaseColor"), BlockColor);
	}
}

// ---------------------------------------------------------------------------

bool AJengaBlock::IsMoving() const
{
	return GetSpeed() > StationaryThreshold;
}

// ---------------------------------------------------------------------------

float AJengaBlock::GetSpeed() const
{
	if (!BlockCollision)
	{
		return 0.f;
	}
	return BlockCollision->GetPhysicsLinearVelocity().Size();
}

// ---------------------------------------------------------------------------

void AJengaBlock::ConfigurePhysics()
{
	if (!BlockCollision)
	{
		return;
	}

	// Wood-like physics material: high friction, low bounce
	UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(this, TEXT("WoodPhysMat"));
	PhysMat->Friction = 0.65f;
	PhysMat->Restitution = 0.1f;
	PhysMat->RestitutionCombineMode = EFrictionCombineMode::Average;
	PhysMat->FrictionCombineMode = EFrictionCombineMode::Average;

	BlockCollision->BodyInstance.SetPhysMaterialOverride(PhysMat);

	// Jenga block mass (pine wood, approx dimensions)
	// Real Jenga block ~14g, we scale for game feel
	BlockCollision->SetMassOverrideInKg(NAME_None, 0.15f);
	BlockCollision->BodyInstance.MaxAngularVelocity = 800.f;
}
