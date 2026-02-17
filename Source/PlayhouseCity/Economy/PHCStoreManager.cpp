// Copyright PlayhouseCity. All Rights Reserved.

#include "Economy/PHCStoreManager.h"
#include "Economy/PHCEconomyManager.h"
#include "Network/PHCHttpClient.h"
#include "PlayhouseCity.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "OnlineSubsystem.h"
#include "OnlineError.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"

// ---------------------------------------------------------------------------
// Product ID Constants
// ---------------------------------------------------------------------------

namespace PHCStoreConstants
{
	static const FString GemPack100  = TEXT("com.playhousecity.gems.100");
	static const FString GemPack500  = TEXT("com.playhousecity.gems.500");
	static const FString GemPack1200 = TEXT("com.playhousecity.gems.1200");
	static const FString GemPack5000 = TEXT("com.playhousecity.gems.5000");
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCStoreManager::UPHCStoreManager()
	: bProductsQueried(false)
	, bPurchaseInProgress(false)
	, HttpClient(nullptr)
	, EconomyManager(nullptr)
{
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void UPHCStoreManager::Initialize()
{
	SetupProductDefinitions();

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCStoreManager::Initialize -- Store manager initialized with %d product definitions."),
		ProductIds.Num());
}

void UPHCStoreManager::SetDependencies(UPHCHttpClient* InHttpClient, UPHCEconomyManager* InEconomyManager)
{
	HttpClient = InHttpClient;
	EconomyManager = InEconomyManager;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCStoreManager::SetDependencies -- Dependencies set."));
}

// ---------------------------------------------------------------------------
// Product Queries
// ---------------------------------------------------------------------------

void UPHCStoreManager::QueryProducts()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCStoreManager::QueryProducts -- Online Subsystem is not available. Using local definitions only."));

		// Mark all products as available with placeholder prices for development
		for (FPHCIAPProduct& Product : CachedProducts)
		{
			Product.bIsAvailable = true;
			if (Product.PriceString.IsEmpty())
			{
				Product.PriceString = TEXT("$--");
			}
		}

