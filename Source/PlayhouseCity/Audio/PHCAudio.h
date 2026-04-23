// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UAudioComponent;
class USceneComponent;
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

	/**
	 * Spawn a looping, spatialized AudioComponent attached to the supplied scene
	 * component. Used for vehicle engines, generators, etc. that should follow
	 * a moving source. Returns the AudioComponent so the caller can modulate
	 * pitch/volume per tick (e.g. by speed). Returns nullptr if the asset is
	 * missing or AttachTo is null — safe to call early.
	 */
	UAudioComponent* SpawnAttached3DLoop(USceneComponent* AttachTo, const FString& Path,
		float VolumeMultiplier = 1.f, float PitchMultiplier = 1.f);
}
