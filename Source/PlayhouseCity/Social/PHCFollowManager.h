// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/PHCTypes.h"
#include "PHCFollowManager.generated.h"

class UPHCHttpClient;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast when a follow or unfollow action completes for a player. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPHCFollowChanged, int32, PlayerId, bool, bNowFollowing);

// ---------------------------------------------------------------------------
// UPHCFollowManager
// ---------------------------------------------------------------------------

/**
 * UPHCFollowManager
 *
 * Manages the follow/unfollow social system for PlayhouseCity. Maintains
 * a local cache of followed player IDs for fast lookups and synchronizes
 * with the Django backend via REST API.
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCFollowManager : public UObject
{
	GENERATED_BODY()

public:
	UPHCFollowManager();

	// ---------------------------------------------------------------
	//  Initialization
	// ---------------------------------------------------------------

	/**
	 * Initialize the follow manager with an HTTP client reference.
	 * Must be called before any other methods.
	 *
	 * @param InHttpClient  The HTTP client used for API communication.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Social")
	void Initialize(UPHCHttpClient* InHttpClient);

	// ---------------------------------------------------------------
	//  Follow / Unfollow
	// ---------------------------------------------------------------

	/**
	 * Follow a player. Sends a POST to the server, updates the local
	 * cache on success, and fires OnFollowChanged.
	 *
	 * @param PlayerId  The ID of the player to follow.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Social")
	void FollowPlayer(int32 PlayerId);

	/**
	 * Unfollow a player. Sends a DELETE to the server, updates the local
	 * cache on success, and fires OnFollowChanged.
	 *
	 * @param PlayerId  The ID of the player to unfollow.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Social")
	void UnfollowPlayer(int32 PlayerId);

	/**
	 * Check whether the local player is currently following a given player.
	 * Uses the local cache for instant results.
	 *
	 * @param PlayerId  The player to check.
	 * @return True if currently following.
	 */
	UFUNCTION(BlueprintPure, Category = "PHC|Social")
	bool IsFollowing(int32 PlayerId) const;

	// ---------------------------------------------------------------
	//  Social Lists
	// ---------------------------------------------------------------

	/**
	 * Fetch the list of players who follow a given player.
	 *
	 * @param PlayerId  The player whose followers to retrieve.
	 * @param Callback  Called with the array of follower info on completion.
	 */
	void GetFollowers(int32 PlayerId, TFunction<void(TArray<FPHCFollowInfo>)> Callback);

	/**
	 * Fetch the list of players that a given player is following.
	 *
	 * @param PlayerId  The player whose following list to retrieve.
	 * @param Callback  Called with the array of following info on completion.
	 */
	void GetFollowing(int32 PlayerId, TFunction<void(TArray<FPHCFollowInfo>)> Callback);

	/**
	 * Refresh the local following cache by fetching the current user's
	 * full following list from the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Social")
	void RefreshFollowingCache();

	// ---------------------------------------------------------------
	//  Delegates
	// ---------------------------------------------------------------

	/** Fired when a follow or unfollow action completes. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Social")
	FOnPHCFollowChanged OnFollowChanged;

private:
	// ---------------------------------------------------------------
	//  Internal Helpers
	// ---------------------------------------------------------------

	/**
	 * Handle the API response from a follow or unfollow request.
	 *
	 * @param bSuccess   Whether the HTTP request succeeded.
	 * @param Response   The raw JSON response body.
	 * @param PlayerId   The player the action was performed on.
	 * @param bFollow    True if this was a follow, false if unfollow.
	 */
	void OnFollowResponse(bool bSuccess, const FString& Response, int32 PlayerId, bool bFollow);

	/**
	 * Parse a JSON array of follow entries into an array of FPHCFollowInfo.
	 *
	 * @param JsonResponse  The raw JSON response containing a "results" array.
	 * @return Parsed array of follow info structs.
	 */
	TArray<FPHCFollowInfo> ParseFollowList(const FString& JsonResponse) const;

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** Locally cached set of player IDs the current user is following. */
	TSet<int32> FollowingCache;

	/** Cached reference to the HTTP client for API calls. */
	UPROPERTY()
	TObjectPtr<UPHCHttpClient> HttpClient;
};
