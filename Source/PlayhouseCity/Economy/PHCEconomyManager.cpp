// Copyright PlayhouseCity. All Rights Reserved.

#include "Economy/PHCEconomyManager.h"
#include "Network/PHCHttpClient.h"
#include "PlayhouseCity.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCEconomyManager::UPHCEconomyManager()
	: CachedCoins(0)
	, CachedGems(0)
	, HttpClient(nullptr)
{
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void UPHCEconomyManager::Initialize(UPHCHttpClient* InHttpClient)
{
	HttpClient = InHttpClient;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCEconomyManager::Initialize -- Economy manager initialized."));
}

// ---------------------------------------------------------------------------
// Balance Queries
// ---------------------------------------------------------------------------

void UPHCEconomyManager::RefreshBalance()
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCEconomyManager::RefreshBalance -- HttpClient is null."));
		return;
	}

	FPHCHttpResponse Callback;
	Callback.BindUObject(this, &UPHCEconomyManager::OnBalanceResponse);

	HttpClient->Get(TEXT("/api/economy/balance/"), Callback, /*CacheTTL=*/ 0.f);

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCEconomyManager::RefreshBalance -- Requesting balance from server."));
}

bool UPHCEconomyManager::CanAfford(EPHCCurrencyType Currency, int32 Amount) const
{
	if (Amount <= 0)
	{
		return true;
	}

	switch (Currency)
	{
	case EPHCCurrencyType::Coins:
		return CachedCoins >= Amount;
	case EPHCCurrencyType::Gems:
		return CachedGems >= Amount;
	default:
		return false;
	}
}

// ---------------------------------------------------------------------------
// Spending (server-authoritative)
// ---------------------------------------------------------------------------

void UPHCEconomyManager::SpendCoins(int32 Amount, const FString& Reason, TFunction<void(bool)> Callback)
{
	if (Amount <= 0)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::SpendCoins -- Invalid amount: %d."), Amount);
		if (Callback)
		{
			Callback(false);
		}
		return;
	}

	if (!CanAfford(EPHCCurrencyType::Coins, Amount))
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::SpendCoins -- Insufficient coins. Have %d, need %d."),
			CachedCoins, Amount);
		if (Callback)
		{
			Callback(false);
		}
		return;
	}

	SpendCurrency(TEXT("coins"), Amount, Reason, MoveTemp(Callback));
}

void UPHCEconomyManager::SpendGems(int32 Amount, const FString& Reason, TFunction<void(bool)> Callback)
{
	if (Amount <= 0)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::SpendGems -- Invalid amount: %d."), Amount);
		if (Callback)
		{
			Callback(false);
		}
		return;
	}

	if (!CanAfford(EPHCCurrencyType::Gems, Amount))
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::SpendGems -- Insufficient gems. Have %d, need %d."),
			CachedGems, Amount);
		if (Callback)
		{
			Callback(false);
		}
		return;
	}

	SpendCurrency(TEXT("gems"), Amount, Reason, MoveTemp(Callback));
}

// ---------------------------------------------------------------------------
// Earning (server-confirmed)
// ---------------------------------------------------------------------------

void UPHCEconomyManager::EarnCoins(int32 Amount, const FString& Source)
{
	if (Amount <= 0)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::EarnCoins -- Invalid amount: %d."), Amount);
		return;
	}

	CachedCoins += Amount;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCEconomyManager::EarnCoins -- Earned %d coins from '%s'. New balance: %d coins."),
		Amount, *Source, CachedCoins);

	OnBalanceChanged.Broadcast(CachedCoins, CachedGems);
}

void UPHCEconomyManager::EarnGems(int32 Amount, const FString& Source)
{
	if (Amount <= 0)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::EarnGems -- Invalid amount: %d."), Amount);
		return;
	}

	CachedGems += Amount;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCEconomyManager::EarnGems -- Earned %d gems from '%s'. New balance: %d gems."),
		Amount, *Source, CachedGems);

	OnBalanceChanged.Broadcast(CachedCoins, CachedGems);
}

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------