		bProductsQueried = true;
		OnProductsQueried.Broadcast(true);
		return;
	}

	IOnlineStoreV2Ptr StoreInterface = OnlineSub->GetStoreV2Interface();
	if (!StoreInterface.IsValid())
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCStoreManager::QueryProducts -- StoreV2 interface is not available."));

		bProductsQueried = false;
		OnProductsQueried.Broadcast(false);
		return;
	}

	// Build the offer ID filter
	TArray<FString> OfferIds;
	for (const FString& Id : ProductIds)
	{
		OfferIds.Add(Id);
	}

	// Query the store for product offers
	FOnQueryOnlineStoreOffersComplete QueryDelegate;
	QueryDelegate.BindLambda([this](bool bWasSuccessful, const TArray<FUniqueOfferId>& OfferIds, const FString& ErrorMsg)
	{
		if (bWasSuccessful)
		{
			IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
			if (OSS)
			{
				IOnlineStoreV2Ptr Store = OSS->GetStoreV2Interface();
				if (Store.IsValid())
				{
					TArray<FOnlineStoreOfferRef> Offers;
					Store->GetOffers(Offers);

					for (const FOnlineStoreOfferRef& Offer : Offers)
					{
						FPHCIAPProduct* Product = FindProduct(Offer->OfferId);
						if (Product)
						{
							Product->DisplayName = Offer->Title.ToString();
							Product->Description = Offer->Description.ToString();
							Product->PriceString = Offer->GetDisplayPrice().ToString();
							Product->Price = static_cast<float>(Offer->NumericPrice) / 100.0f;
							Product->CurrencyCode = Offer->CurrencyCode;
							Product->bIsAvailable = true;

							UE_LOG(LogPlayhouseCity, Log,
								TEXT("PHCStoreManager::QueryProducts -- Product '%s': %s (%d gems)"),
								*Product->ProductId, *Product->PriceString, Product->GemAmount);
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCStoreManager::QueryProducts -- Query failed: %s"), *ErrorMsg);
		}

		OnQueryComplete(bWasSuccessful);
	});

	// Construct filter for query
	FOnlineStoreFilter Filter;
	IOnlineIdentityPtr IdentityForQuery = OnlineSub->GetIdentityInterface();
	FUniqueNetIdPtr QueryUserId = IdentityForQuery.IsValid() ? IdentityForQuery->GetUniquePlayerId(0) : nullptr;
	if (QueryUserId.IsValid())
	{
		StoreInterface->QueryOffersById(*QueryUserId, OfferIds, QueryDelegate);
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCStoreManager::QueryProducts -- Querying %d products from App Store."), OfferIds.Num());
}

FString UPHCStoreManager::GetProductPrice(const FString& ProductId) const
{
	const FPHCIAPProduct* Product = FindProduct(ProductId);
	if (Product && Product->bIsAvailable)
	{
		return Product->PriceString;
	}
	return TEXT("N/A");
}

// ---------------------------------------------------------------------------
// Purchase Flow
// ---------------------------------------------------------------------------

void UPHCStoreManager::PurchaseProduct(const FString& ProductId)
{
	if (bPurchaseInProgress)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCStoreManager::PurchaseProduct -- A purchase is already in progress for '%s'."),
			*CurrentPurchaseProductId);
		return;
	}

	const FPHCIAPProduct* Product = FindProduct(ProductId);
	if (!Product)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCStoreManager::PurchaseProduct -- Unknown product ID: '%s'."), *ProductId);
		OnPurchaseComplete.Broadcast(false, ProductId);
		return;
	}

	if (!Product->bIsAvailable)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCStoreManager::PurchaseProduct -- Product '%s' is not available. Call QueryProducts() first."),
			*ProductId);
		OnPurchaseComplete.Broadcast(false, ProductId);
		return;
	}

	bPurchaseInProgress = true;
	CurrentPurchaseProductId = ProductId;

	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCStoreManager::PurchaseProduct -- Online Subsystem not available."));
		bPurchaseInProgress = false;
		OnPurchaseComplete.Broadcast(false, ProductId);
		return;
	}

	IOnlinePurchasePtr PurchaseInterface = OnlineSub->GetPurchaseInterface();
	if (!PurchaseInterface.IsValid())
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCStoreManager::PurchaseProduct -- Purchase interface not available."));
		bPurchaseInProgress = false;
		OnPurchaseComplete.Broadcast(false, ProductId);
		return;
	}

	// Create the purchase checkout request
	FPurchaseCheckoutRequest CheckoutRequest;
	CheckoutRequest.PurchaseOffers.Emplace(TEXT(""), ProductId, 1, true);

	FOnPurchaseCheckoutComplete PurchaseDelegate;
	PurchaseDelegate.BindLambda([this, ProductId](const FOnlineError& Result, const TSharedRef<FPurchaseReceipt>& Receipt)
	{
		if (Result.WasSuccessful())
		{
			// Extract receipt data for server validation
			FString ReceiptData;
			if (Receipt->ReceiptOffers.Num() > 0)
			{
				const FPurchaseReceipt::FReceiptOfferEntry& ReceiptOffer = Receipt->ReceiptOffers[0];
				if (ReceiptOffer.LineItems.Num() > 0)
				{
					ReceiptData = ReceiptOffer.LineItems[0].ValidationInfo;
				}
			}

			OnPurchaseComplete_Internal(ProductId, ReceiptData, true);
		}
		else
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCStoreManager::PurchaseProduct -- Purchase failed for '%s': %s"),
				*ProductId, *Result.GetErrorMessage().ToString());

			OnPurchaseComplete_Internal(ProductId, FString(), false);
		}
	});

	IOnlineIdentityPtr IdentityForPurchase = OnlineSub->GetIdentityInterface();
	FUniqueNetIdPtr PurchaseUserId = IdentityForPurchase.IsValid() ? IdentityForPurchase->GetUniquePlayerId(0) : nullptr;
	if (PurchaseUserId.IsValid())
	{
		PurchaseInterface->Checkout(*PurchaseUserId, CheckoutRequest, PurchaseDelegate);
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCStoreManager::PurchaseProduct -- Initiating purchase for '%s'."), *ProductId);
}

