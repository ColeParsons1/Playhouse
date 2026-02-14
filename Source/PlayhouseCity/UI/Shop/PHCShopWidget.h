// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCShopWidget.generated.h"

class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UWidget;
class UButton;
class UPHCGameInstance;
class UPHCHttpClient;
class UPHCShopItemCard;

/**
 * UPHCShopWidget
 *
 * The in-game shop screen. Displays purchasable items organized by category
 * (All, Avatar, Room, Game, Special). Supports coin and gem purchases,
 * balance display, loading states, and empty-state messaging.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCShopWidget(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	// -- Bind Widgets ----------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> CategoryTabs;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> ItemGrid;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CoinsDisplay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> GemsDisplay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> LoadingIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyStateText;

	// -- Configuration ---------------------------------------------------------

	/** Widget class used to spawn individual shop-item cards. Must be a
	 *  UPHCShopItemCard subclass (set in Blueprint or DefaultSubobjectClass). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Shop")
	TSubclassOf<UPHCShopItemCard> ShopItemCardClass;

	// -- Methods ---------------------------------------------------------------

	/** Switch the displayed category; reloads the item grid. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Shop")
	void OnCategoryChanged(EPHCItemCategory Category);

	/** Fetch shop items for the given category from the backend. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Shop")
	void LoadItems(EPHCItemCategory Category);

	/** Called when a child shop-item card requests a purchase. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Shop")
	void OnPurchaseRequested(int32 ItemId, EPHCCurrencyType Currency, int32 Price);

	/** Update the coin / gem balance display from GameInstance. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Shop")
	void RefreshBalance();

private:
	// -- Internal helpers ------------------------------------------------------

	/** Build category tab buttons in the CategoryTabs box. */
	void BuildCategoryTabs();

	/** Show or hide the loading spinner. */
	void SetLoading(bool bLoading);

	/** Callback after GetShopItems API returns. */
	UFUNCTION()
	void OnItemsReceived(bool bSuccess, int32 StatusCode, const FString& Response);

	/** Callback after PurchaseItem API returns. */
	UFUNCTION()
	void OnPurchaseResult(bool bSuccess, int32 StatusCode, const FString& Response);

	/** Convenience accessor for the HttpClient via GameInstance. */
	UPHCHttpClient* GetHttpClient() const;

	/** Currently selected category filter. */
	EPHCItemCategory ActiveCategory;

	/** Cached items for the current category. */
	UPROPERTY()
	TArray<FPHCShopItem> CachedItems;

	/** Item ID of the purchase currently in-flight. */
	int32 PendingPurchaseItemId = 0;

	/** True while a purchase request is active. */
	bool bPurchaseInFlight = false;

	/** Maps category enum to a display-name string. */
	static FString CategoryToApiString(EPHCItemCategory Category);

	/** All tab categories including the synthetic "All" category. */
	static const TArray<FString> TabNames;
};
