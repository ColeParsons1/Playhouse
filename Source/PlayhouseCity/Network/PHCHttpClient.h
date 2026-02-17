// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Core/PHCTypes.h"
#include "PHCHttpClient.generated.h"

class UPHCApiCache;

// -------------------------------------------------------------------
//  Delegate declarations
// -------------------------------------------------------------------

/** Generic native (C++) callback: (bSuccess, ResponseBody). */
DECLARE_DELEGATE_TwoParams(FPHCHttpResponse, bool /*bSuccess*/, const FString& /*ResponseBody*/);

/** Blueprint-friendly generic response delegate. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FPHCHttpResponseDynamic, bool, bSuccess, const FString&, ResponseBody);

/** Blueprint-friendly delegate carrying a parsed JSON object and status. */
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FPHCJsonResponseDynamic, bool, bSuccess, int32, StatusCode, const FString&, JsonBody);

/** Auth-specific response: carries the token on success. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FPHCAuthResponseDynamic, bool, bSuccess, const FString&, Token);

/** Simple success/fail with message. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FPHCSimpleResponseDynamic, bool, bSuccess, const FString&, Message);

// -------------------------------------------------------------------
//  Lightweight data structs used by HTTP endpoints
//  NOTE: FPHCAvatarData and FPHCAvatarLayer are now defined in
//  Core/PHCTypes.h (the canonical types header).
// -------------------------------------------------------------------

USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCRoomCreateParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayhouseCity|Rooms")
	FString RoomName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayhouseCity|Rooms")
	FString Theme;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayhouseCity|Rooms")
	int32 MaxPlayers = 20;

	FString ToJsonString() const;
};

// -------------------------------------------------------------------
//  UPHCHttpClient
// -------------------------------------------------------------------

/**
 * UPHCHttpClient
 *
 * Wraps FHttpModule to provide a clean, delegate-driven REST client for
 * the PlayhouseCity Django backend. Every request automatically attaches
 * the auth token (when available) and routes through an optional
 * UPHCApiCache layer for GET requests.
 *
 * Intended to be owned by the Game Instance or a subsystem.
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCHttpClient : public UObject
{
	GENERATED_BODY()

public:
	UPHCHttpClient();

	// ---------------------------------------------------------------
	//  Initialization
	// ---------------------------------------------------------------

	/** Set the base URL for all API calls (e.g. "https://api.playhousecity.com"). */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP")
	void Initialize(const FString& BaseUrl);

	/** Manually set or clear the auth token. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP")
	void SetAuthToken(const FString& Token);

	/** Returns the current auth token (empty if not authenticated). */
	UFUNCTION(BlueprintPure, Category = "PlayhouseCity|HTTP")
	FString GetAuthToken() const { return AuthToken; }

	/** Returns true if an auth token is currently stored. */
	UFUNCTION(BlueprintPure, Category = "PlayhouseCity|HTTP")
	bool IsAuthenticated() const { return !AuthToken.IsEmpty(); }

	/** Assign an external cache object for GET response caching. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP")
	void SetApiCache(UPHCApiCache* InCache);

	// ---------------------------------------------------------------
	//  Generic REST verbs (C++ delegates)
	// ---------------------------------------------------------------

	void Get(const FString& Endpoint, const FPHCHttpResponse& Callback, float CacheTTL = 0.f);
	void Post(const FString& Endpoint, const FString& JsonBody, const FPHCHttpResponse& Callback);
	void Put(const FString& Endpoint, const FString& JsonBody, const FPHCHttpResponse& Callback);
	void Delete(const FString& Endpoint, const FPHCHttpResponse& Callback);

	// ---------------------------------------------------------------
	//  Generic REST verbs (Blueprint dynamic delegates)
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP", meta = (DisplayName = "GET"))
	void K2_Get(const FString& Endpoint, const FPHCJsonResponseDynamic& Callback, float CacheTTL = 0.f);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP", meta = (DisplayName = "POST"))
	void K2_Post(const FString& Endpoint, const FString& JsonBody, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP", meta = (DisplayName = "PUT"))
	void K2_Put(const FString& Endpoint, const FString& JsonBody, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP", meta = (DisplayName = "DELETE"))
	void K2_Delete(const FString& Endpoint, const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Auth endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Auth")
	void Login(const FString& Email, const FString& Password, const FPHCAuthResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Auth")
	void Register(const FString& Username, const FString& Email, const FString& Password, const FPHCAuthResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Auth")
	void GuestLogin(const FPHCAuthResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Auth")
	void Logout(const FPHCSimpleResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Profile endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Profile")
	void GetProfile(int32 PlayerId, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Profile")
	void UpdateProfile(const FString& ProfileDataJson, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Profile")
	void GetMyProfile(const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Avatar endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Avatar")
	void GetAvatar(int32 PlayerId, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Avatar")
	void SaveAvatar(const FString& AvatarJsonBody, const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Shop endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Shop")
	void GetShopItems(const FString& Category, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Shop")
	void PurchaseItem(int32 ItemId, const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Room endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Rooms")
	void GetRoomList(const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Rooms")
	void CreateRoom(const FString& Name, const FString& Theme, int32 MaxPlayers, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Rooms")
	void JoinRoom(int32 RoomId, const FString& Password, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Rooms")
	void LeaveRoom(int32 RoomId, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Rooms")
	void GetRoomDetails(int32 RoomId, const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Economy endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Economy")
	void GetBalance(const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Economy")
	void ClaimDailyBonus(const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Economy")
	void GetDailyBonusInfo(const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Economy")
	void ScratchCard(const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Garden endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Garden")
	void GetGarden(const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Garden")
	void PlantSeed(int32 PlotIndex, const FString& PlantType, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Garden")
	void WaterPlant(int32 PlotIndex, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Garden")
	void HarvestPlant(int32 PlotIndex, const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Leaderboard endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Leaderboard")
	void GetLeaderboard(const FString& GameType, int32 Page, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Leaderboard")
	void SubmitScore(const FString& GameType, int32 Score, const FPHCJsonResponseDynamic& Callback);

	// ---------------------------------------------------------------
	//  Social endpoints
	// ---------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Social")
	void FollowPlayer(int32 PlayerId, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Social")
	void UnfollowPlayer(int32 PlayerId, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Social")
	void GetFollowers(int32 PlayerId, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Social")
	void GetFollowing(int32 PlayerId, const FPHCJsonResponseDynamic& Callback);

	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|HTTP|Social")
	void SearchPlayers(const FString& Query, const FPHCJsonResponseDynamic& Callback);

private:
	// ---------------------------------------------------------------
	//  Internal helpers
	// ---------------------------------------------------------------

	/** Build a full URL from a relative endpoint. */
	FString MakeUrl(const FString& Endpoint) const;

	/** Create and configure an FHttpRequest with verb, URL, headers. */
	TSharedRef<IHttpRequest> CreateRequest(const FString& Verb, const FString& Endpoint, const FString& JsonBody = FString());

	/** Fire the request and bind completion to a native callback. */
	void ProcessRequest(TSharedRef<IHttpRequest> Request, const FPHCHttpResponse& Callback);

	/** Fire the request and bind completion to a dynamic (Blueprint) callback. */
	void ProcessRequestDynamic(TSharedRef<IHttpRequest> Request, const FPHCJsonResponseDynamic& Callback);

	/** Build a JSON body string from a key-value map. */
	static FString BuildJsonBody(const TMap<FString, FString>& Fields);

	/** Build a JSON body string from a key-value map supporting mixed types via FJsonValue. */
	static FString BuildJsonBodyFromObject(const TSharedRef<FJsonObject>& JsonObject);

	/** Parse a "token" field from a JSON response body. Returns empty string on failure. */
	static FString ExtractTokenFromResponse(const FString& ResponseBody);

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** Base URL for all API endpoints (no trailing slash). */
	UPROPERTY()
	FString ServerBaseUrl;

	/** Authentication token sent as "Authorization: Token <value>". */
	UPROPERTY()
	FString AuthToken;

	/** Optional response cache for GET requests. */
	UPROPERTY()
	TObjectPtr<UPHCApiCache> ApiCache;
};
