// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCGameInstance.h"
#include "PlayhouseCity.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/NetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/NetworkVersion.h"

// Subsystem headers -- uncomment each as the corresponding class is created.
// #include "Network/PHCHttpClient.h"
// #include "Network/PHCWebSocketClient.h"
#include "Theme/PHCThemeManager.h"
// #include "Economy/PHCEconomyManager.h"
#include "Auth/PHCAuthManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CoreDelegates.h"
#if PLATFORM_IOS
#import <UserNotifications/UserNotifications.h>
#endif

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCGameInstance::UPHCGameInstance()
{
	ServerBaseUrl = TEXT("https://tfplayhouse.com");
	WebSocketUrl  = TEXT("wss://tfplayhouse.com/ws/");
}

// ---------------------------------------------------------------------------
// UGameInstance interface
// ---------------------------------------------------------------------------

void UPHCGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::Init -- Initializing PlayhouseCity game instance."));

	// Override network version so source-built server and launcher-built client
	// always report the same version (both are UE 5.7.3).
	static constexpr uint32 PHCNetworkVersion = 1;
	FNetworkVersion::GetLocalNetworkVersionOverride.BindLambda([]() -> uint32
	{
		return PHCNetworkVersion;
	});
	FNetworkVersion::IsNetworkCompatibleOverride.BindLambda([](uint32 LocalVersion, uint32 RemoteVersion) -> bool
	{
		return true;
	});
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::Init -- Network version override set to %u."), PHCNetworkVersion);

	bPendingServerConnect = true;

	CreateSubsystems();

	SetupPushNotifications();

	// Initialize Agora global voice — all players join the same channel
	EnableVoiceChat();
}

void UPHCGameInstance::Shutdown()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::Shutdown -- Tearing down PlayhouseCity game instance."));

	FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.RemoveAll(this);

	DestroySubsystems();

	Super::Shutdown();
}

// ---------------------------------------------------------------------------
// Static helper
// ---------------------------------------------------------------------------

UPHCGameInstance* UPHCGameInstance::GetPHCGameInstance(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	return Cast<UPHCGameInstance>(GI);
}

// ---------------------------------------------------------------------------
// Login helpers
// ---------------------------------------------------------------------------

void UPHCGameInstance::HandleLoginSuccess(const FString& InToken, const FPHCPlayerProfile& InProfile)
{
	AuthToken         = InToken;
	LocalPlayerProfile = InProfile;
	bIsLoggedIn        = true;

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::HandleLoginSuccess -- User '%s' (ID %d) logged in."),
		*InProfile.Username, InProfile.PlayerId);

	OnLoginSuccess.Broadcast(LocalPlayerProfile);

	if (!APNsDeviceToken.IsEmpty() && !bDeviceTokenRegistered)
	{
		RegisterDeviceTokenWithBackend();
	}
}

void UPHCGameInstance::SaveCoinsToBackend()
{
	if (LocalPlayerProfile.PlayerId <= 0) return;

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("coins"), LocalPlayerProfile.Coins);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = ServerBaseUrl / FString::Printf(TEXT("api/players/%d/"), LocalPlayerProfile.PlayerId);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("PATCH"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(BodyStr);
	Request->ProcessRequest();
}

void UPHCGameInstance::HandleLogout()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::HandleLogout -- User '%s' logged out."),
		*LocalPlayerProfile.Username);

	AuthToken.Empty();
	bIsLoggedIn        = false;
	LocalPlayerProfile = FPHCPlayerProfile();

	OnLogoutComplete.Broadcast();

	bDeviceTokenRegistered = false;
}

// ---------------------------------------------------------------------------
// Multiplayer session management
// ---------------------------------------------------------------------------

