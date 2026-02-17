// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/PHCTypes.h"
#include "PHCPlayerState.generated.h"

/**
 * Per-player replicated state for PlayhouseCity.
 *
 * Each connected player gets one of these, holding their profile snapshot,
 * the room they are currently in, and whether they have toggled "ready"
 * for the next game round.  Key properties are replicated so that every
 * client has an authoritative view of every player's state.
 */
UCLASS()
class PLAYHOUSECITY_API APHCPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	APHCPlayerState();

	// -- Profile --------------------------------------------------------------

	/** Snapshot of the player's profile (set on join, updated on currency
	 *  changes, level-ups, etc.).  Replicated to all clients. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Player")
	FPHCPlayerProfile PlayerProfile;

	/** Set the full profile. Typically called on the server when the player joins. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Player")
	void SetPlayerProfile(const FPHCPlayerProfile& InProfile);

	/** Return a const reference to the current profile. */
	UFUNCTION(BlueprintPure, Category = "PHC|Player")
	const FPHCPlayerProfile& GetPlayerProfile() const { return PlayerProfile; }

	// -- Room state -----------------------------------------------------------

	/** ID of the room this player is currently in. 0 = not in any room.
	 *  Replicated to all clients. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Room")
	int32 CurrentRoomId = 0;

	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void SetCurrentRoom(int32 RoomId);

	UFUNCTION(BlueprintPure, Category = "PHC|Room")
	int32 GetCurrentRoomId() const { return CurrentRoomId; }

	UFUNCTION(BlueprintPure, Category = "PHC|Room")
	bool IsInRoom() const { return CurrentRoomId != 0; }

	// -- Ready state ----------------------------------------------------------

	/** Whether the player has toggled "ready" for the next game round.
	 *  Replicated to all clients. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Room")
	bool bIsReady = false;

	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void SetReady(bool bReady);

	UFUNCTION(BlueprintPure, Category = "PHC|Room")
	bool IsReady() const { return bIsReady; }

	// -- Host flag ------------------------------------------------------------

	/** Whether this player is the host of their current room.
	 *  Determined locally; not replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Room")
	bool bIsHost = false;

	// -- Utilities ------------------------------------------------------------

	/** Convenience: reset transient room/game state when leaving a room. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void ResetRoomState();

	// -- Replication ----------------------------------------------------------

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
