// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PHCStoreManager.generated.h"

class UPHCHttpClient;
class UPHCEconomyManager;

// ---------------------------------------------------------------------------
// Data Structs
// ---------------------------------------------------------------------------

/**
 * FPHCIAPProduct
 *
 * Represents a single In-App Purchase product available through the
 * Apple App Store. Mirrors the product information returned by StoreKit.
 */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCIAPProduct
{
	GENERATED_BODY()

	/** App Store product identifier (e.g. "com.playhousecity.gems.100"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	FString ProductId;

	/** Human-readable display name for the product. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	FString DisplayName;

	/** Product description text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	FString Description;

	/** Localized price string returned by StoreKit (e.g. "$0.99"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	FString PriceString;

	/** Raw price value in the store's currency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	float Price = 0.0f;

	/** Number of gems awarded for this product. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	int32 GemAmount = 0;

	/** Currency code (e.g. "USD", "EUR"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	FString CurrencyCode;

	/** Whether this product has been successfully queried from the App Store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Store")
	bool bIsAvailable = false;
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast when a purchase flow completes (success or failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPHCPurchaseComplete, bool, bSuccess, const FString&, ProductId);

/** Broadcast when the product query completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPHCProductsQueried, bool, bSuccess);

// ---------------------------------------------------------------------------
// UPHCStoreManager
// ---------------------------------------------------------------------------

/**
 * UPHCStoreManager
 *
 * Manages Apple In-App Purchases for PlayhouseCity gem packs. Handles
 * product queries, purchase flows, receipt validation with the Django
 * backend, and purchase restoration.
 *
 * Product catalog:
 *   - com.playhousecity.gems.100  = 100 gems
 *   - com.playhousecity.gems.500  = 500 gems
 *   - com.playhousecity.gems.1200 = 1200 gems
 *   - com.playhousecity.gems.5000 = 5000 gems
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCStoreManager : public UObject
{
	GENERATED_BODY()

public:
	UPHCStoreManager();

	// ---------------------------------------------------------------
	//  Initialization
	// ---------------------------------------------------------------

	/**
	 * Initialize the store manager. Sets up product definitions and
	 * binds to the Online Subsystem's purchase interface.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Store")
	void Initialize();

	/**
	 * Set external dependencies after Initialize().
	 *
	 * @param InHttpClient      HTTP client for receipt validation with the backend.
	 * @param InEconomyManager  Economy manager for crediting gems after purchase.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Store")
	void SetDependencies(UPHCHttpClient* InHttpClient, UPHCEconomyManager* InEconomyManager);

	// ---------------------------------------------------------------
	//  Product Queries
	// ---------------------------------------------------------------

	/**
	 * Query available IAP products from the App Store. Results are
	 * cached internally and accessible via GetAvailableProducts().
	 * Fires OnProductsQueried when complete.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Store")
	void QueryProducts();

	/**
	 * Get the list of all available products (only populated after a
	 * successful QueryProducts() call).
	 */
	UFUNCTION(BlueprintPure, Category = "PHC|Store")
	TArray<FPHCIAPProduct> GetAvailableProducts() const { return CachedProducts; }

	/**
	 * Get the localized price string for a specific product.
	 *
	 * @param ProductId  The App Store product identifier.
	 * @return The localized price string, or "N/A" if not available.
	 */
	UFUNCTION(BlueprintPure, Category = "PHC|Store")
	FString GetProductPrice(const FString& ProductId) const;

	// ---------------------------------------------------------------
	//  Purchase Flow
	// ---------------------------------------------------------------

	/**
	 * Initiate an IAP purchase flow for the specified product.
	 * The result is delivered via the OnPurchaseComplete delegate.
	 *
	 * @param ProductId  The App Store product identifier to purchase.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Store")
	void PurchaseProduct(const FString& ProductId);

	/**
	 * Restore previously completed purchases. Useful when a user
	 * reinstalls the app or switches devices.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Store")
	void RestorePurchases();

	// ---------------------------------------------------------------
	//  Delegates
	// ---------------------------------------------------------------

	/** Fired when a purchase completes or fails. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Store")
	FOnPHCPurchaseComplete OnPurchaseComplete;

	/** Fired when product query completes. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Store")
	FOnPHCProductsQueried OnProductsQueried;

private:
	// ---------------------------------------------------------------
	//  Internal Helpers
	// ---------------------------------------------------------------

	/** Build the initial product definitions with known gem amounts. */
	void SetupProductDefinitions();

	/**
	 * Handle the callback from the Online Subsystem product query.
	 *
	 * @param bSuccess  Whether the query succeeded.
	 */
	void OnQueryComplete(bool bSuccess);

	/**
	 * Handle the internal purchase completion from the Online Subsystem.
	 * Validates the receipt with the Django backend before crediting gems.
	 *
	 * @param ProductId    The product that was purchased.
	 * @param ReceiptData  The App Store receipt data for server-side validation.
	 * @param bSuccess     Whether the store-side purchase succeeded.
	 */
	void OnPurchaseComplete_Internal(const FString& ProductId, const FString& ReceiptData, bool bSuccess);

	/**
	 * Validate the purchase receipt with the Django backend.
	 *
	 * @param ProductId    The product that was purchased.
	 * @param ReceiptData  The receipt data to validate.
	 */
	void ValidateReceiptWithServer(const FString& ProductId, const FString& ReceiptData);

	/**
	 * Find a product definition by its product ID.
	 *
	 * @param ProductId  The product ID to look up.
	 * @return Pointer to the product, or nullptr if not found.
	 */
	FPHCIAPProduct* FindProduct(const FString& ProductId);

	/**
	 * Find a product definition by its product ID (const version).
	 *
	 * @param ProductId  The product ID to look up.
	 * @return Pointer to the product, or nullptr if not found.
	 */
	const FPHCIAPProduct* FindProduct(const FString& ProductId) const;

	/**
	 * Get the gem amount for a given product ID from the definition table.
	 *
	 * @param ProductId  The product ID to look up.
	 * @return The gem amount, or 0 if not found.
	 */
	int32 GetGemAmountForProduct(const FString& ProductId) const;

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** All known product IDs in order. */
	UPROPERTY()
	TArray<FString> ProductIds;

	/** Cached product information from the App Store. */
	UPROPERTY()
	TArray<FPHCIAPProduct> CachedProducts;

	/** Map from product ID to gem count for quick lookup. */
	TMap<FString, int32> ProductGemAmounts;

	/** Whether products have been successfully queried. */
	UPROPERTY()
	bool bProductsQueried;

	/** Whether a purchase is currently in progress. */
	UPROPERTY()
	bool bPurchaseInProgress;

	/** The product ID of the purchase currently in progress. */
	UPROPERTY()
	FString CurrentPurchaseProductId;

	/** HTTP client for receipt validation with the Django backend. */
	UPROPERTY()
	TObjectPtr<UPHCHttpClient> HttpClient;

	/** Economy manager for crediting gems after validated purchases. */
	UPROPERTY()
	TObjectPtr<UPHCEconomyManager> EconomyManager;
};
