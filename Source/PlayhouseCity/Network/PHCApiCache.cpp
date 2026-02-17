// Copyright PlayhouseCity. All Rights Reserved.

#include "Network/PHCApiCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCApiCache, Log, All);

// -------------------------------------------------------------------
UPHCApiCache::UPHCApiCache()
{
}

// -------------------------------------------------------------------
bool UPHCApiCache::Get(const FString& Key, FString& OutData) const
{
	const FPHCCachedResponse* Entry = Cache.Find(Key);
	if (!Entry)
	{
		return false;
	}

	if (Entry->IsExpired())
	{
		UE_LOG(LogPHCApiCache, Verbose, TEXT("Cache MISS (expired) for key: %s"), *Key);
		return false;
	}

	OutData = Entry->Data;
	UE_LOG(LogPHCApiCache, Verbose, TEXT("Cache HIT for key: %s"), *Key);
	return true;
}

// -------------------------------------------------------------------
void UPHCApiCache::Set(const FString& Key, const FString& Data, float TTLSeconds)
{
	Cache.Add(Key, FPHCCachedResponse(Data, TTLSeconds));
	UE_LOG(LogPHCApiCache, Verbose, TEXT("Cache SET key: %s  TTL: %.1fs  Size: %d bytes"),
		*Key, TTLSeconds, Data.Len());
}

// -------------------------------------------------------------------
void UPHCApiCache::Invalidate(const FString& Key)
{
	const int32 Removed = Cache.Remove(Key);
	if (Removed > 0)
	{
		UE_LOG(LogPHCApiCache, Verbose, TEXT("Cache INVALIDATE key: %s"), *Key);
	}
}

// -------------------------------------------------------------------
void UPHCApiCache::InvalidateAll()
{
	const int32 Count = Cache.Num();
	Cache.Empty();
	UE_LOG(LogPHCApiCache, Log, TEXT("Cache INVALIDATE ALL  (%d entries removed)"), Count);
}

// -------------------------------------------------------------------
void UPHCApiCache::CleanupExpired()
{
	int32 RemovedCount = 0;

	// Collect keys to remove (cannot modify map during iteration).
	TArray<FString> KeysToRemove;
	for (const auto& Pair : Cache)
	{
		if (Pair.Value.IsExpired())
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const FString& Key : KeysToRemove)
	{
		Cache.Remove(Key);
		++RemovedCount;
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogPHCApiCache, Verbose, TEXT("Cache CLEANUP removed %d expired entries  (%d remaining)"),
			RemovedCount, Cache.Num());
	}
}

// -------------------------------------------------------------------
int32 UPHCApiCache::GetCacheSize() const
{
	return Cache.Num();
}
