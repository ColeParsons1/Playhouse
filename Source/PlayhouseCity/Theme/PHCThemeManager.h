// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/PHCTypes.h"
#include "PHCThemeManager.generated.h"

/**
 * Delegate broadcast whenever the active theme changes.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThemeChanged, const FPHCThemeData&, NewTheme);

/**
 * Central authority for the 21 built-in visual themes.
 *
 * Owned by the GameInstance and initialised once at startup.
 * Widgets (typically UPHCThemedWidget subclasses) subscribe to
 * OnThemeChanged to repaint themselves when the user picks a new theme.
 */
UCLASS(BlueprintType)
class PLAYHOUSECITY_API UPHCThemeManager : public UObject
{
	GENERATED_BODY()

public:
	UPHCThemeManager();

	// ----------------------------------------------------------------
	// Lifecycle
	// ----------------------------------------------------------------

	/** Populates the ThemeRegistry with all 21 built-in themes. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	void Initialize();

	// ----------------------------------------------------------------
	// Theme selection
	// ----------------------------------------------------------------

	/**
	 * Switch to the theme identified by ThemeId.
	 * Does nothing (and logs a warning) if the ID is unknown.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	void SetTheme(const FString& ThemeId);

	// ----------------------------------------------------------------
	// Accessors
	// ----------------------------------------------------------------

	/** Returns the currently active theme. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PHC|Theme")
	FPHCThemeData GetCurrentTheme() const;

	/** Returns the current theme ID string. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PHC|Theme")
	FString GetCurrentThemeId() const;

	/** Look up a theme by ID. Returns an invalid (empty) struct if not found. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PHC|Theme")
	FPHCThemeData GetTheme(const FString& ThemeId) const;

	/** Returns the IDs of every registered theme. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PHC|Theme")
	TArray<FString> GetAllThemeIds() const;

	/** Returns a copy of every registered theme. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PHC|Theme")
	TArray<FPHCThemeData> GetAllThemes() const;

	// ----------------------------------------------------------------
	// Delegate
	// ----------------------------------------------------------------

	/** Broadcast every time the active theme changes. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Theme")
	FOnThemeChanged OnThemeChanged;

private:
	/** Internal helper -- creates and registers a single theme entry. */
	void RegisterTheme(
		const FString& InId,
		const FString& InName,
		FLinearColor InPrimary,
		FLinearColor InSecondary,
		FLinearColor InAccent,
		FLinearColor InBackground,
		FLinearColor InSurface,
		FLinearColor InText,
		FLinearColor InTextSecondary,
		FLinearColor InSuccess,
		FLinearColor InWarning,
		FLinearColor InError,
		FLinearColor InBorder);

	/** All registered themes keyed by ThemeId. */
	UPROPERTY()
	TMap<FString, FPHCThemeData> ThemeRegistry;

	/** Snapshot of the currently active theme. */
	UPROPERTY()
	FPHCThemeData CurrentTheme;

	/** Cached ID for quick access. */
	UPROPERTY()
	FString CurrentThemeId;
};
