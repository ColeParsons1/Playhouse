// Copyright PlayhouseCity. All Rights Reserved.

#include "Social/PHCFollowManager.h"
#include "Network/PHCHttpClient.h"
#include "PlayhouseCity.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCFollowManager::UPHCFollowManager()
	: HttpClient(nullptr)
{
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void UPHCFollowManager::Initialize(UPHCHttpClient* InHttpClient)
{
	HttpClient = InHttpClient;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCFollowManager::Initialize -- Follow manager initialized."));
}

// ---------------------------------------------------------------------------
// Follow / Unfollow
// ---------------------------------------------------------------------------

void UPHCFollowManager::FollowPlayer(int32 PlayerId)
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCFollowManager::FollowPlayer -- HttpClient is null."));
		return;
	}

	if (IsFollowing(PlayerId))
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCFollowManager::FollowPlayer -- Already following player %d."), PlayerId);
		return;
	}

	const FString Endpoint = FString::Printf(TEXT("/api/social/follow/%d/"), PlayerId);

	// Build an empty body -- the player ID is in the URL
	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	BodyObj->SetNumberField(TEXT("player_id"), PlayerId);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(BodyObj, Writer);

	const int32 CapturedPlayerId = PlayerId;

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this, CapturedPlayerId](bool bSuccess, const FString& ResponseBody)
	{
		OnFollowResponse(bSuccess, ResponseBody, CapturedPlayerId, /*bFollow=*/ true);
	});

	HttpClient->Post(Endpoint, BodyString, ResponseDelegate);

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCFollowManager::FollowPlayer -- Sending follow request for player %d."), PlayerId);
}

void UPHCFollowManager::UnfollowPlayer(int32 PlayerId)
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCFollowManager::UnfollowPlayer -- HttpClient is null."));
		return;
	}

	if (!IsFollowing(PlayerId))
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCFollowManager::UnfollowPlayer -- Not following player %d."), PlayerId);
		return;
	}

	const FString Endpoint = FString::Printf(TEXT("/api/social/follow/%d/"), PlayerId);

	const int32 CapturedPlayerId = PlayerId;

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this, CapturedPlayerId](bool bSuccess, const FString& ResponseBody)
	{
		OnFollowResponse(bSuccess, ResponseBody, CapturedPlayerId, /*bFollow=*/ false);
	});

	HttpClient->Delete(Endpoint, ResponseDelegate);

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCFollowManager::UnfollowPlayer -- Sending unfollow request for player %d."), PlayerId);
}

bool UPHCFollowManager::IsFollowing(int32 PlayerId) const
{
	return FollowingCache.Contains(PlayerId);
}

// ---------------------------------------------------------------------------
// Social Lists
// ---------------------------------------------------------------------------

void UPHCFollowManager::GetFollowers(int32 PlayerId, TFunction<void(TArray<FPHCFollowInfo>)> Callback)
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCFollowManager::GetFollowers -- HttpClient is null."));
		if (Callback)
		{
			Callback(TArray<FPHCFollowInfo>());
		}
		return;
	}

	const FString Endpoint = FString::Printf(TEXT("/api/social/followers/%d/"), PlayerId);

	TFunction<void(TArray<FPHCFollowInfo>)> CapturedCallback = MoveTemp(Callback);

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this, CapturedCallback](bool bSuccess, const FString& ResponseBody)
	{
		TArray<FPHCFollowInfo> Results;

		if (bSuccess)
		{
			Results = ParseFollowList(ResponseBody);
			UE_LOG(LogPlayhouseCity, Log,
				TEXT("PHCFollowManager::GetFollowers -- Received %d followers."), Results.Num());
		}
		else
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCFollowManager::GetFollowers -- Request failed."));
		}

		if (CapturedCallback)
		{
			CapturedCallback(Results);
		}
	});

	HttpClient->Get(Endpoint, ResponseDelegate, /*CacheTTL=*/ 30.0f);
}

