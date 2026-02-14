// Copyright PlayhouseCity. All Rights Reserved.

#include "Network/PHCHttpClient.h"
#include "Network/PHCApiCache.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCHttp, Log, All);

// ===================================================================
//  FPHCRoomCreateParams
// ===================================================================

FString FPHCRoomCreateParams::ToJsonString() const
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), RoomName);
	Obj->SetStringField(TEXT("theme"), Theme);
	Obj->SetNumberField(TEXT("max_players"), MaxPlayers);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Obj, Writer);
	return Output;
}

// ===================================================================
//  UPHCHttpClient
// ===================================================================

UPHCHttpClient::UPHCHttpClient()
	: ServerBaseUrl(TEXT("http://localhost:8000"))
{
}

// -------------------------------------------------------------------
//  Initialization
// -------------------------------------------------------------------

void UPHCHttpClient::Initialize(const FString& BaseUrl)
{
	// Strip trailing slash for consistent URL building.
	ServerBaseUrl = BaseUrl;
	if (ServerBaseUrl.EndsWith(TEXT("/")))
	{
		ServerBaseUrl.RemoveFromEnd(TEXT("/"));
	}
	UE_LOG(LogPHCHttp, Log, TEXT("PHCHttpClient initialized with base URL: %s"), *ServerBaseUrl);
}

void UPHCHttpClient::SetAuthToken(const FString& Token)
{
	AuthToken = Token;
	UE_LOG(LogPHCHttp, Log, TEXT("Auth token %s"), Token.IsEmpty() ? TEXT("cleared") : TEXT("set"));
}

void UPHCHttpClient::SetApiCache(UPHCApiCache* InCache)
{
	ApiCache = InCache;
}

// -------------------------------------------------------------------
//  Generic REST verbs (C++ native delegates)
// -------------------------------------------------------------------

void UPHCHttpClient::Get(const FString& Endpoint, const FPHCHttpResponse& Callback, float CacheTTL)
{
	// Check cache first
	if (ApiCache && CacheTTL > 0.f)
	{
		FString CachedData;
		const FString CacheKey = MakeUrl(Endpoint);
		if (ApiCache->Get(CacheKey, CachedData))
		{
			UE_LOG(LogPHCHttp, Verbose, TEXT("Returning cached response for: %s"), *Endpoint);
			Callback.ExecuteIfBound(true, CachedData);
			return;
		}
	}

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);

	// Wrap callback to handle caching
	FPHCHttpResponse WrappedCallback;
	if (ApiCache && CacheTTL > 0.f)
	{
		const FString CacheKey = MakeUrl(Endpoint);
		TObjectPtr<UPHCApiCache> CacheRef = ApiCache;
		WrappedCallback = FPHCHttpResponse::CreateLambda(
			[Callback, CacheRef, CacheKey, CacheTTL](bool bSuccess, const FString& Body)
			{
				if (bSuccess && CacheRef.Get())
				{
					CacheRef->Set(CacheKey, Body, CacheTTL);
				}
				Callback.ExecuteIfBound(bSuccess, Body);
			});
	}
	else
	{
		WrappedCallback = Callback;
	}

	ProcessRequest(Request, WrappedCallback);
}

void UPHCHttpClient::Post(const FString& Endpoint, const FString& JsonBody, const FPHCHttpResponse& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), Endpoint, JsonBody);
	ProcessRequest(Request, Callback);
}

void UPHCHttpClient::Put(const FString& Endpoint, const FString& JsonBody, const FPHCHttpResponse& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("PUT"), Endpoint, JsonBody);
	ProcessRequest(Request, Callback);
}

void UPHCHttpClient::Delete(const FString& Endpoint, const FPHCHttpResponse& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("DELETE"), Endpoint);
	ProcessRequest(Request, Callback);
}

// -------------------------------------------------------------------
//  Generic REST verbs (Blueprint dynamic delegates)
// -------------------------------------------------------------------

void UPHCHttpClient::K2_Get(const FString& Endpoint, const FPHCJsonResponseDynamic& Callback, float CacheTTL)
{
	// Check cache first
	if (ApiCache && CacheTTL > 0.f)
	{
		FString CachedData;
		const FString CacheKey = MakeUrl(Endpoint);
		if (ApiCache->Get(CacheKey, CachedData))
		{
			Callback.ExecuteIfBound(true, 200, CachedData);
			return;
		}
	}

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);

	if (ApiCache && CacheTTL > 0.f)
	{
		const FString CacheKey = MakeUrl(Endpoint);
		TObjectPtr<UPHCApiCache> CacheRef = ApiCache;
		FPHCJsonResponseDynamic CallbackCopy = Callback;

		// We use the raw ProcessRequestDynamic but wrap caching via the OnComplete
		Request->OnProcessRequestComplete().BindLambda(
			[CallbackCopy, CacheRef, CacheKey, CacheTTL](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
			{
				const bool bSuccess = bConnected && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode());
				const int32 StatusCode = Resp.IsValid() ? Resp->GetResponseCode() : 0;
				const FString Body = Resp.IsValid() ? Resp->GetContentAsString() : FString();

				if (bSuccess && CacheRef.Get())
				{
					CacheRef->Set(CacheKey, Body, CacheTTL);
				}

				CallbackCopy.ExecuteIfBound(bSuccess, StatusCode, Body);
			});
		Request->ProcessRequest();
	}
	else
	{
		ProcessRequestDynamic(Request, Callback);
	}
}

