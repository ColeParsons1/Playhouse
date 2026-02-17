// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Pool/PoolCue.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"

// ---------------------------------------------------------------------------

APoolCue::APoolCue()
{
	PrimaryActorTick.bCanEverTick = true;

	CueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CueMesh"));
	SetRootComponent(CueMesh);
	CueMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default mesh: a thin elongated cylinder approximated by a scaled cube
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		CueMesh->SetStaticMesh(CylMesh.Object);
		// Scale to cue-stick proportions: long, thin
		CueMesh->SetRelativeScale3D(FVector(0.012f, 0.012f, 1.45f));
	}
}

// ---------------------------------------------------------------------------

void APoolCue::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Strike animation: snap forward then reset
	if (bStriking)
	{
		StrikeTimer += DeltaTime;
		if (StrikeTimer >= StrikeDuration)
		{
			bStriking = false;
			StrikeTimer = 0.f;
			// Reset pull-back
			SetPowerLevel(0.f);
		}
		else
		{
			// Lerp forward during strike
			const float Alpha = StrikeTimer / StrikeDuration;
			const float Offset = FMath::Lerp(MaxPullBack * CurrentPower, -5.f, Alpha);
			CueMesh->SetRelativeLocation(-AimDir * Offset);
		}
	}

	// Render aim line every frame if visible
	if (bAimLineVisible)
	{
		DrawAimLine();
	}
}

// ---------------------------------------------------------------------------

void APoolCue::SetAimDirection(FVector Direction)
{
	AimDir = Direction.GetSafeNormal();
	if (AimDir.IsNearlyZero())
	{
		AimDir = FVector::ForwardVector;
	}

	// Rotate cue to face the aim direction (tip toward cue ball)
	const FRotator Rot = AimDir.Rotation();
	SetActorRotation(Rot);
}

// ---------------------------------------------------------------------------

void APoolCue::SetPowerLevel(float Power)
{
	CurrentPower = FMath::Clamp(Power, 0.f, 1.f);

	if (!bStriking)
	{
		// Pull back proportionally
		CueMesh->SetRelativeLocation(-AimDir * MaxPullBack * CurrentPower);
	}
}

// ---------------------------------------------------------------------------

void APoolCue::ShowAimLine(FVector Start, FVector Direction)
{
	bAimLineVisible = true;
	AimLineStart = Start;
	AimLineDir = Direction.GetSafeNormal();
}

// ---------------------------------------------------------------------------

void APoolCue::HideAimLine()
{
	bAimLineVisible = false;
}

// ---------------------------------------------------------------------------

void APoolCue::PlayStrikeAnimation()
{
	bStriking = true;
	StrikeTimer = 0.f;
	HideAimLine();
}

// ---------------------------------------------------------------------------

void APoolCue::DrawAimLine() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float DotSize = 1.5f;
	const FColor LineColor = FColor::White;

	float Distance = 0.f;
	bool bDotOn = true;
	while (Distance < AimLineLength)
	{
		if (bDotOn)
		{
			const FVector P = AimLineStart + AimLineDir * Distance;
			DrawDebugPoint(World, P, DotSize, LineColor, false, -1.f);
		}
		Distance += AimLineDotSpacing * 0.5f;
		bDotOn = !bDotOn;
	}
}
