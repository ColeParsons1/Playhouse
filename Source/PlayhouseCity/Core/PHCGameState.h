// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PHCGameState.generated.h"

/**
 * Custom GameState for PlayhouseCity multiplayer.
 *
 * Replicates the server's day/night time to all clients so the city
 * subsystem can synchronize lighting across machines.
 */
UCLASS()
class PLAYHOUSECITY_API APHCGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	APHCGameState();

	/** Normalized day/night time replicated from the server (0..1).
	 *  The city subsystem reads this on remote clients instead of computing locally. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|World")
	float ServerDayNightTime = 0.f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
