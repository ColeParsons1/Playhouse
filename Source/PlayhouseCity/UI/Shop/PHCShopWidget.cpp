// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Shop/PHCShopWidget.h"
#include "UI/Shop/PHCShopItemCard.h"

#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ---------------------------------------------------------------------------
// Static data
// ---------------------------------------------------------------------------

const TArray<FString> UPHCShopWidget::TabNames = {
	TEXT("All"),
	TEXT("Avatar"),
	TEXT("Room"),
	TEXT("Game"),
	TEXT("Special")
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCShopWidget::UPHCShopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ActiveCategory(EPHCItemCategory::Avatar) // default - "All" maps to Avatar initially
{
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildCategoryTabs();
	RefreshBalance();

	// Load default category ("All" represented by Avatar as first pass).
	LoadItems(ActiveCategory);
}

void UPHCShopWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public / Protected methods
// ---------------------------------------------------------------------------

void UPHCShopWidget::OnCategoryChanged(EPHCItemCategory Category)
{
	ActiveCategory = Category;
	LoadItems(Category);
}

void UPHCShopWidget::LoadItems(EPHCItemCategory Category)
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCShop: HttpClient unavailable."));
		return;
	}

	SetLoading(true);

	if (ItemGrid)
	{
		ItemGrid->ClearChildren();
	}
	if (EmptyStateText)
	{
		EmptyStateText->SetVisibility(ESlateVisibility::Collapsed);
	}

	const FString CategoryStr = CategoryToApiString(Category);

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCShopWidget::OnItemsReceived);
	Http->GetShopItems(CategoryStr, Callback);
}

void UPHCShopWidget::OnPurchaseRequested(int32 ItemId, EPHCCurrencyType Currency, int32 Price)
{
	if (bPurchaseInFlight)
	{
		return;
	}

	// Validate the player can afford the item.
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		const int32 Balance = (Currency == EPHCCurrencyType::Coins)
			? GI->LocalPlayerProfile.Coins
			: GI->LocalPlayerProfile.Gems;

		if (Balance < Price)
		{
			UE_LOG(LogTemp, Warning, TEXT("PHCShop: Insufficient balance for item %d. Need %d, have %d."),
				ItemId, Price, Balance);
			return;
		}
	}

	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		return;
	}

	bPurchaseInFlight = true;
	PendingPurchaseItemId = ItemId;

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCShopWidget::OnPurchaseResult);
	Http->PurchaseItem(ItemId, Callback);
}

void UPHCShopWidget::RefreshBalance()
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (!GI)
	{
		return;
	}

	if (CoinsDisplay)
	{
		CoinsDisplay->SetText(FText::AsNumber(GI->LocalPlayerProfile.Coins));
	}
	if (GemsDisplay)
	{
		GemsDisplay->SetText(FText::AsNumber(GI->LocalPlayerProfile.Gems));
	}
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UPHCShopWidget::BuildCategoryTabs()
{
	if (!CategoryTabs)
	{
		return;
	}

	CategoryTabs->ClearChildren();

	for (const FString& TabName : TabNames)
	{
		UButton* TabBtn = NewObject<UButton>(this);
		if (!TabBtn)
		{
			continue;
		}

		UTextBlock* Label = NewObject<UTextBlock>(TabBtn);
		if (Label)
		{
			Label->SetText(FText::FromString(TabName));
			FSlateFontInfo FontInfo = Label->GetFont();
			FontInfo.Size = 14;
			Label->SetFont(FontInfo);
			TabBtn->AddChild(Label);
		}

		CategoryTabs->AddChild(TabBtn);
	}
}

