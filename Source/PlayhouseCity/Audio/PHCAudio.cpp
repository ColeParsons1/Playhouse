// Copyright PlayhouseCity. All Rights Reserved.

#include "Audio/PHCAudio.h"
#include "PlayhouseCity.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace
{
	TMap<FString, TWeakObjectPtr<USoundBase>>& GetSoundCache()
	{
		static TMap<FString, TWeakObjectPtr<USoundBase>> Cache;
		return Cache;
	}
}

USoundBase* PHCAudio::LoadSound(const FString& Path)
{
	if (Path.IsEmpty()) return nullptr;

	auto& Cache = GetSoundCache();
	if (TWeakObjectPtr<USoundBase>* Found = Cache.Find(Path))
	{
		if (Found->IsValid()) return Found->Get();
	}

	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);
	if (!Sound)
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("PHCAudio: Sound asset not found at '%s'"), *Path);
		return nullptr;
	}
	Sound->AddToRoot();
	Cache.Add(Path, Sound);
	return Sound;
}

UAudioComponent* PHCAudio::PlayAmbientLoop2D(UWorld* World, const FString& Path, float VolumeMultiplier)
{
	if (!World) return nullptr;
	USoundBase* Sound = LoadSound(Path);
	if (!Sound) return nullptr;

	// Force USoundWave assets to loop. USoundCues are expected to handle
	// looping in their own graph, so leave them alone.
	if (USoundWave* Wave = Cast<USoundWave>(Sound))
	{
		Wave->bLooping = true;
	}

	UAudioComponent* AC = UGameplayStatics::SpawnSound2D(World, Sound, VolumeMultiplier);
	if (AC)
	{
		AC->bAutoDestroy = false;
	}
	return AC;
}

void PHCAudio::PlayOneShot2D(UWorld* World, const FString& Path, float VolumeMultiplier)
{
	if (!World) return;
	USoundBase* Sound = LoadSound(Path);
	if (!Sound) return;
	UGameplayStatics::PlaySound2D(World, Sound, VolumeMultiplier);
}

void PHCAudio::PlayOneShotAtLocation(UWorld* World, const FString& Path, const FVector& Location, float VolumeMultiplier)
{
	if (!World) return;
	USoundBase* Sound = LoadSound(Path);
	if (!Sound) return;
	UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, VolumeMultiplier);
}

void PHCAudio::PlayRandomOneShotAtLocation(UWorld* World, const TArray<FString>& Paths, const FVector& Location, float VolumeMultiplier)
{
	if (!World || Paths.Num() == 0) return;
	const int32 Index = FMath::RandRange(0, Paths.Num() - 1);
	PlayOneShotAtLocation(World, Paths[Index], Location, VolumeMultiplier);
}
