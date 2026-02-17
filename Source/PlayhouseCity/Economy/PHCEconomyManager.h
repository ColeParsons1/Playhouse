// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/PHCTypes.h"
#include "PHCEconomyManager.generated.h"

class UPHCHttpClient;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast whenever the locally cached balance changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPHCBalanceChanged, int32, NewCoins, int32, NewGems);

// ---------------------------------------------------------------------------
// UPHCEconomyManager
// ---------------------------------------------------------------------------

/**
 * UPHCEconomyManager
 *
 * Server-authoritative currency manager for PlayhouseCity. All spend
 * operations are validated by the Django backend; the client maintains
 * a local cache that is updated on every confirmed transaction and
 * periodic refresh.
 *
 * Supports two currency types: Coins (soft/earnable) and Gems (premium/purchasable).
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCEconomyManager : public UObject
{
	GENERATED_BODY()

public:
	UPHCEconomyManager();

	// ---------------------------------------------------------------
	//  Initialization
	// ---------------------------------------------------------------

	/**
	 * Initialize the economy manager with an HTTP client reference.
	 * Must be called before any other methods.
	 *
	 * @param InHttpClient  The HTTP client used for API communication.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Economy")
	void Initialize(UPHCHttpClient* InHttpClient);

	// ---------------------------------------------------------------
	//  Balance Queries
	// ---------------------------------------------------------------

	/**
	 * Refresh the locally cached balance from the server.
	 * On success, updates CachedCoins and CachedGems and fires OnBalanceChanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Economy")
	void RefreshBalance();

	/** Returns the locally cached coin balance. */
	UFUNCTION(BlueprintPure, Category = "PHC|Economy")
	int32 GetCoins() const { return CachedCoins; }

	/** Returns the locally cached gem balance. */
	UFUNCTION(BlueprintPure, Category = "PHC|Economy")
	int32 GetGems() const { return CachedGems; }

	/**
	 * Check whether the player can afford a given amount in the specified currency.
	 *
	 * @param Currency  The currency type to check (Coins or Gems).
	 * @param Amount    The amount required.
	 * @return True if the cached balance is sufficient.
	 */
	UFUNCTION(BlueprintPure, Category = "PHC|Economy")
	bool CanAfford(EPHCCurrencyType Currency, int32 Amount) const;

	// ---------------------------------------------------------------
	//  Spending (server-authoritative)
	// ---------------------------------------------------------------

	/**
	 * Spend coins on a server-validated transaction.
	 * The callback receives true if the server approved the spend.
	 *
	 * @param Amount    Number of coins to spend.
	 * @param Reason    Human-readable reason or item ID for audit logging.
	 * @param Callback  Called with the transaction result.
	 */
	void SpendCoins(int32 Amount, const FString& Reason, TFunction<void(bool)> Callback);

	/**
	 * Spend gems on a server-validated transaction.
	 * The callback receives true if the server approved the spend.
	 *
	 * @param Amount    Number of gems to spend.
	 * @param Reason    Human-readable reason or item ID for audit logging.
	 * @param Callback  Called with the transaction result.
	 */
	void SpendGems(int32 Amount, const FString& Reason, TFunction<void(bool)> Callback);

	// ---------------------------------------------------------------
	//  Earning (server-confirmed)
	// ---------------------------------------------------------------

	/**
	 * Record a coin earning that has already been confirmed by the server.
	 * Updates the local cache and fires OnBalanceChanged.
	 *
	 * @param Amount  Number of coins earned.
	 * @param Source  Description of the earning source (e.g. "game_reward", "daily_bonus").
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Economy")
	void EarnCoins(int32 Amount, const FString& Source);

	/**
	 * Record a gem earning that has already been confirmed by the server.
	 * Updates the local cache and fires OnBalanceChanged.
	 *
	 * @param Amount  Number of gems earned.
	 * @param Source  Description of the earning source (e.g. "iap_purchase", "achievement").
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Economy")
	void EarnGems(int32 Amount, const FString& Source);

	// ---------------------------------------------------------------
	//  Delegates
	// ---------------------------------------------------------------

	/** Fired whenever the cached coin or gem balance changes. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Economy")
	FOnPHCBalanceChanged OnBalanceChanged;

private:
	// ---------------------------------------------------------------
	//  Internal Helpers
	// ---------------------------------------------------------------

	/**
	 * Parse and apply a balance response from the server.
	 *
	 * @param bSuccess  Whether the HTTP request succeeded.
	 * @param Response  The raw JSON response body.
	 */
	void OnBalanceResponse(bool bSuccess, const FString& Response);

	/**
	 * Internal spend implementation shared by SpendCoins and SpendGems.
	 *
	 * @param CurrencyName  "coins" or "gems" for the API endpoint.
	 * @param Amount        Amount to spend.
	 * @param Reason        Audit reason string.
	 * @param Callback      Result callback.
	 */
	void SpendCurrency(const FString& CurrencyName, int32 Amount, const FString& Reason, TFunction<void(bool)> Callback);

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** Locally cached coin balance. Updated on server responses. */
	UPROPERTY()
	int32 CachedCoins;

	/** Locally cached gem balance. Updated on server responses. */
	UPROPERTY()
	int32 CachedGems;

	/** Cached reference to the HTTP client for API calls. */
	UPROPERTY()
	TObjectPtr<UPHCHttpClient> HttpClient;
};
