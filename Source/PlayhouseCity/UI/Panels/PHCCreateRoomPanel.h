// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PHCCreateRoomPanel.generated.h"

class UEditableTextBox;
class UComboBoxString;
class USlider;
class UTextBlock;
class UButton;
class UCheckBox;
class UPHCMainHUD;

/**
 * All available room decoration themes.
 * Matches server-side enum; order is significant for serialization.
 */
UENUM(BlueprintType)
enum class EPHCRoomTheme : uint8
{
	Default = 0		UMETA(DisplayName = "Default"),
	Beach			UMETA(DisplayName = "Beach"),
	Space			UMETA(DisplayName = "Space"),
	Forest			UMETA(DisplayName = "Forest"),
	Castle			UMETA(DisplayName = "Castle"),
	Cyberpunk		UMETA(DisplayName = "Cyberpunk"),
	Underwater		UMETA(DisplayName = "Underwater"),
	Desert			UMETA(DisplayName = "Desert"),
	Arctic			UMETA(DisplayName = "Arctic"),
	Haunted			UMETA(DisplayName = "Haunted"),
	Tropical		UMETA(DisplayName = "Tropical"),
	Retro			UMETA(DisplayName = "Retro"),
	Neon			UMETA(DisplayName = "Neon"),
	Steampunk		UMETA(DisplayName = "Steampunk"),
	Japanese		UMETA(DisplayName = "Japanese"),

	MAX				UMETA(Hidden)
};

/**
 * UPHCCreateRoomPanel
 *
 * Form widget that allows the player to create a new room.
 * Collects room name, theme, max players, and privacy settings,
 * then submits via the HTTP client and navigates on success.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCCreateRoomPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCCreateRoomPanel(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

	// ---------------------------------------------------------------
	//  Public API
	// ---------------------------------------------------------------

	/** Resets the form to default state. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI|CreateRoom")
	void ResetForm();

protected:
	// ---------------------------------------------------------------
	//  Bound Widgets
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomNameInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ThemeSelector;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<USlider> MaxPlayersSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxPlayersLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> PasswordInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> PrivateRoomCheckBox;

	// ---------------------------------------------------------------
	//  Configuration
	// ---------------------------------------------------------------

	/** Minimum allowed players in a room. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|CreateRoom")
	int32 MinPlayers = 2;

	/** Maximum allowed players in a room. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|CreateRoom")
	int32 MaxPlayers = 8;

	/** Default player count. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|CreateRoom")
	int32 DefaultPlayers = 4;

	/** Maximum allowed length for room name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|CreateRoom")
	int32 MaxRoomNameLength = 32;

private:
	// ---------------------------------------------------------------
	//  Callbacks
	// ---------------------------------------------------------------

	UFUNCTION()
	void OnCreateClicked();

	UFUNCTION()
	void OnMaxPlayersChanged(float Value);

	UFUNCTION()
	void OnPrivateToggled(bool bIsChecked);

	// ---------------------------------------------------------------
	//  Internal helpers
	// ---------------------------------------------------------------

	/** Populates the ThemeSelector combo box with all EPHCRoomTheme values. */
	void PopulateThemeSelector();

	/** Validates form input. Returns true if all fields are acceptable. */
	bool ValidateForm(FString& OutErrorMessage) const;

	/** Converts the slider (0..1) to a player count integer. */
	int32 SliderToPlayerCount(float Value) const;

	/** Converts a player count to slider value (0..1). */
	float PlayerCountToSlider(int32 Count) const;

	/** HTTP response handler for room creation. */
	void OnCreateRoomResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Attempts to find the owning MainHUD. */
	UPHCMainHUD* GetMainHUD() const;

	/** True while a create request is in flight. */
	bool bCreating = false;
};
