// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/GooBridge/GooBridgeGameMode.h"
#include "Games/GooBridge/GooBridgeHUD.h"
#include "Games/GooBridge/GooNode.h"
#include "Games/GooBridge/GooStrut.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------

AGooBridgeGameMode::AGooBridgeGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameType = EPHCGameType::GooBridge;
	GameHUDClass = UGooBridgeHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void AGooBridgeGameMode::StartGame()
{
	Super::StartGame();
	LoadLevel(1);
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::LoadLevel(int32 Level)
{
	ClearLevel();

	CurrentLevel = FMath::Max(1, Level);

	// Scale difficulty with level
	GapWidth = 500.f + (CurrentLevel - 1) * 100.f;
	GooAvailable = FMath::Max(10, 25 - CurrentLevel);

	// Position platforms on either side of the gap
	const float HalfGap = GapWidth * 0.5f;
	StartPlatformPos = FVector2D(-HalfGap - 100.f, 0.f);
	EndPlatformPos   = FVector2D( HalfGap + 100.f, 0.f);
	GoalPosition     = EndPlatformPos;

	// Spawn anchor nodes on each platform
	SpawnAnchorNodes(StartPlatformPos, 3);
	SpawnAnchorNodes(EndPlatformPos, 3);

	bSimulating = false;
	SelectedNode = nullptr;
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::PlaceNode(FVector2D Position)
{
	if (bSimulating || GooAvailable <= 0 || !GetWorld())
	{
		return;
	}

	const FVector SpawnLoc(Position.X, Position.Y, PlayZ);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AGooNode* NewNode = GetWorld()->SpawnActor<AGooNode>(
		AGooNode::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);

	if (!NewNode)
	{
		return;
	}

	NewNode->bIsPlaced = true;
	Nodes.Add(NewNode);
	--GooAvailable;

	// Auto-connect to nearby nodes
	TArray<AGooNode*> Nearby = GetNearbyNodes(Position, AutoConnectRadius);
	for (AGooNode* Neighbour : Nearby)
	{
		if (Neighbour != NewNode)
		{
			ConnectNodes(NewNode, Neighbour);
		}
	}
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::ConnectNodes(AGooNode* A, AGooNode* B)
{
	if (!A || !B || A == B || !GetWorld())
	{
		return;
	}

	// Prevent duplicate struts between the same pair
	for (const TObjectPtr<AGooStrut>& Existing : Struts)
	{
		if (!Existing) continue;
		if ((Existing->NodeA == A && Existing->NodeB == B) ||
			(Existing->NodeA == B && Existing->NodeB == A))
		{
			return;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector MidPoint = (A->GetActorLocation() + B->GetActorLocation()) * 0.5f;

	AGooStrut* Strut = GetWorld()->SpawnActor<AGooStrut>(
		AGooStrut::StaticClass(), MidPoint, FRotator::ZeroRotator, SpawnParams);

	if (Strut)
	{
		Strut->Initialize(A, B);
		Struts.Add(Strut);
	}
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::StartSimulation()
{
	if (bSimulating)
	{
		return;
	}

	bSimulating = true;

	// Snapshot strut connections for potential reset
	PreSimStruts.Reset();
	for (const TObjectPtr<AGooStrut>& Strut : Struts)
	{
		if (Strut)
		{
			FStrutSnapshot Snap;
			Snap.A = Strut->NodeA;
			Snap.B = Strut->NodeB;
			PreSimStruts.Add(Snap);
		}
	}

	// Enable physics on all nodes
	for (TObjectPtr<AGooNode>& Node : Nodes)
	{
		if (Node)
		{
			Node->SetSimulating(true);
		}
	}
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::StopSimulation()
{
	if (!bSimulating)
	{
		return;
	}

	bSimulating = false;

	// Disable physics and reset node positions
	for (TObjectPtr<AGooNode>& Node : Nodes)
	{
		if (Node)
		{
			Node->SetSimulating(false);
		}
	}

	// Destroy all current struts
	for (TObjectPtr<AGooStrut>& Strut : Struts)
	{
		if (Strut)
		{
			Strut->Destroy();
		}
	}
	Struts.Reset();

	// Rebuild struts from pre-sim snapshot
	for (const FStrutSnapshot& Snap : PreSimStruts)
	{
		if (Snap.A.IsValid() && Snap.B.IsValid())
		{
			ConnectNodes(Snap.A.Get(), Snap.B.Get());
		}
	}
	PreSimStruts.Reset();
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || !bSimulating)
	{
		return;
	}

	// Clean up destroyed struts from the array
	Struts.RemoveAll([](const TObjectPtr<AGooStrut>& S) { return !IsValid(S); });

	// Check for success
	if (CheckBridgeSuccess())
	{
		StopSimulation();

		// Score bonus for remaining goo
		AddScore(100 + GooAvailable * 10);

		// Advance to next level
		const int32 NextLevel = CurrentLevel + 1;
		if (NextLevel > 10)
		{
			EndGame(true);
		}
		else
		{
			LoadLevel(NextLevel);
		}
		return;
	}

	// Check for collapse
	if (CheckBridgeCollapse())
	{
		StopSimulation();
		// Player can try again - don't end the game
	}
}

// ---------------------------------------------------------------------------

bool AGooBridgeGameMode::CheckBridgeSuccess() const
{
	const FVector GoalCenter(GoalPosition.X, GoalPosition.Y, PlayZ);

	for (const TObjectPtr<AGooNode>& Node : Nodes)
	{
		if (!Node || Node->bIsFixed)
		{
			continue;
		}

		const float DistXY = FVector::DistXY(Node->GetActorLocation(), GoalCenter);
		if (DistXY <= GoalRadius)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------

bool AGooBridgeGameMode::CheckBridgeCollapse() const
{
	int32 FallenCount = 0;
	int32 MovableCount = 0;

	for (const TObjectPtr<AGooNode>& Node : Nodes)
	{
		if (!Node || Node->bIsFixed)
		{
			continue;
		}

		++MovableCount;
		if (Node->GetActorLocation().Z < DeathPlaneZ)
		{
			++FallenCount;
		}
	}

	// Collapse if more than half of movable nodes fell below death plane
	return MovableCount > 0 && FallenCount > (MovableCount / 2);
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::RemoveNode(AGooNode* Node)
{
	if (!Node || bSimulating || Node->bIsFixed)
	{
		return;
	}

	// Destroy all connected struts
	TArray<TObjectPtr<AGooStrut>> StrutsToRemove = Node->ConnectedStruts;
	for (TObjectPtr<AGooStrut>& Strut : StrutsToRemove)
	{
		if (Strut)
		{
			Struts.Remove(Strut);
			Strut->BreakStrut();
		}
	}

	Nodes.Remove(Node);
	Node->Destroy();

	// Refund the goo
	++GooAvailable;
}

// ---------------------------------------------------------------------------

TArray<AGooNode*> AGooBridgeGameMode::GetNearbyNodes(
	FVector2D Position, float Radius) const
{
	TArray<AGooNode*> Result;
	const float RadiusSq = Radius * Radius;
	const FVector WorldPos(Position.X, Position.Y, PlayZ);

	for (const TObjectPtr<AGooNode>& Node : Nodes)
	{
		if (!Node)
		{
			continue;
		}

		const float DistSq = FVector::DistSquaredXY(Node->GetActorLocation(), WorldPos);
		if (DistSq <= RadiusSq)
		{
			Result.Add(Node.Get());
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void AGooBridgeGameMode::SpawnAnchorNodes(FVector2D PlatformCenter, int32 Count)
{
	if (!GetWorld() || Count <= 0)
	{
		return;
	}

	// Spread anchor nodes evenly along the platform edge
	const float Spacing = 60.f;
	const float StartOffset = -((Count - 1) * Spacing) * 0.5f;

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector SpawnLoc(
			PlatformCenter.X,
			PlatformCenter.Y + StartOffset + i * Spacing,
			PlayZ);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AGooNode* Anchor = GetWorld()->SpawnActor<AGooNode>(
			AGooNode::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SpawnParams);

		if (Anchor)
		{
			Anchor->bIsFixed = true;
			Anchor->bIsPlaced = true;
			Nodes.Add(Anchor);
		}
	}
}

// ---------------------------------------------------------------------------

void AGooBridgeGameMode::ClearLevel()
{
	bSimulating = false;

	// Destroy all struts
	for (TObjectPtr<AGooStrut>& Strut : Struts)
	{
		if (Strut)
		{
			Strut->Destroy();
		}
	}
	Struts.Reset();

	// Destroy all nodes
	for (TObjectPtr<AGooNode>& Node : Nodes)
	{
		if (Node)
		{
			Node->Destroy();
		}
	}
	Nodes.Reset();

	PreSimStruts.Reset();
	SelectedNode = nullptr;
}
