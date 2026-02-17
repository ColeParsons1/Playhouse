// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Jenga/JengaTower.h"
#include "Games/Jenga/JengaBlock.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------

AJengaTower::AJengaTower()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

// ---------------------------------------------------------------------------

void AJengaTower::BuildStack()
{
	ClearBlocks();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TopLayer = TotalLayers - 1;

	for (int32 Layer = 0; Layer < TotalLayers; ++Layer)
	{
		const bool bAlternate = (Layer % 2 != 0);
		const FRotator BlockRotation = GetBlockRotation(Layer);

		for (int32 Pos = 0; Pos < BlocksPerLayer; ++Pos)
		{
			const FVector BlockPos = GetBlockPosition(Layer, Pos, bAlternate);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			AJengaBlock* Block = World->SpawnActor<AJengaBlock>(
				AJengaBlock::StaticClass(), BlockPos, BlockRotation, SpawnParams);

			if (Block)
			{
				Block->LayerIndex = Layer;
				Block->PositionInLayer = Pos;

				// Slight wood tone variation: lighter as you go up
				const float LightFactor = static_cast<float>(Layer) / static_cast<float>(TotalLayers);
				Block->BlockColor = FLinearColor(
					0.70f + LightFactor * 0.08f,
					0.55f + LightFactor * 0.06f,
					0.36f + LightFactor * 0.04f,
					1.f);

				AllBlocks.Add(Block);
			}
		}
	}
}

// ---------------------------------------------------------------------------

FVector AJengaTower::GetBlockPosition(
	int32 Layer, int32 Position, bool bAlternateOrientation) const
{
	// Block dimensions from AJengaBlock constants
	const float Length = AJengaBlock::BlockLength;
	const float Width  = AJengaBlock::BlockWidth;
	const float Height = AJengaBlock::BlockHeight;

	// Stack origin: center of base
	const float BaseZ = TowerBasePosition.Z + Height * 0.5f;

	// Vertical offset for this layer
	const float Z = BaseZ + Layer * Height;

	// Horizontal positioning: 3 blocks side-by-side across the width
	// Position 0 = left, 1 = center, 2 = right
	const float Offset = (Position - 1) * Width; // -Width, 0, +Width

	FVector Pos;
	if (bAlternateOrientation)
	{
		// Alternate layers: blocks run along Y axis
		Pos = FVector(
			TowerBasePosition.X + Offset,
			TowerBasePosition.Y,
			Z);
	}
	else
	{
		// Normal layers: blocks run along X axis
		Pos = FVector(
			TowerBasePosition.X,
			TowerBasePosition.Y + Offset,
			Z);
	}

	return Pos;
}

// ---------------------------------------------------------------------------

FRotator AJengaTower::GetBlockRotation(int32 Layer) const
{
	// Alternate layers are rotated 90 degrees around Z
	if (Layer % 2 != 0)
	{
		return FRotator(0.f, 90.f, 0.f);
	}
	return FRotator::ZeroRotator;
}

// ---------------------------------------------------------------------------

float AJengaTower::GetCurrentLeanAngle() const
{
	if (AllBlocks.Num() == 0)
	{
		return 0.f;
	}

	// Calculate center of mass of all non-removed blocks
	FVector CenterOfMass = FVector::ZeroVector;
	FVector BaseCenter = FVector::ZeroVector;
	int32 ActiveCount = 0;
	int32 BaseCount = 0;

	for (const TObjectPtr<AJengaBlock>& Block : AllBlocks)
	{
		if (!Block || Block->bIsRemoved)
		{
			continue;
		}

		CenterOfMass += Block->GetActorLocation();
		++ActiveCount;

		// Track base layer blocks for reference
		if (Block->LayerIndex <= 1)
		{
			BaseCenter += Block->GetActorLocation();
			++BaseCount;
		}
	}

	if (ActiveCount == 0 || BaseCount == 0)
	{
		return 0.f;
	}

	CenterOfMass /= static_cast<float>(ActiveCount);
	BaseCenter /= static_cast<float>(BaseCount);

	// Calculate lean angle: horizontal deviation from base center
	const FVector Deviation = CenterOfMass - BaseCenter;
	const float HorizontalOffset = FVector2D(Deviation.X, Deviation.Y).Size();
	const float VerticalSpan = FMath::Max(CenterOfMass.Z - BaseCenter.Z, 1.f);

	// Lean angle in degrees
	return FMath::RadiansToDegrees(FMath::Atan2(HorizontalOffset, VerticalSpan));
}

// ---------------------------------------------------------------------------

int32 AJengaTower::GetTopLayerIndex() const
{
	int32 HighestLayer = 0;

	for (const TObjectPtr<AJengaBlock>& Block : AllBlocks)
	{
		if (Block && !Block->bIsRemoved)
		{
			HighestLayer = FMath::Max(HighestLayer, Block->LayerIndex);
		}
	}

	return HighestLayer;
}

// ---------------------------------------------------------------------------

float AJengaTower::GetTowerHeight() const
{
	float MaxZ = TowerBasePosition.Z;

	for (const TObjectPtr<AJengaBlock>& Block : AllBlocks)
	{
		if (Block && !Block->bIsRemoved)
		{
			const float BlockTop =
				Block->GetActorLocation().Z + AJengaBlock::BlockHeight * 0.5f;
			MaxZ = FMath::Max(MaxZ, BlockTop);
		}
	}

	return MaxZ - TowerBasePosition.Z;
}

// ---------------------------------------------------------------------------

void AJengaTower::ClearBlocks()
{
	for (TObjectPtr<AJengaBlock>& Block : AllBlocks)
	{
		if (Block)
		{
			Block->Destroy();
		}
	}
	AllBlocks.Reset();
}
