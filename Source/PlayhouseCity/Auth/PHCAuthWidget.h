// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PHCAuthWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UThrobber;
class UWidgetSwitcher;
class UPHCAuthManager;

/**
 * Full-featured login / register / guest UI widget.
 *
 * Designed to be placed as the root widget on the authentication screen.
 * The entire UI tree is built programmatically in NativeConstruct();
 * no UMG Blueprint layout is required. Sub-widgets are marked
 * BindWidgetOptional so a Blueprint may still override them if desired.
 *
 * Flow:
 *   1. User sees the Login panel by default.
 *   2. Toggle button switches between Login and Register panels.
 *   3. Guest button bypasses credentials entirely.
 *   4. On success the widget fires OnNavigateAway so the owning
 *      HUD / level blueprint can transition to the main menu.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UPHCAuthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCAuthWidget(const FObjectInitializer& ObjectInitializer);

	// ----------------------------------------------------------------
	// Blueprint event -- fired when auth succeeds and the UI should close.
	// ----------------------------------------------------------------
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNavigateAway);

	UPROPERTY(BlueprintAssignable, Category = "PHC|Auth")
	FOnNavigateAway OnNavigateAway;

protected:
	// ----------------------------------------------------------------
	// UUserWidget overrides
	// ----------------------------------------------------------------
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ----------------------------------------------------------------
	// Bound widgets (built programmatically, or overridden by Blueprint)
	// ----------------------------------------------------------------

	/** Email input shared between login and register flows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> EmailInput;

	/** Password input shared between login and register flows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> PasswordInput;

	/** Username input -- only visible / relevant in register mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> UsernameInput;

	/** Displays validation / server error messages. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoginButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RegisterButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> GuestButton;

	/** Toggles the view between login and register modes. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ToggleButton;

	/** Animated spinner shown while a request is in-flight. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UThrobber> LoadingSpinner;

	/**
	 * Switcher with at least two children:
	 *   Index 0 = Login panel
	 *   Index 1 = Register panel
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> PanelSwitcher;

	/** Optional label on the toggle button so we can update its text. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ToggleButtonText;

	// ----------------------------------------------------------------
	// Button handlers
	// ----------------------------------------------------------------

	UFUNCTION()
	void OnLoginClicked();

	UFUNCTION()
	void OnRegisterClicked();

	UFUNCTION()
	void OnGuestClicked();

	UFUNCTION()
	void OnToggleClicked();

	// ----------------------------------------------------------------
	// Auth result callback
	// ----------------------------------------------------------------

	UFUNCTION()
	void OnAuthResult(bool bSuccess, const FString& ErrorMessage);

	// ----------------------------------------------------------------
	// Helpers
	// ----------------------------------------------------------------

	/** Display an error string beneath the form. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void ShowError(const FString& Message);

	/** Enable / disable buttons and show / hide the loading spinner. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Auth")
	void SetLoadingState(bool bLoading);

private:
	/** Resolve and cache the AuthManager from the GameInstance. */
	UPHCAuthManager* GetAuthManager() const;

	/** True when the widget is showing the login panel. */
	bool bShowingLogin = true;
};
