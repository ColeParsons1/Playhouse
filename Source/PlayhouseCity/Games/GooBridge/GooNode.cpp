// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/GooBridge/GooNode.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Games/GooBridge/GooStrut.h"

// ---------------------------------------------------------------------------

AGooNode::AGooNode()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root collision sphere
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(NodeRadius);
	Collision->SetCollisionProfileName(TEXT("PhysicsActor"));
	Collision->SetSimulatePhysics(false); // starts kinematic until simulation
	Collision->SetLinearDamping(0.5f);
	Collision->SetAngularDamping(0.5f);
	Collision->BodyInstance.bNotifyRigidBodyCollision = true;
	SetRootComponent(Collision);

	// Visual mesh
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(NodeRadius * 2.f / 100.f)); // scale unit sphere

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
}

// ---------------------------------------------------------------------------

void AGooNode::BeginPlay()
{
	Super::BeginPlay();
	ConfigurePhysics();

	// Apply a green-ish goo colour
	if (Mesh && Mesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			const FLinearColor GooColor = bIsFixed
				? FLinearColor(0.6f, 0.6f, 0.6f, 1.f)   // grey for anchors
				: FLinearColor(0.1f, 0.85f, 0.2f, 1.f);  // green for player goo
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), GooColor);
		}
	}
}

// ---------------------------------------------------------------------------

void AGooNode::SetSimulating(bool bSim)
{
	if (!Collision)
	{
		return;
	}

	if (bSim)
	{
		// Store position for potential reset
		PreSimLocation = GetActorLocation();

		if (bIsFixed)
		{
			// Anchor nodes stay kinematic so the bridge is attached
			Collision->SetSimulatePhysics(false);
		}
		else
		{
			Collision->SetSimulatePhysics(true);
		}
	}
	else
	{
		// Disable physics and restore position
		Collision->SetSimulatePhysics(false);
		Collision->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Collision->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		SetActorLocation(PreSimLocation);
	}
}

// ---------------------------------------------------------------------------

void AGooNode::AddStrut(AGooStrut* Strut)
{
	if (Strut && !ConnectedStruts.Contains(Strut))
	{
		ConnectedStruts.Add(Strut);
	}
}

// ---------------------------------------------------------------------------

void AGooNode::RemoveStrut(AGooStrut* Strut)
{
	ConnectedStruts.Remove(Strut);
}

// ---------------------------------------------------------------------------

void AGooNode::ConfigurePhysics()
{
	if (!Collision)
	{
		return;
	}

	UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(this, TEXT("GooNodePhysMat"));
	PhysMat->Friction = 0.6f;
	PhysMat->Restitution = 0.2f;
	PhysMat->RestitutionCombineMode = EFrictionCombineMode::Average;
	PhysMat->FrictionCombineMode = EFrictionCombineMode::Average;

	Collision->BodyInstance.SetPhysMaterialOverride(PhysMat);
	Collision->SetMassOverrideInKg(NAME_None, 0.5f); // 500g per goo node
}
