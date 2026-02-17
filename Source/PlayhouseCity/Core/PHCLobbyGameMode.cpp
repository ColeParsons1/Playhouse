// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCLobbyGameMode.h"
#include "Core/PHCPlayerController.h"
#include "PlayhouseCity.h"

APHCLobbyGameMode::APHCLobbyGameMode()
{
	// No physical pawn in the lobby -- everything is UI.
	DefaultPawnClass = nullptr;

	// Use the PlayhouseCity player controller that manages top-level widgets.
	PlayerControllerClass = APHCPlayerController::StaticClass();
}

void APHCLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCLobbyGameMode::BeginPlay -- Lobby started."));
}
