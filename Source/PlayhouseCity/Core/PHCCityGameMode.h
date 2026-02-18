// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PHCCityGameMode.generated.h"

class APHCVehiclePawn;

/**
 * Multiplayer game mode for PlayhouseCity.
 *
 * Handles player login/logout, vehicle tracking, and session management.
 * City generation and day/night cycle are handled by UPHCCitySubsystem
 * (runs on all machines independently).
 */
UCLASS()
class PLAYHOUSECITY_API APHCCityGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APHCCityGameMode();

	/** All spawned vehicles in the city (for interaction queries). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|City")
	TArray<TObjectPtr<APHCVehiclePawn>> SpawnedVehicles;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Spawn the player pawn via direct SpawnActor (same as vehicle spawn).
	 *  Bypasses UE's default FindTeleportSpot which can adjust the Z position. */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

protected:
	virtual void BeginPlay() override;
};
