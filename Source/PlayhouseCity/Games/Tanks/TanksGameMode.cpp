// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Tanks/TanksGameMode.h"
#include "Games/Tanks/TanksHUD.h"
#include "Games/Tanks/TankPawn.h"
#include "Games/Tanks/DestructibleWall.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

ATanksGameMode::ATanksGameMode()
{
	GameType = EPHCGameType::Tanks;
	GameHUDClass = UTanksHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATanksGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnArena();
	StartGame();
}

void ATanksGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ATanksGameMode::StartGame()
{
	Super::StartGame();
	SpawnTanks();
	SpawnWalls();
	CPUTanksRemaining = CPUTanks.Num();
}

// -- Player input proxies --

void ATanksGameMode::MovePlayerForward(float Value)
{
	if (PlayerTank && bGameActive && !bGamePaused)
	{
		PlayerTank->MoveForward(Value);
	}
}

void ATanksGameMode::TurnPlayer(float Value)
{
	if (PlayerTank && bGameActive && !bGamePaused)
	{
		PlayerTank->Turn(Value);
	}
}

void ATanksGameMode::AimPlayerTurret(FVector2D WorldTarget)
{
	if (PlayerTank && bGameActive && !bGamePaused)
	{
		PlayerTank->AimTurret(WorldTarget);
	}
}

void ATanksGameMode::PlayerFire()
{
	if (PlayerTank && bGameActive && !bGamePaused)
	{
		PlayerTank->Fire();
	}
}

// -- Arena setup --

void ATanksGameMode::SpawnArena()
{
	// Arena boundary walls are spawned via SpawnWalls as part of StartGame.
}

void ATanksGameMode::SpawnTanks()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Destroy any existing tanks from a previous round.
	if (PlayerTank)
	{
		PlayerTank->Destroy();
		PlayerTank = nullptr;
	}
	for (ATankPawn* CPU : CPUTanks)
	{
		if (CPU)
		{
			CPU->Destroy();
		}
	}
	CPUTanks.Empty();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Spawn player tank in bottom-left quadrant.
	const FVector PlayerSpawn(ArenaWidth * 0.2f, ArenaHeight * 0.2f, 0.f);
	PlayerTank = World->SpawnActor<ATankPawn>(ATankPawn::StaticClass(), PlayerSpawn, FRotator::ZeroRotator, SP);
	if (PlayerTank)
	{
		PlayerTank->bIsCPU = false;
		PlayerTank->HP = PlayerTank->MaxHP;
		PlayerTank->OnTankDestroyed.AddDynamic(this, &ATanksGameMode::OnPlayerTankDestroyed);
	}

	// Spawn CPU tanks in top-right area.
	for (int32 i = 0; i < NumCPUTanks; ++i)
	{
		const float SpawnX = ArenaWidth * 0.7f + FMath::FRandRange(-50.f, 50.f);
		const float SpawnY = ArenaHeight * 0.7f + FMath::FRandRange(-50.f, 50.f);
		const FVector CPUSpawn(SpawnX, SpawnY, 0.f);

		ATankPawn* CPUTank = World->SpawnActor<ATankPawn>(ATankPawn::StaticClass(), CPUSpawn, FRotator(0.f, 180.f, 0.f), SP);
		if (CPUTank)
		{
			CPUTank->bIsCPU = true;
			CPUTank->HP = CPUTank->MaxHP;
			CPUTank->SetCPUTarget(PlayerTank);
			CPUTank->OnTankDestroyed.AddDynamic(this, &ATanksGameMode::OnCPUTankDestroyed);

			// Tint CPU tanks blue
			if (CPUTank->TankMesh && CPUTank->TankMesh->GetNumMaterials() > 0)
			{
				UMaterialInstanceDynamic* DynMat = CPUTank->TankMesh->CreateDynamicMaterialInstance(0);
				if (DynMat)
				{
					DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.3f, 0.8f, 1.f));
				}
			}

			CPUTanks.Add(CPUTank);
		}
	}
}

void ATanksGameMode::SpawnWalls()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Destroy existing walls.
	for (ADestructibleWall* Wall : Walls)
	{
		if (Wall)
		{
			Wall->Destroy();
		}
	}
	Walls.Empty();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Spawn random interior walls.
	for (int32 i = 0; i < WallCount; ++i)
	{
		const float X = FMath::FRandRange(ArenaWidth * 0.15f, ArenaWidth * 0.85f);
		const float Y = FMath::FRandRange(ArenaHeight * 0.15f, ArenaHeight * 0.85f);
		const FVector WallPos(X, Y, 0.f);

		ADestructibleWall* Wall = World->SpawnActor<ADestructibleWall>(
			ADestructibleWall::StaticClass(), WallPos, FRotator::ZeroRotator, SP);
		if (Wall)
		{
			Wall->HP = FMath::RandRange(1, 3);
			Wall->MaxHP = Wall->HP;
			Walls.Add(Wall);
		}
	}
}

void ATanksGameMode::OnPlayerTankDestroyed(ATankPawn* /*Tank*/)
{
	EndGame(/*bWon=*/ false);
}

void ATanksGameMode::OnCPUTankDestroyed(ATankPawn* /*Tank*/)
{
	--CPUTanksRemaining;
	AddScore(500);

	if (CPUTanksRemaining <= 0)
	{
		EndGame(/*bWon=*/ true);
	}
}
