// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PHCAuthManager.generated.h"

class UPHCGameInstance;

/**
 * Delegate broadcast when any authentication operation completes.
 * @param bSuccess      True if the operation succeeded.
 * @param ErrorMessage  Empty on success; contains a user-facing message on failure.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAuthResult, bool, bSuccess, const FString&, ErrorMessage);

/**
 * Manages all authentication state: login, registration, guest access,
 * and player-ID persistence.
 *
 * The Django backend at GAME_SERVER_IP uses a simple ID-based auth model:
 *   POST /api/players/login/    -> {success, player}
 *   POST /api/players/register/ -> {success, player}
 * No JWT tokens are involved. The full player profile is returned inline.
 *
 * Owned by the GameInstance.
 */
UCLASS(BlueprintType)
class PLAYHOUSECITY_API UPHCAuthManager : public UObject
{
	GENERATED_BODY()

public:
	UPHCAuthManager();

	// ----------------------------------------------------------------
	// Initialization
	// ----------------------------------------------------------------

	/** Call once after construction to bind the owning GameInstance. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void Initialize(UPHCGameInstance* InGameInstance);

	// ----------------------------------------------------------------
	// Player-ID persistence
	// ----------------------------------------------------------------

	/** Persists the player ID to local storage for auto-login. */
	void SavePlayerId(int32 PlayerId);

	/** Returns the persisted player ID, or 0 if none exists. */
	int32 LoadPlayerId() const;

	/** Removes the persisted player ID from local storage. */
	void ClearPlayerId();

	// ----------------------------------------------------------------
	// Authentication flows
	// ----------------------------------------------------------------

	/** Sign In with Apple (iOS). On non-iOS, falls back to GuestLogin. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void AppleSignIn();

	/** Try auto-login using a stored Apple user ID. Returns true if attempt was started. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	bool AutoLoginWithApple();

	/** Standard email + password login. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void Login(const FString& Email, const FString& Password);

	/** Create a new account. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void Register(const FString& Username, const FString& Email, const FString& Password);

	/** Fetch a player profile by saved player ID. Returns true if attempt was started. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	bool FetchProfileById(int32 PlayerId);

	/** Anonymous / guest session -- creates a local-only profile. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void GuestLogin();

	/** Log out: clears stored data and notifies the GameInstance. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void Logout();

	// ----------------------------------------------------------------
	// Delegates
	// ----------------------------------------------------------------

	/** Broadcast after every authentication attempt. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Auth")
	FOnAuthResult OnAuthResult;

	/** Called from the ObjC delegate when Apple Sign In completes (on game thread). */
	void OnAppleSignInComplete(const FString& IdentityToken, const FString& UserIdentifier,
		const FString& FullName, const FString& Email);

	/** Called from the ObjC delegate when Apple Sign In fails. */
	void OnAppleSignInFailed(const FString& ErrorMessage);

private:
	// ----------------------------------------------------------------
	// Internal helpers
	// ----------------------------------------------------------------

	/** File path for player-ID persistence. */
	FString GetPlayerIdFilePath() const;

	/** Common handler for login and register HTTP responses. */
	void HandleAuthResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Handler for Apple Sign In backend response -- also persists Apple user ID. */
	void HandleAppleAuthResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Handler for FetchProfileById GET response. */
	void HandleFetchProfileResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Parse the player object from a JSON response and apply login. */
	bool ParseAndApplyPlayer(const TSharedPtr<class FJsonObject>& JsonObject);

	/** Broadcasts the auth result on the game thread. */
	void BroadcastResult(bool bSuccess, const FString& ErrorMessage);

	/** Persists the Apple user identifier for auto-login. */
	void SaveAppleUserId(const FString& UserId);

	/** Returns the stored Apple user identifier, or empty if none. */
	FString LoadAppleUserId() const;

	// ----------------------------------------------------------------
	// State
	// ----------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UPHCGameInstance> GameInstance;

	/** True while an auth request is in-flight. */
	bool bRequestInFlight = false;

	/** The Apple user identifier from the most recent sign-in attempt (used in backend POST). */
	FString PendingAppleUserId;
};
