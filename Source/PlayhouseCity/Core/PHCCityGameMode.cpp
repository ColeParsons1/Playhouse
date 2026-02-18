// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCCityGameMode.h"
#include "Core/PHCPlayerController.h"
#include "Player/PHCCityCharacter.h"
#include "Player/PHCPlayerState.h"
#include "Core/PHCGameState.h"
#include "Vehicle/PHCVehiclePawn.h"
#include "PlayhouseCity.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"

APHCCityGameMode::APHCCityGameMode()
{
	DefaultPawnClass = APHCCityCharacter::StaticClass();
	PlayerControllerClass = APHCPlayerController::StaticClass();
	PlayerStateClass = APHCPlayerState::StaticClass();
	GameStateClass = APHCGameState::StaticClass();
}

void APHCCityGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Disable ALL on-screen debug/log text (yellow messages).
	GAreScreenMessagesEnabled = false;
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("APHCCityGameMode::BeginPlay -- Multiplayer game mode ready. City generation handled by UPHCCitySubsystem."));
}

void APHCCityGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("APHCCityGameMode::PostLogin -- Player joined: %s"), *NewPlayer->GetName());
	}
}

APawn* APHCCityGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Spawn the character pawn via direct SpawnActor — EXACT same pattern
	// as vehicle spawn in PHCPlayerController::SpawnAndEnterVehicle().
	// This bypasses UE's default FindTeleportSpot which adjusts Z position.
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass) return nullptr;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.ObjectFlags |= RF_Transient;

	// Spawn at desired city location instead of world origin.
	const FVector SpawnLoc(-28000.f, -28000.f, 85.f);
	const FRotator SpawnRot = SpawnTransform.Rotator();

	APawn* ResultPawn = World->SpawnActor<APawn>(PawnClass, SpawnLoc, SpawnRot, SP);

	UE_LOG(LogPlayhouseCity, Log, TEXT("APHCCityGameMode::SpawnDefaultPawnAtTransform -- Spawned pawn at %s (class %s)"),
		ResultPawn ? *ResultPawn->GetActorLocation().ToString() : TEXT("FAILED"),
		*PawnClass->GetName());

	return ResultPawn;
}

void APHCCityGameMode::Logout(AController* Exiting)
{
	if (Exiting)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("APHCCityGameMode::Logout -- Player left: %s"), *Exiting->GetName());

		// Clean up any vehicles owned by the leaving player
		APawn* ExitPawn = Exiting->GetPawn();
		APHCVehiclePawn* Vehicle = Cast<APHCVehiclePawn>(ExitPawn);
		if (Vehicle)
		{
			SpawnedVehicles.Remove(Vehicle);
			Vehicle->Destroy();
		}
	}

	Super::Logout(Exiting);
}
