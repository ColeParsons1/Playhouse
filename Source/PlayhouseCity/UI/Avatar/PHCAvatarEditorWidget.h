// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCAvatarEditorWidget.generated.h"

class UImage;
class UHorizontalBox;
class UScrollBox;
class UButton;
class UTextBlock;
class UBorder;
class UPHCGameInstance;
class UPHCHttpClient;

/**
 * Delegate broadcast when the avatar editor is closed.
 * @param bSaved  True if the user saved changes before closing.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAvatarEditorClosed, bool, bSaved);

/**
 * UPHCAvatarEditorWidget
 *
 * Full avatar customization screen with layered 2D sprite composition.
 * Users select categories (Hair, Eyes, Mouth, Outfit, Accessories),
 * pick items from a scrollable grid, choose skin tones, and preview the
 * composited result in real time before saving to the backend.
 *
 * Internally the avatar is stored as FPHCAvatarData (from PHCTypes.h)
 * which uses ordered FPHCAvatarLayer entries and a SkinTone string.
 * Each category maps to a layer name in the Layers array.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCAvatarEditorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCAvatarEditorWidget(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// -- Public API ------------------------------------------------------------

	/** Broadcast when the editor closes, indicating whether changes were saved. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Avatar")
	FOnAvatarEditorClosed OnEditorClosed;

protected:
	// -- Bind Widgets ----------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> AvatarPreviewImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SkinToneSelector;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> CategoryTabs;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> ItemGrid;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	// -- Data ------------------------------------------------------------------

	/** The avatar being actively edited (layered format). */
	UPROPERTY(BlueprintReadWrite, Category = "PHC|Avatar")
	FPHCAvatarData CurrentAvatar;

	/** Snapshot of the avatar at the time the editor opened (for reset). */
	UPROPERTY(BlueprintReadWrite, Category = "PHC|Avatar")
	FPHCAvatarData OriginalAvatar;

	/** Which category tab is currently selected (e.g. "Hair", "Eyes"). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Avatar")
	FString CurrentCategory;

	/** Maps a category name to a serialized comma-separated list (for Blueprint access). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Avatar")
	TMap<FString, FString> AvailableItemsSerialized;

	// -- Methods ---------------------------------------------------------------

	/** Load current avatar data from the GameInstance / API. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void LoadAvatarData();

	/** Called when a category tab is selected. Repopulates the item grid. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void OnCategorySelected(const FString& Category);

	/** Called when an item is picked from the grid. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void OnItemSelected(const FString& Category, const FString& AssetId);

	/** Called when a skin-tone swatch is picked. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void OnSkinToneSelected(const FString& Tone);

	/** Re-composite all active layers and update the preview image. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void RefreshPreview();

	/** Persist the current avatar to the backend, then close on success. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void OnSaveClicked();

	/** Restore the avatar to its state when the editor was opened. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void OnResetClicked();

	/** Prompt for unsaved changes, then close. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Avatar")
	void OnCloseClicked();

private:
	// -- Internal helpers ------------------------------------------------------

	/** Populate skin-tone swatches in the SkinToneSelector box. */
	void BuildSkinToneSwatches();

	/** Populate category tab buttons in the CategoryTabs box. */
	void BuildCategoryTabs();

	/** Populate ItemGrid with items for the given category. */
	void PopulateItemGrid(const FString& Category);

	/** Callback after the API returns avatar data. */
	UFUNCTION()
	void OnAvatarDataReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Callback after save-avatar API completes. */
	UFUNCTION()
	void OnAvatarSaveResult(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Returns true if the current avatar differs from the original. */
	bool HasUnsavedChanges() const;

	/** Set or update a layer by category name. Creates if not present. */
	void SetLayerAsset(const FString& LayerName, const FString& AssetId);

	/** Get the AssetId for a given layer name, or empty if not found. */
	FString GetLayerAsset(const FString& LayerName) const;

	/** Serialize the CurrentAvatar to a JSON string for the API. */
	FString AvatarToJsonString() const;

	/** Convenience accessor for the HttpClient via GameInstance. */
	UPHCHttpClient* GetHttpClient() const;

	/** Per-category asset ID lists (native container). */
	TMap<FString, TArray<FString>> AvailableItems;

	/** Ordered list of supported categories. */
	static const TArray<FString> Categories;

	/** Ordered list of supported skin-tone presets. */
	static const TArray<FString> SkinTones;

	/** Maps skin-tone names to preview tint colors. */
	static const TMap<FString, FLinearColor> SkinToneColors;

	/** Tracks whether a save operation is in-flight to prevent double-clicks. */
	bool bSaveInFlight = false;

	/** True once initial data has been loaded. */
	bool bDataLoaded = false;
};