void UPHCFollowManager::GetFollowing(int32 PlayerId, TFunction<void(TArray<FPHCFollowInfo>)> Callback)
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCFollowManager::GetFollowing -- HttpClient is null."));
		if (Callback)
		{
			Callback(TArray<FPHCFollowInfo>());
		}
		return;
	}

	const FString Endpoint = FString::Printf(TEXT("/api/social/following/%d/"), PlayerId);

	TFunction<void(TArray<FPHCFollowInfo>)> CapturedCallback = MoveTemp(Callback);

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this, CapturedCallback](bool bSuccess, const FString& ResponseBody)
	{
		TArray<FPHCFollowInfo> Results;

		if (bSuccess)
		{
			Results = ParseFollowList(ResponseBody);
			UE_LOG(LogPlayhouseCity, Log,
				TEXT("PHCFollowManager::GetFollowing -- Received %d following."), Results.Num());
		}
		else
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCFollowManager::GetFollowing -- Request failed."));
		}

		if (CapturedCallback)
		{
			CapturedCallback(Results);
		}
	});

	HttpClient->Get(Endpoint, ResponseDelegate, /*CacheTTL=*/ 30.0f);
}

void UPHCFollowManager::RefreshFollowingCache()
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCFollowManager::RefreshFollowingCache -- HttpClient is null."));
		return;
	}

	// Use player ID 0 as a sentinel meaning "current user" in the API
	const FString Endpoint = TEXT("/api/social/following/me/");

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this](bool bSuccess, const FString& ResponseBody)
	{
		if (!bSuccess)
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCFollowManager::RefreshFollowingCache -- Failed to refresh cache."));
			return;
		}

		TArray<FPHCFollowInfo> FollowList = ParseFollowList(ResponseBody);

		FollowingCache.Empty();
		for (const FPHCFollowInfo& Info : FollowList)
		{
			FollowingCache.Add(Info.PlayerId);
		}

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHCFollowManager::RefreshFollowingCache -- Cache refreshed with %d entries."),
			FollowingCache.Num());
	});

	HttpClient->Get(Endpoint, ResponseDelegate, /*CacheTTL=*/ 0.0f);
}

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------

void UPHCFollowManager::OnFollowResponse(bool bSuccess, const FString& Response, int32 PlayerId, bool bFollow)
{
	if (!bSuccess)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCFollowManager::OnFollowResponse -- %s request failed for player %d."),
			bFollow ? TEXT("Follow") : TEXT("Unfollow"), PlayerId);
		return;
	}

	// Parse response to confirm success
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		// Check if the server returned an error
		if (JsonObject->HasField(TEXT("error")))
		{
			const FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCFollowManager::OnFollowResponse -- Server error for player %d: %s"),
				PlayerId, *ErrorMsg);
			return;
		}
	}

	// Update local cache
	if (bFollow)
	{
		FollowingCache.Add(PlayerId);
		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHCFollowManager::OnFollowResponse -- Now following player %d. Cache size: %d."),
			PlayerId, FollowingCache.Num());
	}
	else
	{
		FollowingCache.Remove(PlayerId);
		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHCFollowManager::OnFollowResponse -- Unfollowed player %d. Cache size: %d."),
			PlayerId, FollowingCache.Num());
	}

	OnFollowChanged.Broadcast(PlayerId, bFollow);
}

TArray<FPHCFollowInfo> UPHCFollowManager::ParseFollowList(const FString& JsonResponse) const
{
	TArray<FPHCFollowInfo> Results;

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCFollowManager::ParseFollowList -- Failed to parse JSON response."));
		return Results;
	}

	// The API returns { "results": [...] } or directly an array
	const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;

	if (RootObject->TryGetArrayField(TEXT("results"), ResultsArray))
	{
		// Paginated response format
	}
	else
	{
		// Try to parse the root as an object with a "data" field
		if (!RootObject->TryGetArrayField(TEXT("data"), ResultsArray))
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCFollowManager::ParseFollowList -- No 'results' or 'data' array found in response."));
			return Results;
		}
	}

	if (!ResultsArray)
	{
		return Results;
	}

	Results.Reserve(ResultsArray->Num());

	for (const TSharedPtr<FJsonValue>& Value : *ResultsArray)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>& PlayerObj = Value->AsObject();
		if (!PlayerObj.IsValid())
		{
			continue;
		}

		FPHCFollowInfo Info;
		Info.PlayerId    = static_cast<int32>(PlayerObj->GetNumberField(TEXT("player_id")));
		Info.Username    = PlayerObj->GetStringField(TEXT("username"));
		Info.AvatarUrl   = PlayerObj->GetStringField(TEXT("avatar_url"));
		Info.bIsFollowing = PlayerObj->HasField(TEXT("is_following"))
			? PlayerObj->GetBoolField(TEXT("is_following"))
			: false;
		Info.bIsOnline = PlayerObj->HasField(TEXT("is_online"))
			? PlayerObj->GetBoolField(TEXT("is_online"))
			: false;

		Results.Add(Info);
	}

	return Results;
}
