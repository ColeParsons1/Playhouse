// Copyright PlayhouseCity. All Rights Reserved.

#include "Auth/PHCAuthManager.h"
#include "Core/PHCGameInstance.h"
#include "PlayhouseCity.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"

// ===========================================================================
// Construction
// ===========================================================================

UPHCAuthManager::UPHCAuthManager()
{
}

// ===========================================================================
// Initialization
// ===========================================================================

void UPHCAuthManager::Initialize(UPHCGameInstance* InGameInstance)
{
	if (!InGameInstance)
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("PHCAuthManager::Initialize called with null GameInstance."));
		return;
	}
	GameInstance = InGameInstance;
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCAuthManager::Initialize -- AuthManager bound to GameInstance."));
}

// ===========================================================================
// Player-ID persistence
// ===========================================================================

FString UPHCAuthManager::GetPlayerIdFilePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("Auth") / TEXT("player_id.dat");
}

void UPHCAuthManager::SavePlayerId(int32 PlayerId)
{
	const FString FilePath = GetPlayerIdFilePath();
	const FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	FFileHelper::SaveStringToFile(FString::FromInt(PlayerId), *FilePath);
}

int32 UPHCAuthManager::LoadPlayerId() const
{
	FString Content;
	const FString FilePath = GetPlayerIdFilePath();
	if (FFileHelper::LoadFileToString(Content, *FilePath))
	{
		Content.TrimStartAndEndInline();
		if (Content.IsNumeric())
		{
			return FCString::Atoi(*Content);
		}
	}
	return 0;
}

void UPHCAuthManager::ClearPlayerId()
{
	const FString FilePath = GetPlayerIdFilePath();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*FilePath))
	{
		PlatformFile.DeleteFile(*FilePath);
	}
}

// ===========================================================================
// Authentication flows
// ===========================================================================

void UPHCAuthManager::Login(const FString& Email, const FString& Password)
{
	if (!GameInstance)
	{
		BroadcastResult(false, TEXT("GameInstance not set."));
		return;
	}

	if (bRequestInFlight)
	{
		return;
	}

	if (Email.IsEmpty() || Password.IsEmpty())
	{
		BroadcastResult(false, TEXT("Email and password are required."));
		return;
	}

	bRequestInFlight = true;

	TSharedPtr<FJsonObject> JsonBody = MakeShared<FJsonObject>();
	JsonBody->SetStringField(TEXT("email"), Email);
	JsonBody->SetStringField(TEXT("password"), Password);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(JsonBody.ToSharedRef(), Writer);

	const FString Url = GameInstance->ServerBaseUrl / TEXT("api/players/login/");
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindUObject(this, &UPHCAuthManager::HandleAuthResponse);
	Request->ProcessRequest();

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCAuthManager::Login -- POST %s"), *Url);
}

void UPHCAuthManager::Register(const FString& Username, const FString& Email, const FString& Password)
{
	if (!GameInstance)
	{
		BroadcastResult(false, TEXT("GameInstance not set."));
		return;
	}

	if (bRequestInFlight)
	{
		return;
	}

	if (Username.IsEmpty() || Email.IsEmpty() || Password.IsEmpty())
	{
		BroadcastResult(false, TEXT("Username, email, and password are required."));
		return;
	}

	bRequestInFlight = true;

	// The Django API expects: name, email, password, confirmPassword
	TSharedPtr<FJsonObject> JsonBody = MakeShared<FJsonObject>();
	JsonBody->SetStringField(TEXT("name"), Username);
	JsonBody->SetStringField(TEXT("email"), Email);
	JsonBody->SetStringField(TEXT("password"), Password);
	JsonBody->SetStringField(TEXT("confirmPassword"), Password);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(JsonBody.ToSharedRef(), Writer);

	const FString Url = GameInstance->ServerBaseUrl / TEXT("api/players/register/");
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(BodyString);
	Request->OnProcessRequestComplete().BindUObject(this, &UPHCAuthManager::HandleAuthResponse);
	Request->ProcessRequest();

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCAuthManager::Register -- POST %s"), *Url);
}

void UPHCAuthManager::GuestLogin()
{
	if (!GameInstance)
	{
		BroadcastResult(false, TEXT("GameInstance not set."));
		return;
	}

	// Guest mode: create a local-only profile (no server call).
	FPHCPlayerProfile GuestProfile;
	GuestProfile.PlayerId = -1;
	GuestProfile.Username = TEXT("Guest");
	GuestProfile.DisplayName = TEXT("Guest");
	GuestProfile.bIsGuest = true;
	GuestProfile.Coins = 0;
	GuestProfile.Gems = 0;
	GuestProfile.Level = 1;

	GameInstance->HandleLoginSuccess(FString(), GuestProfile);

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCAuthManager::GuestLogin -- Local guest session created."));
	BroadcastResult(true, FString());
}