bool UPHCGameInstance::HostGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("HostGame: No world available."));
		return false;
	}

	if (bInMultiplayerSession)
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("HostGame: Already in a multiplayer session."));
		return false;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("HostGame: Starting listen server on current world..."));

	// Use UWorld::Listen() to open a listen socket on the CURRENT world,
	// avoiding a full ServerTravel map reload (which is too heavy on iOS
	// with thousands of procedural city actors).
	FURL ListenURL;
	ListenURL.Port = 7777;

	if (World->Listen(ListenURL))
	{
		bInMultiplayerSession = true;
		bIsHost = true;
		EnableVoiceChat();
		UE_LOG(LogPlayhouseCity, Log, TEXT("HostGame: Listen server started on port 7777. Net mode: %d"), (int32)World->GetNetMode());
		return true;
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("HostGame: Failed to start listen server."));
		return false;
	}
}

void UPHCGameInstance::JoinGame(const FString& IPAddress)
{
	if (IPAddress.IsEmpty())
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("JoinGame: Empty IP address."));
		return;
	}

	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC)
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("JoinGame: No local player controller."));
		return;
	}

	FString TravelURL = FString::Printf(TEXT("%s:7777"), *IPAddress);
	UE_LOG(LogPlayhouseCity, Log, TEXT("JoinGame: Connecting to %s..."), *TravelURL);

	// Clear the pending flag so the map reload after ClientTravel generates the city
	bPendingServerConnect = false;

	PC->ClientTravel(TravelURL, TRAVEL_Absolute);

	bInMultiplayerSession = true;
	bIsHost = false;
	EnableVoiceChat();
}

void UPHCGameInstance::LeaveGame()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("LeaveGame: Disconnecting from session..."));

	UWorld* World = GetWorld();
	if (World)
	{
		if (bIsHost)
		{
			// Host: just travel to the map without ?listen
			World->ServerTravel(TEXT("/Game/Maps/City_Main"));
		}
		else
		{
			// Client: disconnect
			APlayerController* PC = GetFirstLocalPlayerController();
			if (PC)
			{
				PC->ClientTravel(TEXT("/Game/Maps/City_Main"), TRAVEL_Absolute);
			}
		}
	}

	bInMultiplayerSession = false;
	bIsHost = false;
	DisableVoiceChat();
}

// ---------------------------------------------------------------------------
// Push Notifications
// ---------------------------------------------------------------------------

void UPHCGameInstance::SetupPushNotifications()
{
#if PLATFORM_IOS
	FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.AddUObject(
		this, &UPHCGameInstance::OnRegisteredForRemoteNotifications);

	dispatch_async(dispatch_get_main_queue(), ^{
		UNUserNotificationCenter* Center = [UNUserNotificationCenter currentNotificationCenter];
		[Center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge)
			completionHandler:^(BOOL granted, NSError* _Nullable error) {
				if (granted)
				{
					dispatch_async(dispatch_get_main_queue(), ^{
						[[UIApplication sharedApplication] registerForRemoteNotifications];
					});
				}
			}];
	});

	UE_LOG(LogPlayhouseCity, Log, TEXT("SetupPushNotifications -- Requested iOS notification permission."));
#endif
}

void UPHCGameInstance::OnRegisteredForRemoteNotifications(TArray<uint8> InToken)
{
	FString HexToken;
	for (uint8 Byte : InToken)
	{
		HexToken += FString::Printf(TEXT("%02x"), Byte);
	}

	APNsDeviceToken = HexToken;
	UE_LOG(LogPlayhouseCity, Log, TEXT("APNs device token received: %s"), *APNsDeviceToken);

	if (bIsLoggedIn && !bDeviceTokenRegistered)
	{
		RegisterDeviceTokenWithBackend();
	}
}

