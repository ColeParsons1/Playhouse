// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PHCShopItemCard.generated.h"

class UImage;
class UTextBlock;
class UButton;
class UBorder;
class UWidget;

/**
 * Native delegate fired when the purchase button is clicked on a card.
 * Parameters: ItemId, CurrencyType, Price
 */
DECLARE_DELEGATE_ThreeParams(FOnShopCardPurchaseClicked, int32 /*ItemId*/, EPHCCurrencyType /*Currency*/, int32 /*Price*/);

/**
 * UPHCShopItemCard
 *
 * Represents a single purchasable item in the shop grid. Displays the item
 * image, name, price with currency icon, and an "Owned" overlay badge when
 * the player already owns the item.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCShopItemCard : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCShopItemCard(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// -- Public API ------------------------------------------------------------

	/** Populate the card with item data. Call after creation, before adding to parent. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Shop")
	void SetItemData(const FPHCShopItem& Item);

	/** Mark this item as owned or not. Toggles the owned badge and disables purchase. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Shop")
	void SetOwned(bool bOwned);

	/** Native delegate the parent shop widget binds to receive purchase requests. */
	FOnShopCardPurchaseClicked OnPurchaseClicked;

protected:
	// -- Bind Widgets ----------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> ItemImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CurrencyIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> PurchaseButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> OwnedBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> ItemBorder;

	// -- Data ------------------------------------------------------------------

	/** The shop item this card represents. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Shop")
	FPHCShopItem ItemData;

private:
	// -- Internal helpers ------------------------------------------------------

	/** Handle the purchase button click. */
	UFUNCTION()
	void HandlePurchaseClicked();

	/** Download the item image asynchronously from ItemData.ImageUrl. */
	void LoadItemImage();

	/** Callback when the HTTP image download completes. */
	void OnImageDownloadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Apply the coin or gem icon to the CurrencyIcon image. */
	void UpdateCurrencyIcon();
};
