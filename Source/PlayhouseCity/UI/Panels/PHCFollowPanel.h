// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Core/PHCTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PHCFollowPanel.generated.h"

class UListView;
class UButton;
class UTextBlock;
class UWidgetSwitcher;
class UScrollBox;
class UPHCMainHUD;

// FPHCFollowInfo is defined in Core/PHCTypes.h.

/**
 * UObject wrapper for FPHCFollowInfo so it can be used with UListView.
 */
UCLASS(BlueprintType)
class PLAYHOUSECITY_API UPHCFollowListItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "PlayhouseCity|Social")
	FPHCFollowInfo FollowInfo;

	static UPHCFollowListItem* Create(UObject* Outer, const FPHCFollowInfo& InInfo)
	{
		UPHCFollowListItem* Item = NewObject<UPHCFollowListItem>(Outer);
		Item->FollowInfo = InInfo;
		return Item;
	}
};

/**
 * UPHCFollowListEntryWidget
 *
 * Widget representing a single row in the followers/following list.
 * Implements IUserObjectListEntry so UListView can drive it.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCFollowListEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	/** Delegate fired when the follow/unfollow button on this entry is clicked. */
	DECLARE_DELEGATE_TwoParams(FOnFollowToggleEntry, int32 /*PlayerId*/, bool /*bFollow*/);
	FOnFollowToggleEntry OnFollowToggleEntry;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UsernameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OnlineStatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FollowToggleButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FollowToggleLabel;

	UPROPERTY(BlueprintReadOnly, Category = "PlayhouseCity|Social")
	TObjectPtr<UPHCFollowListItem> CachedItem;

private:
	UFUNCTION()
	void OnFollowToggleClicked();
};

/**
 * Which sub-tab is active: Followers or Following.
 */
UENUM(BlueprintType)
enum class EPHCFollowTab : uint8
{
	Followers = 0,
	Following = 1
};

/**
 * UPHCFollowPanel
 *
 * Social panel showing the player's followers and following lists.
 * Has two sub-tabs toggled by header buttons.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCFollowPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCFollowPanel(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

	// ---------------------------------------------------------------
	//  Public API
	// ---------------------------------------------------------------

	/** Fetch and display the followers list. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI|Follow")
	void ShowFollowers();

	/** Fetch and display the following list. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI|Follow")
	void ShowFollowing();

	/** Follow or unfollow a player by ID. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI|Follow")
	void OnFollowToggle(int32 PlayerId, bool bFollow);

protected:
	// ---------------------------------------------------------------
	//  Bound Widgets
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FollowersButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FollowingButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UListView> PlayerListView;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	/** Optional: a small widget switcher or visual indicator for which sub-tab is active. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> TabIndicator;

private:
	// ---------------------------------------------------------------
	//  Callbacks
	// ---------------------------------------------------------------

	UFUNCTION()
	void OnFollowersButtonClicked();

	UFUNCTION()
	void OnFollowingButtonClicked();

	UFUNCTION()
	void OnPlayerClicked(UObject* Item);

	// ---------------------------------------------------------------
	//  HTTP handlers
	// ---------------------------------------------------------------

	void OnFollowersReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnFollowingReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnFollowToggleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	// ---------------------------------------------------------------
	//  Internal helpers
	// ---------------------------------------------------------------

	/** Parses JSON response into follow info array. */
	TArray<FPHCFollowInfo> ParseFollowList(const FString& JsonString) const;

	/** Populates the list view from a follow info array. */
	void OnFollowDataReceived(const TArray<FPHCFollowInfo>& Players);

	/** Updates tab button styling to reflect which sub-tab is active. */
	void UpdateTabStyles();

	/** Updates the empty state text visibility. */
	void UpdateEmptyState();

	/** Attempts to find the owning MainHUD. */
	UPHCMainHUD* GetMainHUD() const;

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	EPHCFollowTab ActiveTab = EPHCFollowTab::Followers;

	UPROPERTY()
	TArray<TObjectPtr<UPHCFollowListItem>> DisplayedItems;

	bool bIsLoading = false;

	/** Fallback scroll box when no Blueprint ListView is available. */
	UPROPERTY()
	TObjectPtr<UScrollBox> FollowScrollBox;

	/** Populates the fallback scroll box with follow data. */
	void PopulateFollowScrollBox(const TArray<FPHCFollowInfo>& Players);
};