void UPHCGameInstance::RegisterDeviceTokenWithBackend()
{
	if (APNsDeviceToken.IsEmpty() || LocalPlayerProfile.PlayerId <= 0)
	{
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("device_token"), APNsDeviceToken);
	Body->SetNumberField(TEXT("player_id"), LocalPlayerProfile.PlayerId);
	Body->SetStringField(TEXT("platform"), TEXT("ios"));

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = ServerBaseUrl / TEXT("api/push/register-device/");
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(BodyStr);
	Request->OnProcessRequestComplete().BindLambda(
		[this](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
		{
			if (bSuccess && Resp.IsValid() && Resp->GetResponseCode() >= 200 && Resp->GetResponseCode() < 300)
			{
				bDeviceTokenRegistered = true;
				UE_LOG(LogPlayhouseCity, Log, TEXT("Device token registered with backend."));
			}
			else
			{
				UE_LOG(LogPlayhouseCity, Warning, TEXT("Failed to register device token. HTTP %d"),
					Resp.IsValid() ? Resp->GetResponseCode() : 0);
			}
		});
	Request->ProcessRequest();
}

// ---------------------------------------------------------------------------
// Subsystem lifecycle
// ---------------------------------------------------------------------------

void UPHCGameInstance::CreateSubsystems()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::CreateSubsystems -- Creating manager subsystems."));

	// NOTE: Uncomment each subsystem as the corresponding class is created.

	// HttpClient      = NewObject<UPHCHttpClient>(this, TEXT("PHCHttpClient"));
	// WebSocketClient = NewObject<UPHCWebSocketClient>(this, TEXT("PHCWebSocketClient"));
	// EconomyManager  = NewObject<UPHCEconomyManager>(this, TEXT("PHCEconomyManager"));

	// Phase 5 -- Theme System
	ThemeManager = NewObject<UPHCThemeManager>(this, TEXT("PHCThemeManager"));
	if (ThemeManager)
	{
		ThemeManager->Initialize();
	}

	// Phase 3 -- Authentication
	AuthManager = NewObject<UPHCAuthManager>(this, TEXT("PHCAuthManager"));
	if (AuthManager)
	{
		AuthManager->Initialize(this);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::CreateSubsystems -- Active subsystems created."));
}

void UPHCGameInstance::DestroySubsystems()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::DestroySubsystems -- Releasing manager subsystems."));

	// Subsystem UObjects are owned by this GameInstance (Outer = this).
	// Setting them to nullptr lets the GC collect them.
	HttpClient      = nullptr;
	WebSocketClient = nullptr;
	ThemeManager    = nullptr;
	EconomyManager  = nullptr;
	AuthManager     = nullptr;
}

// ---------------------------------------------------------------------------
// Voice Chat (Agora SDK — global, works without game server)
// ---------------------------------------------------------------------------

#include "Voice/AgoraVoiceManager.h"

void UPHCGameInstance::EnableVoiceChat()
{
	FAgoraVoiceManager& Agora = FAgoraVoiceManager::Get();
	if (!Agora.IsInChannel())
	{
		Agora.Initialize(TEXT("REDACTED_AGORA_APP_ID"));
		Agora.JoinChannel(TEXT("PlayhouseCity_Global"));
	}
	bVoiceMuted = false;
	UE_LOG(LogPlayhouseCity, Log, TEXT("Voice chat enabled (Agora)"));
}

void UPHCGameInstance::DisableVoiceChat()
{
	FAgoraVoiceManager& Agora = FAgoraVoiceManager::Get();
	Agora.LeaveChannel();
	bVoiceMuted = false;
	UE_LOG(LogPlayhouseCity, Log, TEXT("Voice chat disabled (Agora)"));
}

void UPHCGameInstance::ToggleVoiceMute()
{
	SetVoiceMuted(!bVoiceMuted);
}

void UPHCGameInstance::SetVoiceMuted(bool bMute)
{
	bVoiceMuted = bMute;
	FAgoraVoiceManager::Get().SetMuted(bMute);
	UE_LOG(LogPlayhouseCity, Log, TEXT("Voice mute: %s"), bMute ? TEXT("ON") : TEXT("OFF"));
}
