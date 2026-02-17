// Copyright PlayhouseCity. All Rights Reserved.

#include "Player/PHCPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "PlayhouseCity.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCPlayerState::APHCPlayerState()
{
	// APlayerState already replicates by default; no extra flags needed.
}

// ---------------------------------------------------------------------------
// Profile
// ---------------------------------------------------------------------------

void APHCPlayerState::SetPlayerProfile(const FPHCPlayerProfile& InProfile)
{
	PlayerProfile = InProfile;

	// Update the engine-level player name so scoreboards / debug HUDs work.
	SetPlayerName(InProfile.DisplayName.IsEmpty() ? InProfile.Username : InProfile.DisplayName);

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCPlayerState::SetPlayerProfile -- Profile set for '%s' (ID %d)."),
		*PlayerProfile.Username, PlayerProfile.PlayerId);
}

// ---------------------------------------------------------------------------
// Room state
// ---------------------------------------------------------------------------

void APHCPlayerState::SetCurrentRoom(int32 RoomId)
{
	CurrentRoomId = RoomId;

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCPlayerState::SetCurrentRoom -- Player '%s' room set to %d."),
		*PlayerProfile.Username, CurrentRoomId);
}

// ---------------------------------------------------------------------------
// Ready state
// ---------------------------------------------------------------------------

void APHCPlayerState::SetReady(bool bReady)
{
	bIsReady = bReady;

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCPlayerState::SetReady -- Player '%s' ready = %s."),
		*PlayerProfile.Username, bIsReady ? TEXT("true") : TEXT("false"));
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------

void APHCPlayerState::ResetRoomState()
{
	CurrentRoomId = 0;
	bIsReady      = false;
	bIsHost       = false;

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCPlayerState::ResetRoomState -- Room state cleared for '%s'."),
		*PlayerProfile.Username);
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void APHCPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APHCPlayerState, PlayerProfile);
	DOREPLIFETIME(APHCPlayerState, CurrentRoomId);
	DOREPLIFETIME(APHCPlayerState, bIsReady);
}
