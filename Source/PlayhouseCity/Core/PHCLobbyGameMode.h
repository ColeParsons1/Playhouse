// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PHCLobbyGameMode.generated.h"

class APHCPlayerController;

/**
 * Game mode used for the lobby / main-menu level.
 *
 * Sets the default pawn to nullptr (no physical pawn -- this is a UI-driven
 * lobby) and the player-controller class to APHCPlayerController, which owns
 * all top-level widget management.
 */
UCLASS()
class PLAYHOUSECITY_API APHCLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APHCLobbyGameMode();

protected:
	virtual void BeginPlay() override;
};
