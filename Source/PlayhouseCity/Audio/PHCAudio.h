// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UAudioComponent;
class UWorld;

/**
 * Lightweight code-side audio helpers. Loads USoundBase assets by package path,
 * caches them, and plays via UGameplayStatics. Gracefully no-ops (with a log
 * warning) if an asset is missing — keeps gameplay code safe to call even when
 * a sound file hasn't been authored yet.
 */
namespace PHCAudio
{
	USoundBase* LoadSound(const FString& Path);
	UAudioComponent* PlayAmbientLoop2D(UWorld* World, const FString& Path, float VolumeMultiplier = 1.f);
	void PlayOneShot2D(UWorld* World, const FString& Path, float VolumeMultiplier = 1.f);
	void PlayOneShotAtLocation(UWorld* World, const FString& Path, const FVector& Location, float VolumeMultiplier = 1.f);
	void PlayRandomOneShotAtLocation(UWorld* World, const TArray<FString>& Paths, const FVector& Location, float VolumeMultiplier = 1.f);
}
