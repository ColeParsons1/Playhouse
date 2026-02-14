// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Shop/PHCShopItemCard.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "Engine/Texture2D.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCShopItemCard::UPHCShopItemCard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCShopItemCard::NativeConstruct()
{
	Super::NativeConstruct();

	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.AddDynamic(this, &UPHCShopItemCard::HandlePurchaseClicked);
	}

	// Apply initial state.
	if (OwnedBadge)
	{
		OwnedBadge->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPHCShopItemCard::NativeDestruct()
{
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void UPHCShopItemCard::SetItemData(const FPHCShopItem& Item)
{
	ItemData = Item;

	if (ItemNameText)
	{
		ItemNameText->SetText(FText::FromString(ItemData.Name));
	}

	if (PriceText)
	{
		PriceText->SetText(FText::AsNumber(ItemData.Price));
	}

	UpdateCurrencyIcon();
	SetOwned(ItemData.bOwned);

	// Async-load the item thumbnail.
	if (!ItemData.ImageUrl.IsEmpty())
	{
		LoadItemImage();
	}
}

void UPHCShopItemCard::SetOwned(bool bOwned)
{
	ItemData.bOwned = bOwned;

	if (OwnedBadge)
	{
		OwnedBadge->SetVisibility(bOwned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (PurchaseButton)
	{
		PurchaseButton->SetIsEnabled(!bOwned);
	}

	// Dim the card border if already owned.
	if (ItemBorder)
	{
		const FLinearColor BorderColor = bOwned
			? FLinearColor(0.3f, 0.3f, 0.3f, 0.6f)
			: FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);
		ItemBorder->SetBrushColor(BorderColor);
	}
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UPHCShopItemCard::HandlePurchaseClicked()
{
	if (ItemData.bOwned)
	{
		return;
	}

	OnPurchaseClicked.ExecuteIfBound(ItemData.ItemId, ItemData.CurrencyType, ItemData.Price);
}

void UPHCShopItemCard::LoadItemImage()
{
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();

	Request->SetURL(ItemData.ImageUrl);
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindUObject(this, &UPHCShopItemCard::OnImageDownloadComplete);
	Request->ProcessRequest();
}

void UPHCShopItemCard::OnImageDownloadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCShopItemCard: Failed to download image for item '%s'."), *ItemData.Name);
		return;
	}

	const TArray<uint8>& ImageData = Response->GetContent();
	if (ImageData.Num() == 0)
	{
		return;
	}

	// Detect image format and decode.
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	EImageFormat DetectedFormat = EImageFormat::PNG;
	if (ItemData.ImageUrl.EndsWith(TEXT(".jpg")) || ItemData.ImageUrl.EndsWith(TEXT(".jpeg")))
	{
		DetectedFormat = EImageFormat::JPEG;
	}

	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(DetectedFormat);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCShopItemCard: Failed to decompress image for item '%s'."), *ItemData.Name);
		return;
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		return;
	}

	const int32 Width = ImageWrapper->GetWidth();
	const int32 Height = ImageWrapper->GetHeight();

	// Create a transient UTexture2D and upload the raw pixel data.
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		return;
	}

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	if (ItemImage)
	{
		ItemImage->SetBrushFromTexture(Texture);
	}
}

void UPHCShopItemCard::UpdateCurrencyIcon()
{
	if (!CurrencyIcon)
	{
		return;
	}

	// Tint the currency icon to differentiate coins (gold) from gems (blue).
	const FLinearColor IconColor = (ItemData.CurrencyType == EPHCCurrencyType::Coins)
		? FLinearColor(1.0f, 0.84f, 0.0f, 1.0f)    // Gold for coins
		: FLinearColor(0.3f, 0.5f, 1.0f, 1.0f);     // Blue for gems

	CurrencyIcon->SetColorAndOpacity(IconColor);
}
