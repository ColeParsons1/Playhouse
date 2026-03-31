// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCGameInstance.h"
#include "PlayhouseCity.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/NetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/NetworkVersion.h"
#include "TimerManager.h"

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
#include "Misc/EngineVersion.h"
#include "Misc/App.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#if PLATFORM_IOS
#import <UserNotifications/UserNotifications.h>
#elif PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
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

#if WITH_EDITOR
	bPendingServerConnect = false; // Editor: generate city immediately, no server travel
#else
	bPendingServerConnect = true;
#endif

	CreateSubsystems();

	SetupPushNotifications();

	StartSessionLog();

	// Bind app lifecycle delegates so we can end the session on background/terminate
	FCoreDelegates::ApplicationWillDeactivateDelegate.AddLambda([this]()
	{
		EndSessionLog(TEXT("background"));
	});

	// Agora voice is initialized later when player enters the world
	// (not here — iOS audio session isn't ready during Init)
}

void UPHCGameInstance::Shutdown()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::Shutdown -- Tearing down PlayhouseCity game instance."));

	EndSessionLog(TEXT("normal"));

	// ApplicationWillDeactivateDelegate was bound with a lambda — it will be
	// cleaned up when the process exits.  No explicit removal needed.
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
#elif PLATFORM_ANDROID
	// Android push notifications via Firebase can be added later.
	// For now, register for remote notifications through UE's delegate system.
	FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate.AddUObject(
		this, &UPHCGameInstance::OnRegisteredForRemoteNotifications);
	UE_LOG(LogPlayhouseCity, Log, TEXT("SetupPushNotifications -- Android push notification setup (pending Firebase integration)."));
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
#if PLATFORM_IOS
	Body->SetStringField(TEXT("platform"), TEXT("ios"));
#elif PLATFORM_ANDROID
	Body->SetStringField(TEXT("platform"), TEXT("android"));
#else
	Body->SetStringField(TEXT("platform"), TEXT("desktop"));
#endif

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
// Session Logging
// ---------------------------------------------------------------------------

void UPHCGameInstance::StartSessionLog()
{
	SessionStartTime = FDateTime::UtcNow();

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();

	// Device ID — use the platform device ID (stable per device)
	FString DeviceId = FPlatformMisc::GetDeviceId();
	if (DeviceId.IsEmpty())
	{
		DeviceId = FPlatformMisc::GetMachineId().ToString();
	}
	Body->SetStringField(TEXT("device_id"), DeviceId);

	// Build info
	Body->SetStringField(TEXT("build_version"), FApp::GetBuildVersion());
#if UE_BUILD_SHIPPING
	Body->SetStringField(TEXT("build_config"), TEXT("Shipping"));
#elif UE_BUILD_DEVELOPMENT
	Body->SetStringField(TEXT("build_config"), TEXT("Development"));
#else
	Body->SetStringField(TEXT("build_config"), TEXT("Debug"));
#endif
	Body->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());

	// Platform
#if PLATFORM_IOS
	Body->SetStringField(TEXT("platform"), TEXT("IOS"));
	Body->SetStringField(TEXT("device_model"), FPlatformMisc::GetDefaultDeviceProfileName());
	Body->SetStringField(TEXT("os_version"), FPlatformMisc::GetOSVersion());
#elif PLATFORM_ANDROID
	Body->SetStringField(TEXT("platform"), TEXT("Android"));
	Body->SetStringField(TEXT("device_model"), FPlatformMisc::GetDefaultDeviceProfileName());
	Body->SetStringField(TEXT("os_version"), FPlatformMisc::GetOSVersion());
#elif PLATFORM_MAC
	Body->SetStringField(TEXT("platform"), TEXT("Mac"));
#else
	Body->SetStringField(TEXT("platform"), TEXT("Windows"));
#endif

	// Screen resolution
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		Body->SetStringField(TEXT("screen_resolution"),
			FString::Printf(TEXT("%dx%d"), (int32)ViewportSize.X, (int32)ViewportSize.Y));
	}

	// Player ID if already logged in (e.g. auto-login)
	if (bIsLoggedIn && LocalPlayerProfile.PlayerId > 0)
	{
		Body->SetNumberField(TEXT("player_id"), LocalPlayerProfile.PlayerId);
	}

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = ServerBaseUrl / TEXT("api/session/start/");
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
				TSharedPtr<FJsonObject> Json;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
				{
					SessionLogId = (int32)Json->GetNumberField(TEXT("session_id"));
					UE_LOG(LogPlayhouseCity, Log, TEXT("Session log started: ID=%d"), SessionLogId);
				}
			}
			else
			{
				UE_LOG(LogPlayhouseCity, Warning, TEXT("Failed to start session log. HTTP %d"),
					Resp.IsValid() ? Resp->GetResponseCode() : 0);
			}
		});
	Request->ProcessRequest();
}

void UPHCGameInstance::EndSessionLog(const FString& ExitReason)
{
	if (bSessionEnded || SessionLogId <= 0) return;
	bSessionEnded = true;

	const FTimespan Duration = FDateTime::UtcNow() - SessionStartTime;

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("session_id"), SessionLogId);
	Body->SetNumberField(TEXT("duration_seconds"), (int32)Duration.GetTotalSeconds());
	Body->SetStringField(TEXT("exit_reason"), ExitReason);

	// Attach player ID if logged in (covers guest who logged in after session start)
	if (bIsLoggedIn && LocalPlayerProfile.PlayerId > 0)
	{
		Body->SetNumberField(TEXT("player_id"), LocalPlayerProfile.PlayerId);
	}

	// Multiplayer state
	Body->SetBoolField(TEXT("is_multiplayer"), bInMultiplayerSession);

	// Gather stats from the character if we can find it
	APlayerController* PC = GetFirstLocalPlayerController();
	if (PC)
	{
		APawn* Pawn = PC->GetPawn();
		if (Pawn)
		{
			Body->SetNumberField(TEXT("last_position_x"), Pawn->GetActorLocation().X);
			Body->SetNumberField(TEXT("last_position_y"), Pawn->GetActorLocation().Y);
		}
	}

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = ServerBaseUrl / TEXT("api/session/end/");
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(BodyStr);
	Request->ProcessRequest();

	UE_LOG(LogPlayhouseCity, Log, TEXT("Session log ended: ID=%d, Duration=%ds, Reason=%s"),
		SessionLogId, (int32)Duration.GetTotalSeconds(), *ExitReason);
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
	FAgoraVoiceManager::Get().LeaveChannel();
	bVoiceMuted = false;
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