void UPHCStoreManager::RestorePurchases()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (!OnlineSub)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCStoreManager::RestorePurchases -- Online Subsystem not available."));
		return;
	}

	IOnlinePurchasePtr PurchaseInterface = OnlineSub->GetPurchaseInterface();
	if (!PurchaseInterface.IsValid())
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCStoreManager::RestorePurchases -- Purchase interface not available."));
		return;
	}

	FOnQueryReceiptsComplete RestoreDelegate;
	RestoreDelegate.BindLambda([this](const FOnlineError& Result)
	{
		if (Result.WasSuccessful())
		{
			UE_LOG(LogPlayhouseCity, Log,
				TEXT("PHCStoreManager::RestorePurchases -- Restore completed successfully."));

			// Query receipts to process any restored purchases
			IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
			if (OSS)
			{
				IOnlinePurchasePtr Purchase = OSS->GetPurchaseInterface();
				if (Purchase.IsValid())
				{
					TArray<FPurchaseReceipt> Receipts;
					IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
				FUniqueNetIdPtr LocalUserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
				if (LocalUserId.IsValid())
				{
					Purchase->GetReceipts(*LocalUserId, Receipts);
				}

					for (const FPurchaseReceipt& Receipt : Receipts)
					{
						for (const FPurchaseReceipt::FReceiptOfferEntry& Offer : Receipt.ReceiptOffers)
						{
							// Validate each restored receipt with the server
							FString ValidationData;
							if (Offer.LineItems.Num() > 0)
							{
								ValidationData = Offer.LineItems[0].ValidationInfo;
							}

							if (!ValidationData.IsEmpty())
							{
								ValidateReceiptWithServer(Offer.OfferId, ValidationData);
							}
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCStoreManager::RestorePurchases -- Restore failed: %s"),
				*Result.GetErrorMessage().ToString());
		}
	});

	IOnlineIdentityPtr Identity = OnlineSub->GetIdentityInterface();
	FUniqueNetIdPtr LocalUserId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	if (LocalUserId.IsValid())
	{
		PurchaseInterface->QueryReceipts(*LocalUserId, true, RestoreDelegate);
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCStoreManager::RestorePurchases -- Initiating purchase restore."));
}

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------

void UPHCStoreManager::SetupProductDefinitions()
{
	ProductIds.Empty();
	CachedProducts.Empty();
	ProductGemAmounts.Empty();

	// Define the four gem packs
	struct FProductDef
	{
		FString Id;
		FString Name;
		FString Desc;
		int32   Gems;
	};

	const TArray<FProductDef> Definitions = {
		{ PHCStoreConstants::GemPack100,  TEXT("100 Gems"),  TEXT("A small pouch of gems."),        100  },
		{ PHCStoreConstants::GemPack500,  TEXT("500 Gems"),  TEXT("A bag of gems - great value!"),  500  },
		{ PHCStoreConstants::GemPack1200, TEXT("1200 Gems"), TEXT("A chest of gems - best value!"), 1200 },
		{ PHCStoreConstants::GemPack5000, TEXT("5000 Gems"), TEXT("A vault of gems - ultimate!"),   5000 },
	};

	for (const FProductDef& Def : Definitions)
	{
		FPHCIAPProduct Product;
		Product.ProductId   = Def.Id;
		Product.DisplayName = Def.Name;
		Product.Description = Def.Desc;
		Product.GemAmount   = Def.Gems;
		Product.bIsAvailable = false;

		ProductIds.Add(Def.Id);
		CachedProducts.Add(Product);
		ProductGemAmounts.Add(Def.Id, Def.Gems);
	}
}

void UPHCStoreManager::OnQueryComplete(bool bSuccess)
{
	bProductsQueried = bSuccess;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCStoreManager::OnQueryComplete -- Query %s. %d products available."),
		bSuccess ? TEXT("succeeded") : TEXT("failed"),
		CachedProducts.Num());

	OnProductsQueried.Broadcast(bSuccess);
}

void UPHCStoreManager::OnPurchaseComplete_Internal(const FString& ProductId, const FString& ReceiptData, bool bSuccess)
{
	bPurchaseInProgress = false;
	CurrentPurchaseProductId.Empty();

	if (!bSuccess)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCStoreManager::OnPurchaseComplete_Internal -- Purchase failed for '%s'."), *ProductId);
		OnPurchaseComplete.Broadcast(false, ProductId);
		return;
	}

	if (ReceiptData.IsEmpty())
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCStoreManager::OnPurchaseComplete_Internal -- No receipt data for '%s'."), *ProductId);
		OnPurchaseComplete.Broadcast(false, ProductId);
		return;
	}

	// Validate the receipt with the Django backend before crediting gems
	ValidateReceiptWithServer(ProductId, ReceiptData);
}

