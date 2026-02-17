// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PHCApiCache.generated.h"

/**
 * Represents a single cached API response with TTL metadata.
 */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCCachedResponse
{
	GENERATED_BODY()

	/** The raw response body string (typically JSON). */
	UPROPERTY(BlueprintReadOnly, Category = "PlayhouseCity|Cache")
	FString Data;

	/** The UTC timestamp when this entry was cached. */
	UPROPERTY(BlueprintReadOnly, Category = "PlayhouseCity|Cache")
	FDateTime CachedAt;

	/** How many seconds this entry remains valid after CachedAt. */
	UPROPERTY(BlueprintReadOnly, Category = "PlayhouseCity|Cache")
	float TTLSeconds = 60.f;

	FPHCCachedResponse() = default;

	FPHCCachedResponse(const FString& InData, float InTTL)
		: Data(InData)
		, CachedAt(FDateTime::UtcNow())
		, TTLSeconds(InTTL)
	{
	}

	/** Returns true if this entry has exceeded its TTL. */
	bool IsExpired() const
	{
		const FTimespan Age = FDateTime::UtcNow() - CachedAt;
		return Age.GetTotalSeconds() > static_cast<double>(TTLSeconds);
	}
};

/**
 * UPHCApiCache
 *
 * Provides a lightweight, TTL-based in-memory cache for REST API responses.
 * Keyed by arbitrary string (typically the full request URL or a logical key).
 * Thread-safe for single-game-thread usage (standard UObject lifecycle).
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCApiCache : public UObject
{
	GENERATED_BODY()

public:
	UPHCApiCache();

	// ---------------------------------------------------------------
	//  Public API
	// ---------------------------------------------------------------

	/**
	 * Attempt to retrieve a cached value.
	 * @param Key       Cache key (e.g. full URL or logical name).
	 * @param OutData   Populated with the cached data if found and not expired.
	 * @return true if a valid (non-expired) entry was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|Cache")
	bool Get(const FString& Key, FString& OutData) const;

	/**
	 * Store a value in the cache.
	 * @param Key          Cache key.
	 * @param Data         Response body to cache.
	 * @param TTLSeconds   Time-to-live in seconds (default 60).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|Cache")
	void Set(const FString& Key, const FString& Data, float TTLSeconds = 60.f);

	/**
	 * Remove a single entry from the cache.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|Cache")
	void Invalidate(const FString& Key);

	/**
	 * Remove all entries from the cache.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|Cache")
	void InvalidateAll();

	/**
	 * Walk the cache and remove every entry whose TTL has expired.
	 * Call this periodically (e.g. on a timer) to keep memory usage bounded.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|Cache")
	void CleanupExpired();

	/**
	 * Returns the current number of entries (including expired ones that
	 * have not yet been cleaned up).
	 */
	UFUNCTION(BlueprintPure, Category = "PlayhouseCity|Cache")
	int32 GetCacheSize() const;

private:
	/** Internal map of key -> cached response. */
	UPROPERTY()
	TMap<FString, FPHCCachedResponse> Cache;
};