void UPHCShopWidget::SetLoading(bool bLoading)
{
	if (LoadingIndicator)
	{
		LoadingIndicator->SetVisibility(bLoading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UPHCShopWidget::OnItemsReceived(bool bSuccess, int32 StatusCode, const FString& Response)
{
	SetLoading(false);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCShop: Failed to load items. Status=%d"), StatusCode);
		if (EmptyStateText)
		{
			EmptyStateText->SetText(FText::FromString(TEXT("Failed to load shop items.")));
			EmptyStateText->SetVisibility(ESlateVisibility::Visible);
		}
		return;
	}

	// Parse the JSON array of items.
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
	if (!FJsonSerializer::Deserialize(Reader, JsonArray))
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCShop: Failed to parse shop items JSON."));
		return;
	}

	CachedItems.Empty(JsonArray.Num());

	for (const TSharedPtr<FJsonValue>& Val : JsonArray)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Val->TryGetObject(ObjPtr) || !ObjPtr || !(*ObjPtr).IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FPHCShopItem Item;
		Item.ItemId = static_cast<int32>(Obj->GetNumberField(TEXT("id")));
		Item.Name = Obj->GetStringField(TEXT("name"));
		Item.Description = Obj->GetStringField(TEXT("description"));
		Item.ImageUrl = Obj->GetStringField(TEXT("image_url"));
		Item.Price = static_cast<int32>(Obj->GetNumberField(TEXT("price")));
		Item.bOwned = Obj->GetBoolField(TEXT("owned"));

		// Parse currency type.
		const FString CurrStr = Obj->GetStringField(TEXT("currency"));
		Item.CurrencyType = CurrStr.Equals(TEXT("gems"), ESearchCase::IgnoreCase)
			? EPHCCurrencyType::Gems
			: EPHCCurrencyType::Coins;

		// Parse item category.
		const FString CatStr = Obj->GetStringField(TEXT("category"));
		if (CatStr.Equals(TEXT("room"), ESearchCase::IgnoreCase))
		{
			Item.Category = EPHCItemCategory::Room;
		}
		else if (CatStr.Equals(TEXT("game"), ESearchCase::IgnoreCase))
		{
			Item.Category = EPHCItemCategory::Game;
		}
		else if (CatStr.Equals(TEXT("special"), ESearchCase::IgnoreCase))
		{
			Item.Category = EPHCItemCategory::Special;
		}
		else
		{
			Item.Category = EPHCItemCategory::Avatar;
		}

		CachedItems.Add(Item);
	}

	// Populate the visual grid.
	if (ItemGrid)
	{
		ItemGrid->ClearChildren();
	}

	if (CachedItems.Num() == 0)
	{
		if (EmptyStateText)
		{
			EmptyStateText->SetText(FText::FromString(TEXT("No items available in this category.")));
			EmptyStateText->SetVisibility(ESlateVisibility::Visible);
		}
		return;
	}

	if (!ShopItemCardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCShop: ShopItemCardClass not set. Cannot create item cards."));
		return;
	}

	for (const FPHCShopItem& Item : CachedItems)
	{
		UPHCShopItemCard* Card = CreateWidget<UPHCShopItemCard>(GetOwningPlayer(), ShopItemCardClass);
		if (!Card)
		{
			continue;
		}

		Card->SetItemData(Item);

		// Bind purchase delegate.
		Card->OnPurchaseClicked.BindUObject(this, &UPHCShopWidget::OnPurchaseRequested);

		if (ItemGrid)
		{
			ItemGrid->AddChild(Card);
		}
	}
}

void UPHCShopWidget::OnPurchaseResult(bool bSuccess, int32 StatusCode, const FString& Response)
{
	bPurchaseInFlight = false;

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("PHCShop: Purchase of item %d succeeded."), PendingPurchaseItemId);

		// Mark the purchased item as owned in the cached list.
		for (FPHCShopItem& Item : CachedItems)
		{
			if (Item.ItemId == PendingPurchaseItemId)
			{
				Item.bOwned = true;
				break;
			}
		}

		// Update the balance from the response if available.
		TSharedPtr<FJsonObject> JsonObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
			if (GI)
			{
				if (JsonObj->HasField(TEXT("coins")))
				{
					GI->LocalPlayerProfile.Coins = static_cast<int32>(JsonObj->GetNumberField(TEXT("coins")));
				}
				if (JsonObj->HasField(TEXT("gems")))
				{
					GI->LocalPlayerProfile.Gems = static_cast<int32>(JsonObj->GetNumberField(TEXT("gems")));
				}
			}
		}

		RefreshBalance();

		// Reload items to reflect owned state.
		LoadItems(ActiveCategory);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCShop: Purchase of item %d failed. Status=%d"),
			PendingPurchaseItemId, StatusCode);
	}

	PendingPurchaseItemId = 0;
}

UPHCHttpClient* UPHCShopWidget::GetHttpClient() const
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	return GI ? GI->GetHttpClient() : nullptr;
}

FString UPHCShopWidget::CategoryToApiString(EPHCItemCategory Category)
{
	switch (Category)
	{
		case EPHCItemCategory::Avatar:	return TEXT("avatar");
		case EPHCItemCategory::Room:	return TEXT("room");
		case EPHCItemCategory::Game:	return TEXT("game");
		case EPHCItemCategory::Special:	return TEXT("special");
		default:						return TEXT("all");
	}
}