void UPHCAuthManager::Logout()
{
	ClearPlayerId();

	if (GameInstance)
	{
		GameInstance->HandleLogout();
	}

	BroadcastResult(true, FString());
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCAuthManager::Logout -- User logged out."));
}

// ===========================================================================
// HTTP response handler
// ===========================================================================

void UPHCAuthManager::HandleAuthResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bRequestInFlight = false;

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		BroadcastResult(false, TEXT("Could not reach the server. Please check your connection."));
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCAuthManager::HandleAuthResponse -- HTTP %d, Body: %.200s"), Code, *Body);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	const bool bParsed = FJsonSerializer::Deserialize(Reader, JsonObject);

	if (!bParsed || !JsonObject.IsValid())
	{
		BroadcastResult(false, FString::Printf(TEXT("Server returned invalid response (HTTP %d)."), Code));
		return;
	}

	// Check for success field
	bool bSuccess = false;
	if (JsonObject->TryGetBoolField(TEXT("success"), bSuccess) && bSuccess)
	{
		if (ParseAndApplyPlayer(JsonObject))
		{
			BroadcastResult(true, FString());
			return;
		}
		BroadcastResult(false, TEXT("Server response missing player data."));
		return;
	}

	// Error path -- extract a human-readable message.
	FString ErrorMessage = FString::Printf(TEXT("Request failed (HTTP %d)."), Code);
	FString ServerMsg;
	if (JsonObject->TryGetStringField(TEXT("error"), ServerMsg))
	{
		ErrorMessage = ServerMsg;
	}
	else if (JsonObject->TryGetStringField(TEXT("message"), ServerMsg))
	{
		ErrorMessage = ServerMsg;
	}

	BroadcastResult(false, ErrorMessage);
}

bool UPHCAuthManager::ParseAndApplyPlayer(const TSharedPtr<FJsonObject>& JsonObject)
{
	// The API returns: { "success": true, "player": { ... } }
	const TSharedPtr<FJsonObject>* PlayerObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("player"), PlayerObject) || !PlayerObject)
	{
		return false;
	}

	const TSharedPtr<FJsonObject>& P = *PlayerObject;

	FPHCPlayerProfile Profile;

	int32 IntVal = 0;
	FString StrVal;

	if (P->TryGetNumberField(TEXT("id"), IntVal))       { Profile.PlayerId = IntVal; }
	if (P->TryGetStringField(TEXT("name"), StrVal))      { Profile.Username = StrVal; Profile.DisplayName = StrVal; }
	if (P->TryGetStringField(TEXT("username"), StrVal))  { Profile.Username = StrVal; }
	if (P->TryGetStringField(TEXT("email"), StrVal))     { /* stored if needed */ }
	if (P->TryGetNumberField(TEXT("coins"), IntVal))     { Profile.Coins = IntVal; }
	if (P->TryGetNumberField(TEXT("gems"), IntVal))      { Profile.Gems = IntVal; }
	if (P->TryGetNumberField(TEXT("level"), IntVal))     { Profile.Level = IntVal; }
	if (P->TryGetNumberField(TEXT("xp"), IntVal))        { Profile.XP = IntVal; }
	if (P->TryGetNumberField(TEXT("followers"), IntVal)) { Profile.FollowerCount = IntVal; }

	bool bBoolVal = false;
	if (P->TryGetBoolField(TEXT("is_cpu"), bBoolVal) && bBoolVal)
	{
		Profile.bIsGuest = true;
	}

	// Persist the player ID for future auto-login.
	SavePlayerId(Profile.PlayerId);

	// Inform the GameInstance.
	if (GameInstance)
	{
		GameInstance->HandleLoginSuccess(FString::FromInt(Profile.PlayerId), Profile);
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCAuthManager -- Logged in as '%s' (ID %d), Coins: %d, Gems: %d"),
			*Profile.Username, Profile.PlayerId, Profile.Coins, Profile.Gems);
	}

	return true;
}

// ===========================================================================
// Helpers
// ===========================================================================

void UPHCAuthManager::BroadcastResult(bool bSuccess, const FString& ErrorMessage)
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UPHCAuthManager>(this), bSuccess, ErrorMessage]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAuthResult.Broadcast(bSuccess, ErrorMessage);
			}
		});
	}
	else
	{
		OnAuthResult.Broadcast(bSuccess, ErrorMessage);
	}
}