void UPHCHttpClient::K2_Post(const FString& Endpoint, const FString& JsonBody, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), Endpoint, JsonBody);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::K2_Put(const FString& Endpoint, const FString& JsonBody, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("PUT"), Endpoint, JsonBody);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::K2_Delete(const FString& Endpoint, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("DELETE"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Auth endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::Login(const FString& Email, const FString& Password, const FPHCAuthResponseDynamic& Callback)
{
	TMap<FString, FString> Fields;
	Fields.Add(TEXT("email"), Email);
	Fields.Add(TEXT("password"), Password);

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/auth/login/"), BuildJsonBody(Fields));

	FPHCAuthResponseDynamic CallbackCopy = Callback;
	TWeakObjectPtr<UPHCHttpClient> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[CallbackCopy, WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			const bool bSuccess = bConnected && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode());
			FString Token;

			if (bSuccess)
			{
				Token = ExtractTokenFromResponse(Resp->GetContentAsString());
				if (WeakThis.IsValid() && !Token.IsEmpty())
				{
					WeakThis->SetAuthToken(Token);
				}
			}
			else
			{
				UE_LOG(LogPHCHttp, Warning, TEXT("Login failed. Status: %d"),
					Resp.IsValid() ? Resp->GetResponseCode() : 0);
			}

			CallbackCopy.ExecuteIfBound(bSuccess && !Token.IsEmpty(), Token);
		});

	Request->ProcessRequest();
}

void UPHCHttpClient::Register(const FString& Username, const FString& Email, const FString& Password, const FPHCAuthResponseDynamic& Callback)
{
	TMap<FString, FString> Fields;
	Fields.Add(TEXT("username"), Username);
	Fields.Add(TEXT("email"), Email);
	Fields.Add(TEXT("password"), Password);

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/auth/register/"), BuildJsonBody(Fields));

	FPHCAuthResponseDynamic CallbackCopy = Callback;
	TWeakObjectPtr<UPHCHttpClient> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[CallbackCopy, WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			const bool bSuccess = bConnected && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode());
			FString Token;

			if (bSuccess)
			{
				Token = ExtractTokenFromResponse(Resp->GetContentAsString());
				if (WeakThis.IsValid() && !Token.IsEmpty())
				{
					WeakThis->SetAuthToken(Token);
				}
			}
			else
			{
				UE_LOG(LogPHCHttp, Warning, TEXT("Register failed. Status: %d"),
					Resp.IsValid() ? Resp->GetResponseCode() : 0);
			}

			CallbackCopy.ExecuteIfBound(bSuccess && !Token.IsEmpty(), Token);
		});

	Request->ProcessRequest();
}

void UPHCHttpClient::GuestLogin(const FPHCAuthResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/auth/guest/"), TEXT("{}"));

	FPHCAuthResponseDynamic CallbackCopy = Callback;
	TWeakObjectPtr<UPHCHttpClient> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[CallbackCopy, WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			const bool bSuccess = bConnected && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode());
			FString Token;

			if (bSuccess)
			{
				Token = ExtractTokenFromResponse(Resp->GetContentAsString());
				if (WeakThis.IsValid() && !Token.IsEmpty())
				{
					WeakThis->SetAuthToken(Token);
				}
			}
			else
			{
				UE_LOG(LogPHCHttp, Warning, TEXT("Guest login failed. Status: %d"),
					Resp.IsValid() ? Resp->GetResponseCode() : 0);
			}

			CallbackCopy.ExecuteIfBound(bSuccess && !Token.IsEmpty(), Token);
		});

	Request->ProcessRequest();
}

void UPHCHttpClient::Logout(const FPHCSimpleResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/auth/logout/"), TEXT("{}"));

	FPHCSimpleResponseDynamic CallbackCopy = Callback;
	TWeakObjectPtr<UPHCHttpClient> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[CallbackCopy, WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			const bool bSuccess = bConnected && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode());

			if (WeakThis.IsValid())
			{
				WeakThis->SetAuthToken(FString());
			}

			const FString Message = bSuccess ? TEXT("Logged out successfully") : TEXT("Logout request failed");
			CallbackCopy.ExecuteIfBound(bSuccess, Message);
		});

	Request->ProcessRequest();
}

