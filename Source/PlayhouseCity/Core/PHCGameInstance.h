// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/PHCTypes.h"
#include "PHCGameInstance.generated.h"

// Forward declarations for subsystem managers
class UPHCHttpClient;
class UPHCWebSocketClient;
class UPHCThemeManager;
class UPHCEconomyManager;
class UPHCAuthManager;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast when the local player successfully authenticates. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPHCLoginSuccess, const FPHCPlayerProfile&, PlayerProfile);

/** Broadcast when the local player logs out (voluntarily or session expired). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPHCLogoutComplete);

// ---------------------------------------------------------------------------
// UPHCGameInstance
// ---------------------------------------------------------------------------

/**
 * Root game instance for PlayhouseCity.
 *
 * Owns all major subsystem UObjects (HTTP, WebSocket, Theme, Economy, Auth)
 * and holds the authoritative local-player profile + auth token for the
 * lifetime of the application.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPHCGameInstance();

	// -- UGameInstance interface -----------------------------------------------
	virtual void Init() override;
	virtual void Shutdown() override;

	// -- Static helper --------------------------------------------------------

	/** Convenience accessor from any world context. Returns nullptr if the
	 *  world / game-instance is unavailable or is not a UPHCGameInstance. */
	UFUNCTION(BlueprintPure, Category = "PHC|Core", meta = (WorldContext = "WorldContextObject"))
	static UPHCGameInstance* GetPHCGameInstance(const UObject* WorldContextObject);

	// -- Auth state ------------------------------------------------------------

	/** The JWT / token string returned by the backend on login. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Auth")
	FString AuthToken;

	/** True after a successful login and before logout / expiry. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Auth")
	bool bIsLoggedIn = false;

	/** True while the player is inside a mini-game session. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Game")
	bool bInMiniGame = false;

	/** The current gameplay mode (open world, mini-game, vehicle, etc.). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Game")
	EPHCGameplayMode CurrentGameplayMode = EPHCGameplayMode::OpenWorld;

	/** Full profile of the locally-authenticated player. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Player")
	FPHCPlayerProfile LocalPlayerProfile;

	// -- Server URLs ----------------------------------------------------------

	/** Base URL for all REST API calls (no trailing slash). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Network")
	FString ServerBaseUrl = TEXT("https://tfplayhouse.com");

	/** Base URL for the WebSocket connection (no trailing slash). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Network")
	FString WebSocketUrl = TEXT("wss://tfplayhouse.com/ws/");

	// -- Subsystem accessors --------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|Subsystems")
	UPHCHttpClient* GetHttpClient() const { return HttpClient; }

	UFUNCTION(BlueprintPure, Category = "PHC|Subsystems")
	UPHCWebSocketClient* GetWebSocketClient() const { return WebSocketClient; }

	UFUNCTION(BlueprintPure, Category = "PHC|Subsystems")
	UPHCThemeManager* GetThemeManager() const { return ThemeManager; }

	UFUNCTION(BlueprintPure, Category = "PHC|Subsystems")
	UPHCEconomyManager* GetEconomyManager() const { return EconomyManager; }

	UFUNCTION(BlueprintPure, Category = "PHC|Subsystems")
	UPHCAuthManager* GetAuthManager() const { return AuthManager; }

	// -- Delegates ------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Auth")
	FOnPHCLoginSuccess OnLoginSuccess;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Auth")
	FOnPHCLogoutComplete OnLogoutComplete;

	// -- Login helpers (called by AuthManager) ---------------------------------

	/** Called by the auth subsystem once authentication succeeds. Stores the
	 *  token and profile, flips bIsLoggedIn, then broadcasts OnLoginSuccess. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void HandleLoginSuccess(const FString& InToken, const FPHCPlayerProfile& InProfile);

	/** Persist the current coin balance to the backend (fire-and-forget). */
	void SaveCoinsToBackend();

	/** Clears auth state and broadcasts OnLogoutComplete. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void HandleLogout();

	// -- Multiplayer session management ---------------------------------------

	/** Host a listen-server game. Other players connect via this device's IP.
	 *  Returns true if the listen server started successfully. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Multiplayer")
	bool HostGame();

	/** Join a game at the given IP address (e.g., "192.168.1.5"). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Multiplayer")
	void JoinGame(const FString& IPAddress);

	/** Leave the current multiplayer session and return to single-player. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Multiplayer")
	void LeaveGame();

	/** True when hosting or connected to a multiplayer session. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Multiplayer")
	bool bInMultiplayerSession = false;

	/** True when this instance is the host (listen server). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Multiplayer")
	bool bIsHost = false;

	/** Set during Init() — tells CitySubsystem to skip initial city generation
	 *  since ClientTravel will reload the map anyway. */
	bool bPendingServerConnect = false;

	// -- Push Notifications ---------------------------------------------------

	/** APNs device token (hex string), captured on iOS. */
	FString APNsDeviceToken;

	/** True once the device token has been POSTed to the backend. */
	bool bDeviceTokenRegistered = false;

	/** Request notification permission and bind the token delegate. */
	void SetupPushNotifications();

	/** Called when iOS provides the APNs device token. */
	void OnRegisteredForRemoteNotifications(TArray<uint8> InToken);

	/** POST the device token to the Django backend. */
	void RegisterDeviceTokenWithBackend();

private:
	// -- Owned subsystem instances (outer = this) -----------------------------

	UPROPERTY()
	TObjectPtr<UPHCHttpClient> HttpClient;

	UPROPERTY()
	TObjectPtr<UPHCWebSocketClient> WebSocketClient;

	UPROPERTY()
	TObjectPtr<UPHCThemeManager> ThemeManager;

	UPROPERTY()
	TObjectPtr<UPHCEconomyManager> EconomyManager;

	UPROPERTY()
	TObjectPtr<UPHCAuthManager> AuthManager;

	/** Create all subsystem UObjects. Called once from Init(). */
	void CreateSubsystems();

	/** Tear down subsystem UObjects. Called once from Shutdown(). */
	void DestroySubsystems();
};
