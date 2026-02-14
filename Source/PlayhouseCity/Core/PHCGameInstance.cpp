// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCGameInstance.h"
#include "PlayhouseCity.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

// Subsystem headers -- uncomment each as the corresponding class is created.
// #include "Network/PHCHttpClient.h"
// #include "Network/PHCWebSocketClient.h"
#include "Theme/PHCThemeManager.h"
// #include "Economy/PHCEconomyManager.h"
#include "Auth/PHCAuthManager.h"

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

	CreateSubsystems();
}

void UPHCGameInstance::Shutdown()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::Shutdown -- Tearing down PlayhouseCity game instance."));

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
}

void UPHCGameInstance::HandleLogout()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCGameInstance::HandleLogout -- User '%s' logged out."),
		*LocalPlayerProfile.Username);

	AuthToken.Empty();
	bIsLoggedIn        = false;
	LocalPlayerProfile = FPHCPlayerProfile();

	OnLogoutComplete.Broadcast();
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