// -------------------------------------------------------------------
//  Profile endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::GetProfile(int32 PlayerId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/profiles/%d/"), PlayerId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::UpdateProfile(const FString& ProfileDataJson, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("PUT"), TEXT("/api/profiles/me/"), ProfileDataJson);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::GetMyProfile(const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), TEXT("/api/profiles/me/"));
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Avatar endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::GetAvatar(int32 PlayerId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/avatars/%d/"), PlayerId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::SaveAvatar(const FString& AvatarJsonBody, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/avatars/me/"), AvatarJsonBody);
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Shop endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::GetShopItems(const FString& Category, const FPHCJsonResponseDynamic& Callback)
{
	FString Endpoint = TEXT("/api/shop/items/");
	if (!Category.IsEmpty())
	{
		Endpoint += FString::Printf(TEXT("?category=%s"), *Category);
	}
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::PurchaseItem(int32 ItemId, const FPHCJsonResponseDynamic& Callback)
{
	TMap<FString, FString> Fields;
	Fields.Add(TEXT("item_id"), FString::FromInt(ItemId));
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/shop/purchase/"), BuildJsonBody(Fields));
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Room endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::GetRoomList(const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), TEXT("/api/rooms/"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::CreateRoom(const FString& Name, const FString& Theme, int32 MaxPlayers, const FPHCJsonResponseDynamic& Callback)
{
	FPHCRoomCreateParams Params;
	Params.RoomName = Name;
	Params.Theme = Theme;
	Params.MaxPlayers = MaxPlayers;
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/rooms/"), Params.ToJsonString());
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::JoinRoom(int32 RoomId, const FString& Password, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	if (!Password.IsEmpty())
	{
		Body->SetStringField(TEXT("password"), Password);
	}

	const FString Endpoint = FString::Printf(TEXT("/api/rooms/%d/join/"), RoomId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), Endpoint, BuildJsonBodyFromObject(Body));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::LeaveRoom(int32 RoomId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/rooms/%d/leave/"), RoomId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), Endpoint, TEXT("{}"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::GetRoomDetails(int32 RoomId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/rooms/%d/"), RoomId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Economy endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::GetBalance(const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), TEXT("/api/economy/balance/"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::ClaimDailyBonus(const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/economy/daily-bonus/claim/"), TEXT("{}"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::GetDailyBonusInfo(const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), TEXT("/api/economy/daily-bonus/"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::ScratchCard(const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/economy/scratch-card/"), TEXT("{}"));
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Garden endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::GetGarden(const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), TEXT("/api/garden/"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::PlantSeed(int32 PlotIndex, const FString& PlantType, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("plot_index"), PlotIndex);
	Body->SetStringField(TEXT("plant_type"), PlantType);

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/garden/plant/"), BuildJsonBodyFromObject(Body));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::WaterPlant(int32 PlotIndex, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("plot_index"), PlotIndex);

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/garden/water/"), BuildJsonBodyFromObject(Body));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::HarvestPlant(int32 PlotIndex, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("plot_index"), PlotIndex);

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/garden/harvest/"), BuildJsonBodyFromObject(Body));
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Leaderboard endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::GetLeaderboard(const FString& GameType, int32 Page, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/leaderboards/%s/?page=%d"), *GameType, Page);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::SubmitScore(const FString& GameType, int32 Score, const FPHCJsonResponseDynamic& Callback)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("game_type"), GameType);
	Body->SetNumberField(TEXT("score"), Score);

	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), TEXT("/api/leaderboards/submit/"), BuildJsonBodyFromObject(Body));
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Social endpoints
// -------------------------------------------------------------------

void UPHCHttpClient::FollowPlayer(int32 PlayerId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/social/follow/%d/"), PlayerId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), Endpoint, TEXT("{}"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::UnfollowPlayer(int32 PlayerId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/social/unfollow/%d/"), PlayerId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("POST"), Endpoint, TEXT("{}"));
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::GetFollowers(int32 PlayerId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/social/%d/followers/"), PlayerId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::GetFollowing(int32 PlayerId, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/social/%d/following/"), PlayerId);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

void UPHCHttpClient::SearchPlayers(const FString& Query, const FPHCJsonResponseDynamic& Callback)
{
	const FString Endpoint = FString::Printf(TEXT("/api/social/search/?q=%s"), *Query);
	TSharedRef<IHttpRequest> Request = CreateRequest(TEXT("GET"), Endpoint);
	ProcessRequestDynamic(Request, Callback);
}

// -------------------------------------------------------------------
//  Internal helpers
// -------------------------------------------------------------------

FString UPHCHttpClient::MakeUrl(const FString& Endpoint) const
{
	// If the endpoint already starts with http, treat it as absolute.
	if (Endpoint.StartsWith(TEXT("http://")) || Endpoint.StartsWith(TEXT("https://")))
	{
		return Endpoint;
	}

	// Ensure exactly one slash between base and endpoint.
	if (Endpoint.StartsWith(TEXT("/")))
	{
		return ServerBaseUrl + Endpoint;
	}
	return ServerBaseUrl + TEXT("/") + Endpoint;
}

TSharedRef<IHttpRequest> UPHCHttpClient::CreateRequest(const FString& Verb, const FString& Endpoint, const FString& JsonBody)
{
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest> Request = HttpModule.CreateRequest();

	Request->SetVerb(Verb);
	Request->SetURL(MakeUrl(Endpoint));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

	// Attach auth token if available.
	if (!AuthToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Token %s"), *AuthToken));
	}

	// Set body for verbs that carry a payload.
	if (!JsonBody.IsEmpty())
	{
		Request->SetContentAsString(JsonBody);
	}

	UE_LOG(LogPHCHttp, Verbose, TEXT("[%s] %s"), *Verb, *Request->GetURL());
	return Request;
}

void UPHCHttpClient::ProcessRequest(TSharedRef<IHttpRequest> Request, const FPHCHttpResponse& Callback)
{
	Request->OnProcessRequestComplete().BindLambda(
		[Callback](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogPHCHttp, Warning, TEXT("HTTP request failed: no connection or invalid response. URL: %s"),
					*Req->GetURL());
				Callback.ExecuteIfBound(false, FString());
				return;
			}

			const int32 StatusCode = Resp->GetResponseCode();
			const FString Body = Resp->GetContentAsString();
			const bool bSuccess = EHttpResponseCodes::IsOk(StatusCode);

			UE_LOG(LogPHCHttp, Log, TEXT("HTTP %d  %s  (%d bytes)"),
				StatusCode, *Req->GetURL(), Body.Len());

			if (!bSuccess)
			{
				UE_LOG(LogPHCHttp, Warning, TEXT("HTTP error body: %s"), *Body.Left(512));
			}

			Callback.ExecuteIfBound(bSuccess, Body);
		});

	Request->ProcessRequest();
}

