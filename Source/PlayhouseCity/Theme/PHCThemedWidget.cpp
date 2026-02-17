// Copyright PlayhouseCity. All Rights Reserved.

#include "Theme/PHCThemedWidget.h"
#include "Theme/PHCThemeManager.h"
#include "Core/PHCGameInstance.h"
#include "PlayhouseCity.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

// ===========================================================================
// Construction
// ===========================================================================

UPHCThemedWidget::UPHCThemedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ===========================================================================
// UUserWidget overrides
// ===========================================================================

void UPHCThemedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachedThemeManager = GetThemeManager();

	if (CachedThemeManager)
	{
		// Subscribe to future changes.
		CachedThemeManager->OnThemeChanged.AddDynamic(this, &UPHCThemedWidget::OnThemeChanged);

		// Apply the current theme immediately.
		const FPHCThemeData Theme = CachedThemeManager->GetCurrentTheme();
		if (Theme.IsValid())
		{
			OnThemeChanged(Theme);
		}
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCThemedWidget::NativeConstruct -- ThemeManager not available; widget will not be themed."));
	}
}

void UPHCThemedWidget::NativeDestruct()
{
	if (CachedThemeManager)
	{
		CachedThemeManager->OnThemeChanged.RemoveDynamic(this, &UPHCThemedWidget::OnThemeChanged);
		CachedThemeManager = nullptr;
	}

	Super::NativeDestruct();
}

// ===========================================================================
// Theme application (default implementation)
// ===========================================================================

void UPHCThemedWidget::ApplyTheme_Implementation(const FPHCThemeData& Theme)
{
	// Walk the widget tree and apply colours heuristically.
	if (!WidgetTree)
	{
		return;
	}

	WidgetTree->ForEachWidget([&Theme](UWidget* Widget)
	{
		if (!Widget || !Widget->IsVisible())
		{
			return;
		}

		// --- UImage: treat as a background / decorative element -----------
		if (UImage* Img = Cast<UImage>(Widget))
		{
			Img->SetColorAndOpacity(Theme.SurfaceColor);
			return;
		}

		// --- UBorder: treat as a panel background -------------------------
		if (UBorder* Border = Cast<UBorder>(Widget))
		{
			Border->SetBrushColor(Theme.BackgroundColor);
			return;
		}

		// --- UTextBlock: apply the primary text colour --------------------
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			Text->SetColorAndOpacity(FSlateColor(Theme.TextColor));
			return;
		}

		// --- UButton: tint normal / hovered / pressed ---------------------
		if (UButton* Btn = Cast<UButton>(Widget))
		{
			FButtonStyle Style = Btn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(Theme.PrimaryColor);
			Style.Hovered.TintColor = FSlateColor(Theme.AccentColor);
			Style.Pressed.TintColor = FSlateColor(Theme.SecondaryColor);
			Btn->SetStyle(Style);
			return;
		}

		// --- UEditableTextBox: text + background --------------------------
		if (UEditableTextBox* Box = Cast<UEditableTextBox>(Widget))
		{
			Box->SetForegroundColor(Theme.TextColor);
			return;
		}
	});
}

// ===========================================================================
// Colour helpers
// ===========================================================================

void UPHCThemedWidget::ApplyColorToWidget(UWidget* Target, const FLinearColor& Color)
{
	if (!Target)
	{
		return;
	}

	if (UImage* Img = Cast<UImage>(Target))
	{
		SetImageTint(Img, Color);
	}
	else if (UBorder* Border = Cast<UBorder>(Target))
	{
		SetBorderBrush(Border, Color);
	}
	else if (UTextBlock* Text = Cast<UTextBlock>(Target))
	{
		SetTextColor(Text, Color);
	}
	else if (UButton* Btn = Cast<UButton>(Target))
	{
		// Use the colour for all three button states, slightly modified.
		const FLinearColor Hovered = FLinearColor(
			FMath::Min(Color.R * 1.2f, 1.0f),
			FMath::Min(Color.G * 1.2f, 1.0f),
			FMath::Min(Color.B * 1.2f, 1.0f),
			Color.A);
		const FLinearColor Pressed = FLinearColor(
			Color.R * 0.8f,
			Color.G * 0.8f,
			Color.B * 0.8f,
			Color.A);
		SetButtonColor(Btn, Color, Hovered, Pressed);
	}
	else if (UEditableTextBox* Box = Cast<UEditableTextBox>(Target))
	{
		Box->SetForegroundColor(Color);
	}
}

void UPHCThemedWidget::SetImageTint(UImage* Target, const FLinearColor& Color)
{
	if (Target)
	{
		Target->SetColorAndOpacity(Color);
	}
}

void UPHCThemedWidget::SetBorderBrush(UBorder* Target, const FLinearColor& Color)
{
	if (Target)
	{
		Target->SetBrushColor(Color);
	}
}

void UPHCThemedWidget::SetTextColor(UTextBlock* Target, const FLinearColor& Color)
{
	if (Target)
	{
		Target->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UPHCThemedWidget::SetButtonColor(UButton* Target, const FLinearColor& NormalColor, const FLinearColor& HoveredColor, const FLinearColor& PressedColor)
{
	if (!Target)
	{
		return;
	}

	FButtonStyle Style = Target->GetStyle();
	Style.Normal.TintColor  = FSlateColor(NormalColor);
	Style.Hovered.TintColor = FSlateColor(HoveredColor);
	Style.Pressed.TintColor = FSlateColor(PressedColor);
	Target->SetStyle(Style);
}

void UPHCThemedWidget::SetEditableTextBoxColors(UEditableTextBox* Target, const FLinearColor& InTextColor, const FLinearColor& InBackgroundColor)
{
	if (!Target)
	{
		return;
	}

	Target->SetForegroundColor(InTextColor);

	// Apply background tint through the widget style.
	FEditableTextBoxStyle BoxStyle = Target->GetWidgetStyle();
	BoxStyle.BackgroundImageNormal.TintColor  = FSlateColor(InBackgroundColor);
	BoxStyle.BackgroundImageHovered.TintColor = FSlateColor(InBackgroundColor);
	BoxStyle.BackgroundImageFocused.TintColor = FSlateColor(InBackgroundColor);
	Target->SetWidgetStyle(BoxStyle);
}

// ===========================================================================
// Delegate callback
// ===========================================================================

void UPHCThemedWidget::OnThemeChanged(const FPHCThemeData& NewTheme)
{
	// Cache semantic colours for easy subclass access.
	CachedBackgroundColor = NewTheme.BackgroundColor;
	CachedTextColor       = NewTheme.TextColor;
	CachedAccentColor     = NewTheme.AccentColor;
	CachedPrimaryColor    = NewTheme.PrimaryColor;
	CachedSurfaceColor    = NewTheme.SurfaceColor;

	// Invoke the (possibly overridden) theme application logic.
	ApplyTheme(NewTheme);
}

// ===========================================================================
// Helpers
// ===========================================================================

UPHCThemeManager* UPHCThemedWidget::GetThemeManager() const
{
	UPHCGameInstance* GI = Cast<UPHCGameInstance>(GetGameInstance());
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetThemeManager();
}
