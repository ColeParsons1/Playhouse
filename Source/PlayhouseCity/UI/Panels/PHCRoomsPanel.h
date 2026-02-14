// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Core/PHCTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PHCRoomsPanel.generated.h"

class UListView;
class UButton;
class UEditableTextBox;
class UComboBoxString;
class UTextBlock;
class UWidget;
class UScrollBox;
class UPHCMainHUD;

// EPHCRoomStatus and FPHCRoomInfo are defined in Core/PHCTypes.h.

/**
 * UObject wrapper for FPHCRoomInfo so it can be used with UListView.
 */
UCLASS(BlueprintType)
class PLAYHOUSECITY_API UPHCRoomListItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "PlayhouseCity|Rooms")
	FPHCRoomInfo RoomInfo;

	/** Factory helper. */
	static UPHCRoomListItem* Create(UObject* Outer, const FPHCRoomInfo& InInfo)
	{
		UPHCRoomListItem* Item = NewObject<UPHCRoomListItem>(Outer);
		Item->RoomInfo = InInfo;
		return Item;
	}
};

/**
 * UPHCRoomListEntryWidget
 *
 * Widget representing a single row in the room list.
 * Implements IUserObjectListEntry so UListView can drive it.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCRoomListEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	// IUserObjectListEntry
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RoomNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HostNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerCountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusBadge;

	/** Cached reference to the data object for click handling. */
	UPROPERTY(BlueprintReadOnly, Category = "PlayhouseCity|Rooms")
	TObjectPtr<UPHCRoomListItem> CachedItem;
};

// ---------------------------------------------------------------
//  Filter mode for the rooms panel
// ---------------------------------------------------------------

UENUM(BlueprintType)
enum class EPHCRoomFilter : uint8
{
	All = 0			UMETA(DisplayName = "All Rooms"),
	Open			UMETA(DisplayName = "Open"),
	Friends			UMETA(DisplayName = "Friends' Rooms"),

	MAX				UMETA(Hidden)
};

/**
 * UPHCRoomsPanel
 *
 * Scrollable room list panel. Fetches rooms from the backend,
 * supports text search and filter, and allows the player to
 * click into a room.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCRoomsPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCRoomsPanel(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

	// ---------------------------------------------------------------
	//  Public API
	// ---------------------------------------------------------------

	/** Fetches the room list from the server and repopulates the view. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI|Rooms")
	void RefreshRoomList();

protected:
	// ---------------------------------------------------------------
	//  Bound Widgets
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UListView> RoomListView;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SearchInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> FilterCombo;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyStateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LoadingIndicator;

private:
	// ---------------------------------------------------------------
	//  Callbacks
	// ---------------------------------------------------------------

	UFUNCTION()
	void OnRefreshClicked();

	UFUNCTION()
	void OnSearchChanged(const FText& Text);

	UFUNCTION()
	void OnFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnRoomClicked(UObject* Item);

	// ---------------------------------------------------------------
	//  HTTP
	// ---------------------------------------------------------------

	void OnRoomDataReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	// ---------------------------------------------------------------
	//  Internal helpers
	// ---------------------------------------------------------------

	/** Parses a JSON array string into room info structs. */
	TArray<FPHCRoomInfo> ParseRoomList(const FString& JsonString) const;

	/** Applies search text and filter to the full room list and updates the list view. */
	void ApplyFilters();

	/** Sets loading state in the UI. */
	void SetLoading(bool bLoading);

	/** Updates empty state visibility. */
	void UpdateEmptyState();

	/** Attempts to find the owning MainHUD. */
	UPHCMainHUD* GetMainHUD() const;

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** Full unfiltered room list from server. */
	UPROPERTY()
	TArray<FPHCRoomInfo> AllRooms;

	/** Currently displayed (filtered) items. */
	UPROPERTY()
	TArray<TObjectPtr<UPHCRoomListItem>> DisplayedItems;

	/** Current search filter text. */
	FString CurrentSearchText;

	/** Current filter mode. */
	EPHCRoomFilter CurrentFilter = EPHCRoomFilter::All;

	/** True while a network request is in-flight. */
	bool bIsLoading = false;

	/** Fallback scroll box when no Blueprint ListView is available. */
	UPROPERTY()
	TObjectPtr<UScrollBox> RoomScrollBox;

	/** Populates the fallback scroll box with room data. */
	void PopulateRoomScrollBox();
};