void UPHCHttpClient::ProcessRequestDynamic(TSharedRef<IHttpRequest> Request, const FPHCJsonResponseDynamic& Callback)
{
	FPHCJsonResponseDynamic CallbackCopy = Callback;

	Request->OnProcessRequestComplete().BindLambda(
		[CallbackCopy](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogPHCHttp, Warning, TEXT("HTTP request failed: no connection or invalid response. URL: %s"),
					*Req->GetURL());
				CallbackCopy.ExecuteIfBound(false, 0, FString());
				return;
			}

			const int32 StatusCode = Resp->GetResponseCode();
			const FString Body = Resp->GetContentAsString();
			const bool bSuccess = EHttpResponseCodes::IsOk(StatusCode);

			UE_LOG(LogPHCHttp, Log, TEXT("HTTP %d  %s  (%d bytes)"),
				StatusCode, *Req->GetURL(), Body.Len());

			if (!bSuccess)
			{
				UE_LOG(LogPHCHttp, Warning, TEXT("HTTP error body: %s"), *Body.Left(512));
			}

			CallbackCopy.ExecuteIfBound(bSuccess, StatusCode, Body);
		});

	Request->ProcessRequest();
}

FString UPHCHttpClient::BuildJsonBody(const TMap<FString, FString>& Fields)
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	for (const auto& Pair : Fields)
	{
		JsonObject->SetStringField(Pair.Key, Pair.Value);
	}
	return BuildJsonBodyFromObject(JsonObject);
}

FString UPHCHttpClient::BuildJsonBodyFromObject(const TSharedRef<FJsonObject>& JsonObject)
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

FString UPHCHttpClient::ExtractTokenFromResponse(const FString& ResponseBody)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogPHCHttp, Warning, TEXT("Failed to parse auth response JSON"));
		return FString();
	}

	FString Token;
	if (JsonObject->TryGetStringField(TEXT("token"), Token))
	{
		return Token;
	}

	// Some backends nest the token under "key" or "auth_token".
	if (JsonObject->TryGetStringField(TEXT("key"), Token))
	{
		return Token;
	}
	if (JsonObject->TryGetStringField(TEXT("auth_token"), Token))
	{
		return Token;
	}

	UE_LOG(LogPHCHttp, Warning, TEXT("Auth response did not contain a token field. Body: %s"), *ResponseBody.Left(256));
	return FString();
}
