// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/GooBridge/GooStrut.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Games/GooBridge/GooNode.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// ---------------------------------------------------------------------------

AGooStrut::AGooStrut()
{
	PrimaryActorTick.bCanEverTick = true;

	// Scene root
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Physics constraint between two nodes
	Constraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Constraint"));
	Constraint->SetupAttachment(Root);

	// Configure constraint defaults (distance-like constraint)
	Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, 10.f);
	Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Limited, 10.f);
	Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Limited, 10.f);
	Constraint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.f);
	Constraint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Free, 0.f);
	Constraint->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Free, 0.f);

	// Visual strut mesh (thin cylinder between nodes)
	StrutMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StrutMesh"));
	StrutMesh->SetupAttachment(Root);
	StrutMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		StrutMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

// ---------------------------------------------------------------------------

void AGooStrut::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!NodeA || !NodeB)
	{
		return;
	}

	// Keep visual up-to-date
	UpdateVisual();

	// Check if the strut should break during simulation
	if (CheckBreak())
	{
		BreakStrut();
	}
}

// ---------------------------------------------------------------------------

void AGooStrut::Initialize(AGooNode* A, AGooNode* B)
{
	if (!A || !B || A == B)
	{
		return;
	}

	NodeA = A;
	NodeB = B;

	// Calculate rest length
	RestLength = FVector::Dist(A->GetActorLocation(), B->GetActorLocation());

	// Position the strut actor between the two nodes
	const FVector MidPoint = (A->GetActorLocation() + B->GetActorLocation()) * 0.5f;
	SetActorLocation(MidPoint);

	// Setup the physics constraint between the two node collision components
	if (Constraint && A->Collision && B->Collision)
	{
		Constraint->SetConstrainedComponents(A->Collision, NAME_None, B->Collision, NAME_None);

		// Set linear limits based on rest length (allow small play)
		const float LinearLimit = RestLength * (MaxStretch - 1.f) * 0.5f;
		Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, LinearLimit);
		Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Limited, LinearLimit);
		Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Limited, LinearLimit);

		// Soft constraint for more organic feel
		Constraint->SetLinearPositionDrive(true, true, true);
		Constraint->SetLinearDriveParams(500.f, 50.f, 0.f); // spring, damping, force limit
	}

	// Register with both nodes
	A->AddStrut(this);
	B->AddStrut(this);

	// Apply initial visual
	UpdateVisual();

	// Apply a dark-green colour to the strut
	if (StrutMesh && StrutMesh->GetNumMaterials() > 0)
	{
		UMaterialInstanceDynamic* DynMat = StrutMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(
				TEXT("BaseColor"), FLinearColor(0.05f, 0.5f, 0.1f, 1.f));
		}
	}
}

// ---------------------------------------------------------------------------

void AGooStrut::UpdateVisual()
{
	if (!NodeA || !NodeB || !StrutMesh)
	{
		return;
	}

	const FVector PosA = NodeA->GetActorLocation();
	const FVector PosB = NodeB->GetActorLocation();
	const FVector MidPoint = (PosA + PosB) * 0.5f;
	const float Distance = FVector::Dist(PosA, PosB);

	// Position at midpoint
	StrutMesh->SetWorldLocation(MidPoint);

	// Orient cylinder along the direction between nodes
	// UE5 cylinder is aligned along Z by default, height = 100 units
	const FVector Direction = (PosB - PosA).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		const FRotator Rotation = Direction.Rotation();
		// Cylinder Z-axis needs to point along the direction, so rotate 90 degrees on pitch
		StrutMesh->SetWorldRotation(FRotator(Rotation.Pitch + 90.f, Rotation.Yaw, Rotation.Roll));
	}

	// Scale: X/Y for radius, Z for length
	// Default cylinder is 100 units tall, 100 units diameter
	const float RadiusScale = StrutRadius / 50.f;
	const float HeightScale = Distance / 100.f;
	StrutMesh->SetWorldScale3D(FVector(RadiusScale, RadiusScale, HeightScale));
}

// ---------------------------------------------------------------------------

bool AGooStrut::CheckBreak() const
{
	if (!NodeA || !NodeB || RestLength <= 0.f)
	{
		return false;
	}

	const float CurrentDistance = FVector::Dist(
		NodeA->GetActorLocation(), NodeB->GetActorLocation());
	return CurrentDistance > (RestLength * MaxStretch);
}

// ---------------------------------------------------------------------------

void AGooStrut::BreakStrut()
{
	// Unregister from nodes
	if (NodeA)
	{
		NodeA->RemoveStrut(this);
	}
	if (NodeB)
	{
		NodeB->RemoveStrut(this);
	}

	// Break the constraint
	if (Constraint)
	{
		Constraint->BreakConstraint();
	}

	NodeA = nullptr;
	NodeB = nullptr;

	Destroy();
}