void UPHCEconomyManager::OnBalanceResponse(bool bSuccess, const FString& Response)
{
	if (!bSuccess)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::OnBalanceResponse -- Failed to fetch balance."));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCEconomyManager::OnBalanceResponse -- Failed to parse balance JSON."));
		return;
	}

	const int32 PreviousCoins = CachedCoins;
	const int32 PreviousGems = CachedGems;

	CachedCoins = static_cast<int32>(JsonObject->GetNumberField(TEXT("coins")));
	CachedGems = static_cast<int32>(JsonObject->GetNumberField(TEXT("gems")));

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCEconomyManager::OnBalanceResponse -- Balance updated: %d coins, %d gems."),
		CachedCoins, CachedGems);

	// Only broadcast if something actually changed
	if (CachedCoins != PreviousCoins || CachedGems != PreviousGems)
	{
		OnBalanceChanged.Broadcast(CachedCoins, CachedGems);
	}
}

void UPHCEconomyManager::SpendCurrency(const FString& CurrencyName, int32 Amount, const FString& Reason, TFunction<void(bool)> Callback)
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCEconomyManager::SpendCurrency -- HttpClient is null."));
		if (Callback)
		{
			Callback(false);
		}
		return;
	}

	// Build the JSON body
	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	BodyObj->SetNumberField(TEXT("amount"), Amount);
	BodyObj->SetStringField(TEXT("reason"), Reason);
	BodyObj->SetStringField(TEXT("currency"), CurrencyName);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(BodyObj, Writer);

	const FString Endpoint = FString::Printf(TEXT("/api/economy/spend/"));

	// Capture the callback and currency info for the response handler
	TFunction<void(bool)> CapturedCallback = MoveTemp(Callback);
	const FString CapturedCurrency = CurrencyName;
	const int32 CapturedAmount = Amount;

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this, CapturedCallback, CapturedCurrency, CapturedAmount](bool bSuccess, const FString& ResponseBody)
	{
		bool bTransactionSuccess = false;

		if (bSuccess)
		{
			TSharedPtr<FJsonObject> JsonResponse;
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseBody);

			if (FJsonSerializer::Deserialize(JsonReader, JsonResponse) && JsonResponse.IsValid())
			{
				bTransactionSuccess = JsonResponse->GetBoolField(TEXT("success"));

				if (bTransactionSuccess)
				{
					// Update local cache from server response
					if (JsonResponse->HasField(TEXT("coins")))
					{
						CachedCoins = static_cast<int32>(JsonResponse->GetNumberField(TEXT("coins")));
					}
					if (JsonResponse->HasField(TEXT("gems")))
					{
						CachedGems = static_cast<int32>(JsonResponse->GetNumberField(TEXT("gems")));
					}

					UE_LOG(LogPlayhouseCity, Log,
						TEXT("PHCEconomyManager::SpendCurrency -- Spent %d %s. Remaining: %d coins, %d gems."),
						CapturedAmount, *CapturedCurrency, CachedCoins, CachedGems);

					OnBalanceChanged.Broadcast(CachedCoins, CachedGems);
				}
				else
				{
					const FString ErrorMsg = JsonResponse->HasField(TEXT("error"))
						? JsonResponse->GetStringField(TEXT("error"))
						: TEXT("Unknown error");
					UE_LOG(LogPlayhouseCity, Warning,
						TEXT("PHCEconomyManager::SpendCurrency -- Server rejected spend of %d %s: %s"),
						CapturedAmount, *CapturedCurrency, *ErrorMsg);
				}
			}
			else
			{
				UE_LOG(LogPlayhouseCity, Warning,
					TEXT("PHCEconomyManager::SpendCurrency -- Failed to parse spend response."));
			}
		}
		else
		{
			UE_LOG(LogPlayhouseCity, Warning,
				TEXT("PHCEconomyManager::SpendCurrency -- HTTP request failed for spending %d %s."),
				CapturedAmount, *CapturedCurrency);
		}

		if (CapturedCallback)
		{
			CapturedCallback(bTransactionSuccess);
		}
	});

	HttpClient->Post(Endpoint, BodyString, ResponseDelegate);
}
