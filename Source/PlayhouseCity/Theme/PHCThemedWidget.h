// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCThemedWidget.generated.h"

class UPHCThemeManager;
class UImage;
class UBorder;
class UTextBlock;
class UButton;
class UEditableTextBox;

/**
 * Base widget class that automatically subscribes to the ThemeManager
 * and re-paints itself whenever the active theme changes.
 *
 * Subclasses override ApplyTheme() to map theme colours onto their
 * specific widget tree.  The default implementation walks all immediate
 * children and applies colours heuristically (background -> Image/Border,
 * text colour -> TextBlock, accent -> Button).
 *
 * Usage:
 *   1. Create a Blueprint that inherits from this class.
 *   2. Override ApplyTheme in Blueprint (or C++) to customise mapping.
 *   3. The widget will automatically react to theme changes at runtime.
 */
UCLASS(Abstract, Blueprintable)
class PLAYHOUSECITY_API UPHCThemedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCThemedWidget(const FObjectInitializer& ObjectInitializer);

protected:
	// ----------------------------------------------------------------
	// UUserWidget overrides
	// ----------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ----------------------------------------------------------------
	// Theme application (virtual -- override in subclasses)
	// ----------------------------------------------------------------

	/**
	 * Called once on construct and again every time the theme changes.
	 * The default implementation applies colours to well-known widget types
	 * found in the child hierarchy.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PHC|Theme")
	void ApplyTheme(const FPHCThemeData& Theme);
	virtual void ApplyTheme_Implementation(const FPHCThemeData& Theme);

	// ----------------------------------------------------------------
	// Colour helpers -- usable from subclass C++ and Blueprint
	// ----------------------------------------------------------------

	/** Applies a single colour to a widget, adapting based on widget type. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	static void ApplyColorToWidget(UWidget* Target, const FLinearColor& Color);

	/** Convenience: set the tint of an Image widget. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	static void SetImageTint(UImage* Target, const FLinearColor& Color);

	/** Convenience: set the brush colour of a Border widget. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	static void SetBorderBrush(UBorder* Target, const FLinearColor& Color);

	/** Convenience: set the text colour of a TextBlock. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	static void SetTextColor(UTextBlock* Target, const FLinearColor& Color);

	/** Convenience: set the background colour of a Button (normal / hovered / pressed). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	static void SetButtonColor(UButton* Target, const FLinearColor& NormalColor, const FLinearColor& HoveredColor, const FLinearColor& PressedColor);

	/** Convenience: set the text + background colours of an editable text box. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Theme")
	static void SetEditableTextBoxColors(UEditableTextBox* Target, const FLinearColor& InTextColor, const FLinearColor& InBackgroundColor);

	// ----------------------------------------------------------------
	// Cached theme colours -- subclasses can read directly
	// ----------------------------------------------------------------

	/** Brush colour derived from the current theme's BackgroundColor. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Theme")
	FLinearColor CachedBackgroundColor;

	/** Text colour derived from the current theme. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Theme")
	FLinearColor CachedTextColor;

	/** Accent colour derived from the current theme. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Theme")
	FLinearColor CachedAccentColor;

	/** Primary colour derived from the current theme. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Theme")
	FLinearColor CachedPrimaryColor;

	/** Surface / card colour derived from the current theme. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Theme")
	FLinearColor CachedSurfaceColor;

private:
	// ----------------------------------------------------------------
	// Delegate callback
	// ----------------------------------------------------------------

	UFUNCTION()
	void OnThemeChanged(const FPHCThemeData& NewTheme);

	/** Resolves the ThemeManager from the GameInstance. */
	UPHCThemeManager* GetThemeManager() const;

	// ----------------------------------------------------------------
	// State
	// ----------------------------------------------------------------

	/** Cached reference so we can unbind on destruct. */
	UPROPERTY()
	TObjectPtr<UPHCThemeManager> CachedThemeManager;
};
