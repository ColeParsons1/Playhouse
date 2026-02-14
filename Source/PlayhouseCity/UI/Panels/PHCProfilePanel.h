// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PHCProfilePanel.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UButton;
class UPHCMainHUD;

/**
 * UPHCProfilePanel
 *
 * Displays the local player's profile information including
 * username, display name, level/XP, economy balances, follow
 * counts, and avatar. Provides edit and logout actions.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCProfilePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCProfilePanel(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

	// ---------------------------------------------------------------
	//  Public API
	// ---------------------------------------------------------------

	/** Fetches the profile from the server and updates all display fields. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|UI|Profile")
	void RefreshProfile();

protected:
	// ---------------------------------------------------------------
	//  Bound Widgets
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UsernameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DisplayNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> XPBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GemsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FollowersText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FollowingText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AvatarImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> EditAvatarButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> EditProfileButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LogoutButton;

	// ---------------------------------------------------------------
	//  Configuration
	// ---------------------------------------------------------------

	/** XP required per level. Used to calculate XP bar percentage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|UI|Profile")
	int32 XPPerLevel = 1000;

private:
	// ---------------------------------------------------------------
	//  Callbacks
	// ---------------------------------------------------------------

	UFUNCTION()
	void OnEditAvatarClicked();

	UFUNCTION()
	void OnEditProfileClicked();

	UFUNCTION()
	void OnSettingsClicked();

	UFUNCTION()
	void OnLogoutClicked();

	// ---------------------------------------------------------------
	//  HTTP
	// ---------------------------------------------------------------

	void OnProfileResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
	void OnLogoutResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	// ---------------------------------------------------------------
	//  Internal helpers
	// ---------------------------------------------------------------

	/** Parses JSON into a player profile struct. */
	FPHCPlayerProfile ParseProfile(const FString& JsonString) const;

	/** Applies a profile struct to all bound widgets. */
	void OnProfileReceived(const FPHCPlayerProfile& Profile);

	/** Downloads and applies the avatar image from a URL. */
	void LoadAvatarImage(const FString& Url);

	/** Callback for avatar image download. */
	void OnAvatarImageReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Updates the MainHUD economy display to match the profile. */
	void SyncEconomyToHUD(const FPHCPlayerProfile& Profile);

	/** Attempts to find the owning MainHUD. */
	UPHCMainHUD* GetMainHUD() const;

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** Cached profile data. */
	FPHCPlayerProfile CachedProfile;

	/** True while a profile fetch is in-flight. */
	bool bIsLoading = false;
};