void UPHCStoreManager::ValidateReceiptWithServer(const FString& ProductId, const FString& ReceiptData)
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCStoreManager::ValidateReceiptWithServer -- HttpClient is null."));
		OnPurchaseComplete.Broadcast(false, ProductId);
		return;
	}

	// Build the validation request body
	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	BodyObj->SetStringField(TEXT("product_id"), ProductId);
	BodyObj->SetStringField(TEXT("receipt_data"), ReceiptData);
	BodyObj->SetStringField(TEXT("platform"), TEXT("ios"));

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(BodyObj, Writer);

	const FString CapturedProductId = ProductId;
	const int32 GemAmount = GetGemAmountForProduct(ProductId);

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this, CapturedProductId, GemAmount](bool bSuccess, const FString& ResponseBody)
	{
		if (!bSuccess)
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCStoreManager::ValidateReceiptWithServer -- Server validation request failed for '%s'."),
				*CapturedProductId);
			OnPurchaseComplete.Broadcast(false, CapturedProductId);
			return;
		}

		TSharedPtr<FJsonObject> JsonResponse;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseBody);

		if (!FJsonSerializer::Deserialize(JsonReader, JsonResponse) || !JsonResponse.IsValid())
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCStoreManager::ValidateReceiptWithServer -- Failed to parse validation response."));
			OnPurchaseComplete.Broadcast(false, CapturedProductId);
			return;
		}

		const bool bValid = JsonResponse->GetBoolField(TEXT("valid"));
		if (bValid)
		{
			UE_LOG(LogPlayhouseCity, Log,
				TEXT("PHCStoreManager::ValidateReceiptWithServer -- Receipt validated. Crediting %d gems for '%s'."),
				GemAmount, *CapturedProductId);

			// Credit gems through the economy manager
			if (EconomyManager)
			{
				EconomyManager->EarnGems(GemAmount, FString::Printf(TEXT("iap_%s"), *CapturedProductId));
			}

			OnPurchaseComplete.Broadcast(true, CapturedProductId);
		}
		else
		{
			const FString ErrorMsg = JsonResponse->HasField(TEXT("error"))
				? JsonResponse->GetStringField(TEXT("error"))
				: TEXT("Receipt validation failed");

			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCStoreManager::ValidateReceiptWithServer -- Validation rejected for '%s': %s"),
				*CapturedProductId, *ErrorMsg);

			OnPurchaseComplete.Broadcast(false, CapturedProductId);
		}
	});

	HttpClient->Post(TEXT("/api/economy/validate-receipt/"), BodyString, ResponseDelegate);

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCStoreManager::ValidateReceiptWithServer -- Sending receipt for '%s' to server."), *ProductId);
}

FPHCIAPProduct* UPHCStoreManager::FindProduct(const FString& ProductId)
{
	for (FPHCIAPProduct& Product : CachedProducts)
	{
		if (Product.ProductId == ProductId)
		{
			return &Product;
		}
	}
	return nullptr;
}

const FPHCIAPProduct* UPHCStoreManager::FindProduct(const FString& ProductId) const
{
	for (const FPHCIAPProduct& Product : CachedProducts)
	{
		if (Product.ProductId == ProductId)
		{
			return &Product;
		}
	}
	return nullptr;
}

int32 UPHCStoreManager::GetGemAmountForProduct(const FString& ProductId) const
{
	const int32* Amount = ProductGemAmounts.Find(ProductId);
	return Amount ? *Amount : 0;
}
