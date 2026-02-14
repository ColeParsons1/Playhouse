// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Jenga/JengaGameMode.h"
#include "Games/Jenga/JengaHUD.h"
#include "Games/Jenga/JengaBlock.h"
#include "Games/Jenga/JengaTower.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"

// ---------------------------------------------------------------------------

AJengaGameMode::AJengaGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameType = EPHCGameType::Jenga;
	GameHUDClass = UJengaHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void AJengaGameMode::StartGame()
{
	Super::StartGame();

	BlocksRemoved = 0;
	bTowerFallen = false;
	bIsPulling = false;
	bWaitingForSettle = false;
	SettleTimer = 0.f;
	SelectedBlock = nullptr;

	BuildTower();
}

// ---------------------------------------------------------------------------

void AJengaGameMode::BuildTower()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Destroy existing tower if present
	if (Tower)
	{
		Tower->Destroy();
		Tower = nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Tower = World->SpawnActor<AJengaTower>(
		AJengaTower::StaticClass(),
		FVector(0.f, 0.f, 0.f),
		FRotator::ZeroRotator,
		SpawnParams);

	if (Tower)
	{
		Tower->TowerBasePosition = FVector(0.f, 0.f, 50.f); // slightly above ground
		Tower->BuildStack();
		Blocks = Tower->AllBlocks;
	}
}

// ---------------------------------------------------------------------------

void AJengaGameMode::SelectBlock(AJengaBlock* Block)
{
	if (!Block || bTowerFallen || bWaitingForSettle || bIsPulling)
	{
		return;
	}

	// Deselect previous
	if (SelectedBlock)
	{
		SelectedBlock->SetHighlighted(false);
	}

	if (!IsBlockRemovable(Block))
	{
		SelectedBlock = nullptr;
		return;
	}

	SelectedBlock = Block;
	SelectedBlock->SetHighlighted(true);
}

// ---------------------------------------------------------------------------

void AJengaGameMode::PullBlock(FVector Direction, float Amount)
{
	if (!SelectedBlock || bTowerFallen || bWaitingForSettle)
	{
		return;
	}

	if (!bIsPulling)
	{
		// Start pulling
		bIsPulling = true;
		PullStartLocation = SelectedBlock->GetActorLocation();
		PullDistance = 0.f;

		// Temporarily make the block kinematic for controlled sliding
		if (SelectedBlock->BlockCollision)
		{
			SelectedBlock->BlockCollision->SetSimulatePhysics(false);
		}
	}

	// Normalise direction and apply movement
	Direction.Normalize();
	PullDistance += Amount;

	const FVector NewLocation = PullStartLocation + Direction * PullDistance;
	SelectedBlock->SetActorLocation(NewLocation);
}

// ---------------------------------------------------------------------------

void AJengaGameMode::ReleaseBlock()
{
	if (!SelectedBlock || !bIsPulling)
	{
		return;
	}

	bIsPulling = false;

	// Re-enable physics
	if (SelectedBlock->BlockCollision)
	{
		SelectedBlock->BlockCollision->SetSimulatePhysics(true);
	}

	if (PullDistance >= FullRemovalDistance)
	{
		// Block fully removed - place it on top
		SelectedBlock->bIsRemoved = true;
		++BlocksRemoved;
		AddScore(BlocksRemoved); // progressive scoring
		PlaceOnTop(SelectedBlock);

		// Wait for tower to settle
		bWaitingForSettle = true;
		SettleTimer = 0.f;
	}
	else
	{
		// Not pulled far enough - slide back
		SelectedBlock->SetActorLocation(PullStartLocation);
	}

	SelectedBlock->SetHighlighted(false);
	SelectedBlock = nullptr;
	PullDistance = 0.f;
}

// ---------------------------------------------------------------------------

void AJengaGameMode::PlaceOnTop(AJengaBlock* Block)
{
	if (!Block || !Tower)
	{
		return;
	}

	// Determine new top position
	const int32 NewLayer = Tower->TopLayer + 1;
	const bool bAlternate = (NewLayer % 2 != 0);

	// Place in the center position of the new top layer
	const FVector TopPos = Tower->GetBlockPosition(NewLayer, 1, bAlternate);
	const FRotator TopRot = Tower->GetBlockRotation(NewLayer);

	Block->SetActorLocation(TopPos);
	Block->SetActorRotation(TopRot);
	Block->LayerIndex = NewLayer;
	Block->PositionInLayer = 1;

	// Re-enable physics so it drops into place
	if (Block->BlockCollision)
	{
		Block->BlockCollision->SetSimulatePhysics(true);
		Block->BlockCollision->SetPhysicsLinearVelocity(FVector::ZeroVector);
	}

	Tower->TopLayer = NewLayer;
}

// ---------------------------------------------------------------------------

void AJengaGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bTowerFallen)
	{
		return;
	}

	// Monitor tower stability
	if (!CheckTowerStability())
	{
		OnTowerCollapse();
		return;
	}

	// Settle-wait logic after a block removal
	if (bWaitingForSettle)
	{
		SettleTimer += DeltaTime;

		if (SettleTimer >= SettleWaitTime && AreBlocksSettled())
		{
			bWaitingForSettle = false;
			SettleTimer = 0.f;

			// Re-check stability after settling
			if (!CheckTowerStability())
			{
				OnTowerCollapse();
			}
		}
		else if (SettleTimer > SettleWaitTime * 3.f)
		{
			// Force-end settle wait after too long
			bWaitingForSettle = false;
			SettleTimer = 0.f;
		}
	}
}

// ---------------------------------------------------------------------------

bool AJengaGameMode::CheckTowerStability() const
{
	if (!Tower)
	{
		return true;
	}

	const float LeanAngle = Tower->GetCurrentLeanAngle();
	return LeanAngle < TowerStabilityThreshold;
}

// ---------------------------------------------------------------------------

bool AJengaGameMode::IsBlockRemovable(AJengaBlock* Block) const
{
	if (!Block || Block->bIsRemoved || !Tower)
	{
		return false;
	}

	// Cannot remove blocks from the top two layers
	const int32 TopIdx = Tower->GetTopLayerIndex();
	if (Block->LayerIndex >= TopIdx - 1)
	{
		return false;
	}

	// Count how many blocks remain in this layer
	int32 LayerCount = 0;
	for (const TObjectPtr<AJengaBlock>& B : Blocks)
	{
		if (B && !B->bIsRemoved && B->LayerIndex == Block->LayerIndex)
		{
			++LayerCount;
		}
	}

	// Must leave at least one block in the layer to maintain structural support
	return LayerCount > 1;
}

// ---------------------------------------------------------------------------

float AJengaGameMode::GetTowerHeight() const
{
	return Tower ? Tower->GetTowerHeight() : 0.f;
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void AJengaGameMode::OnTowerCollapse()
{
	bTowerFallen = true;

	// Final score is based on blocks removed
	CurrentScore = BlocksRemoved * 10;
	EndGame(BlocksRemoved >= 10); // "win" if removed at least 10 blocks
}

// ---------------------------------------------------------------------------

bool AJengaGameMode::AreBlocksSettled() const
{
	for (const TObjectPtr<AJengaBlock>& Block : Blocks)
	{
		if (Block && !Block->bIsRemoved && Block->IsMoving())
		{
			if (Block->GetSpeed() > SettleSpeedThreshold)
			{
				return false;
			}
		}
	}
	return true;
}
