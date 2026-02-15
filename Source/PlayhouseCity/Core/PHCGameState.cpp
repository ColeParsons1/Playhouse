// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCGameState.h"
#include "Net/UnrealNetwork.h"

APHCGameState::APHCGameState()
{
}

void APHCGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APHCGameState, ServerDayNightTime);
}
