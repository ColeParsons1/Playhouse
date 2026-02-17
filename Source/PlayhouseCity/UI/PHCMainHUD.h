// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCMainHUD.generated.h"

class UWidgetSwitcher;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UUserWidget;
class UPHCCreateRoomPanel;
class UPHCRoomsPanel;
class UPHCFollowPanel;
class UPHCGamesPanel;
class UPHCProfilePanel;

// EPHCGameType is now defined in Core/PHCTypes.h (the canonical types header).

/** Minimal concrete UUserWidget subclass for programmatic overlays. */
UCLASS()
class PLAYHOUSECITY_API UPHCOverlayWidget : public UUserWidget
{
	GENERATED_BODY()
};

/**
 * Delegate broadcast when the active tab changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTabChanged, int32, TabIndex);

/**
 * UPHCMainHUD
 *
 * Root HUD widget for the PlayhouseCity lobby.
 * Contains a bottom navigation bar that switches between five tab panels
 * (Create, Rooms, Follow, Games, Profile) and a top economy bar showing
 * the player's coin / gem balances and username.
 *
 * Designed to be created via CreateWidget<UPHCMainHUD>() and added to
 * the player's viewport. Panel sub-widgets are instantiated at construct
 * time and inserted into the WidgetSwitcher.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCMainHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCMainHUD(const FObjectInitializer& ObjectInitializer);

	// ---------------------------------------------------------------
	//  UUserWidget overrides
	// ---------------------------------------------------------------
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---------------------------------------------------------------
	//  Tab Navigation
	// ---------------------------------------------------------------

	/** Sets the active panel by index (0 = Create, 1 = Rooms, ...). */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void SwitchToTab(int32 TabIndex);

	/** Returns the currently active tab index. */
	UFUNCTION(BlueprintPure, Category = "PlayhouseCity|UI")
	int32 GetActiveTabIndex() const { return ActiveTabIndex; }

	// ---------------------------------------------------------------
	//  Economy HUD
	// ---------------------------------------------------------------

	/** Updates the top-bar economy display. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void UpdateEconomyDisplay(int32 Coins, int32 Gems);

	/** Sets the displayed username. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void SetUsernameDisplay(const FString& Username);

	// ---------------------------------------------------------------
	//  Navigation helpers
	// ---------------------------------------------------------------

	/** Opens the room widget overlay for the given room. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void NavigateToRoom(int32 RoomId);

	/** Transitions to a specific game level / mode. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void NavigateToGame(EPHCGameType GameType);

	/** Adds a modal overlay on top of the HUD. Ownership is taken. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void ShowOverlayWidget(UUserWidget* Widget);

	/** Dismisses the current overlay if one is shown. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void DismissOverlay();

	/** Refreshes data in the currently active panel. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI")
	void RefreshData();

	// ---------------------------------------------------------------
	//  Delegates
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|UI")
	FOnTabChanged OnTabChanged;

protected:
	// ---------------------------------------------------------------
	//  Widgets (created programmatically in NativeConstruct; BindWidgetOptional
	//  kept so a UMG Blueprint can still override them if desired)
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ContentSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RoomsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FollowTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> GamesTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ProfileTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> BottomNavBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinDisplay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GemDisplay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UsernameDisplay;

	// ---------------------------------------------------------------
	//  Panel class references (set in BP or via defaults)
	// ---------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|Panels")
	TSubclassOf<UPHCCreateRoomPanel> CreateRoomPanelClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|Panels")
	TSubclassOf<UPHCRoomsPanel> RoomsPanelClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|Panels")
	TSubclassOf<UPHCFollowPanel> FollowPanelClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|Panels")
	TSubclassOf<UPHCGamesPanel> GamesPanelClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|Panels")
	TSubclassOf<UPHCProfilePanel> ProfilePanelClass;

	// ---------------------------------------------------------------
	//  Runtime panel instances
	// ---------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UPHCCreateRoomPanel> CreateRoomPanel;

	UPROPERTY()
	TObjectPtr<UPHCRoomsPanel> RoomsPanel;

	UPROPERTY()
	TObjectPtr<UPHCFollowPanel> FollowPanel;

	UPROPERTY()
	TObjectPtr<UPHCGamesPanel> GamesPanel;

	UPROPERTY()
	TObjectPtr<UPHCProfilePanel> ProfilePanel;

	/** Currently displayed overlay widget (may be null). */
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveOverlay;

private:
	// ---------------------------------------------------------------
	//  Button callbacks
	// ---------------------------------------------------------------

	UFUNCTION()
	void OnCreateTabClicked();

	UFUNCTION()
	void OnRoomsTabClicked();

	UFUNCTION()
	void OnFollowTabClicked();

	UFUNCTION()
	void OnGamesTabClicked();

	UFUNCTION()
	void OnProfileTabClicked();

	// ---------------------------------------------------------------
	//  Internal helpers
	// ---------------------------------------------------------------

	/** Creates panel widget instances and adds them to the switcher. */
	void CreatePanels();

	/** Updates visual style on all tab buttons to reflect the active tab. */
	void UpdateTabButtonStyles();

	/** Returns the tab button for the given index. */
	UButton* GetTabButton(int32 Index) const;

	/** Index of the currently active tab. */
	int32 ActiveTabIndex = 1; // Default to Rooms

	/** Tab indices as named constants for readability. */
	static constexpr int32 TabCreate  = 0;
	static constexpr int32 TabRooms   = 1;
	static constexpr int32 TabFollow  = 2;
	static constexpr int32 TabGames   = 3;
	static constexpr int32 TabProfile = 4;
	static constexpr int32 NumTabs    = 5;
};
