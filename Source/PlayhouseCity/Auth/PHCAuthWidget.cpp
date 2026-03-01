// Copyright PlayhouseCity. All Rights Reserved.

#include "Auth/PHCAuthWidget.h"
#include "Auth/PHCAuthManager.h"
#include "Core/PHCGameInstance.h"
#include "PlayhouseCity.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Throbber.h"
#include "Components/WidgetSwitcher.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"

// ===========================================================================
// Helpers -- widget construction
// ===========================================================================

namespace PHCAuthWidgetLocal
{
	/**
	 * Create a widget using the WidgetTree if available, otherwise fall back
	 * to NewObject.  The widget is given the supplied name so that
	 * BindWidgetOptional resolution can still find it if desired.
	 */
	template <typename T>
	static T* CreateWidget(UWidgetTree* Tree, UObject* Outer, FName Name)
	{
		if (Tree)
		{
			return Tree->ConstructWidget<T>(T::StaticClass(), Name);
		}
		return NewObject<T>(Outer, T::StaticClass(), Name);
	}

	/** Convenience -- create a styled UButton with a child UTextBlock label. */
	static UButton* MakeButton(
		UWidgetTree* Tree,
		UObject* Outer,
		FName ButtonName,
		const FString& LabelText,
		FLinearColor ButtonColor,
		FLinearColor TextColor,
		TObjectPtr<UTextBlock>* OutLabel = nullptr)
	{
		UButton* Btn = CreateWidget<UButton>(Tree, Outer, ButtonName);

		// Button tint / style.
		FButtonStyle Style = Btn->GetStyle();
		FSlateBrush NormalBrush;
		NormalBrush.TintColor = FSlateColor(ButtonColor);
		Style.SetNormal(NormalBrush);
		Style.SetHovered(NormalBrush);
		Style.SetPressed(NormalBrush);
		Btn->SetStyle(Style);

		// Child label.
		FName LabelName = *FString::Printf(TEXT("%sLabel"), *ButtonName.ToString());
		UTextBlock* Label = CreateWidget<UTextBlock>(Tree, Outer, LabelName);
		Label->SetText(FText::FromString(LabelText));

		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 16;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(TextColor));
		Label->SetJustification(ETextJustify::Center);

		Btn->AddChild(Label);

		if (OutLabel)
		{
			*OutLabel = Label;
		}

		return Btn;
	}
}

// ===========================================================================
// Construction
// ===========================================================================

UPHCAuthWidget::UPHCAuthWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ===========================================================================
// UUserWidget overrides
// ===========================================================================

bool UPHCAuthWidget::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult)
	{
		return false;
	}

	using namespace PHCAuthWidgetLocal;

	// =====================================================================
	// Build the UI tree programmatically (only if the widgets were not
	// already provided by a Blueprint subclass via BindWidgetOptional).
	// This MUST happen in Initialize() -- before TakeWidget() builds the
	// Slate representation from the WidgetTree.
	// =====================================================================

	const bool bNeedsBuild = (LoginButton == nullptr);

	if (bNeedsBuild)
	{
		UWidgetTree* Tree = WidgetTree;
		UObject* Outer = this;

		// ---------------------------------------------------------------
		// Root canvas -- fills the entire viewport.
		// ---------------------------------------------------------------
		UCanvasPanel* RootCanvas = CreateWidget<UCanvasPanel>(Tree, Outer, FName("RootCanvas"));

		// Dark background using a Border that fills the canvas.
		UBorder* Background = CreateWidget<UBorder>(Tree, Outer, FName("Background"));
		Background->SetBrushColor(FLinearColor(0.051f, 0.051f, 0.102f, 1.0f)); // #0D0D1A
		Background->SetPadding(FMargin(0.0f));

		RootCanvas->AddChild(Background);
		if (UCanvasPanelSlot* BgSlot = Cast<UCanvasPanelSlot>(Background->Slot))
		{
			BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BgSlot->SetOffsets(FMargin(0.0f));
		}

		// ---------------------------------------------------------------
		// Centred card wrapper: SizeBox -> Border (card bg) -> VerticalBox
		// ---------------------------------------------------------------
		USizeBox* CardSizeBox = CreateWidget<USizeBox>(Tree, Outer, FName("CardSizeBox"));
		CardSizeBox->SetWidthOverride(400.0f);

		UBorder* CardBorder = CreateWidget<UBorder>(Tree, Outer, FName("CardBorder"));
		CardBorder->SetBrushColor(FLinearColor(0.102f, 0.102f, 0.180f, 1.0f)); // #1A1A2E
		CardBorder->SetPadding(FMargin(30.0f, 40.0f, 30.0f, 40.0f));

		UVerticalBox* FormBox = CreateWidget<UVerticalBox>(Tree, Outer, FName("FormBox"));

		CardBorder->AddChild(FormBox);
		CardSizeBox->AddChild(CardBorder);

		RootCanvas->AddChild(CardSizeBox);
		if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(CardSizeBox->Slot))
		{
			// Centre the card in the viewport.
			CardSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CardSlot->SetAutoSize(true);
		}

		// ---------------------------------------------------------------
		// Title: "Playhouse City"
		// ---------------------------------------------------------------
		UTextBlock* TitleText = CreateWidget<UTextBlock>(Tree, Outer, FName("TitleText"));
		TitleText->SetText(FText::FromString(TEXT("Playhouse City")));
		{
			FSlateFontInfo TitleFont = TitleText->GetFont();
			TitleFont.Size = 32;
			TitleText->SetFont(TitleFont);
		}
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		TitleText->SetJustification(ETextJustify::Center);

		FormBox->AddChild(TitleText);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(TitleText->Slot))
		{
			Slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		// ---------------------------------------------------------------
		// Subtitle: "Welcome"
		// ---------------------------------------------------------------
		UTextBlock* SubtitleText = CreateWidget<UTextBlock>(Tree, Outer, FName("SubtitleText"));
		SubtitleText->SetText(FText::FromString(TEXT("Welcome")));
		{
			FSlateFontInfo SubFont = SubtitleText->GetFont();
			SubFont.Size = 16;
			SubtitleText->SetFont(SubFont);
		}
		SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)));
		SubtitleText->SetJustification(ETextJustify::Center);

		FormBox->AddChild(SubtitleText);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(SubtitleText->Slot))
		{
			Slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		}

		// ---------------------------------------------------------------
		// Spacer (30px)
		// ---------------------------------------------------------------
		USpacer* Spacer1 = CreateWidget<USpacer>(Tree, Outer, FName("Spacer1"));
		Spacer1->SetSize(FVector2D(0.0f, 30.0f));
		FormBox->AddChild(Spacer1);

		// ---------------------------------------------------------------
		// ErrorText (initially collapsed)
		// ---------------------------------------------------------------
		ErrorText = CreateWidget<UTextBlock>(Tree, Outer, FName("ErrorText"));
		ErrorText->SetText(FText::GetEmpty());
		{
			FSlateFontInfo ErrFont = ErrorText->GetFont();
			ErrFont.Size = 13;
			ErrorText->SetFont(ErrFont);
		}
		ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.25f, 0.25f, 1.0f)));
		ErrorText->SetJustification(ETextJustify::Center);
		ErrorText->SetVisibility(ESlateVisibility::Collapsed);

		FormBox->AddChild(ErrorText);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(ErrorText->Slot))
		{
			Slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		// ---------------------------------------------------------------
		// LoadingSpinner (initially collapsed)
		// ---------------------------------------------------------------
		LoadingSpinner = CreateWidget<UThrobber>(Tree, Outer, FName("LoadingSpinner"));
		LoadingSpinner->SetVisibility(ESlateVisibility::Collapsed);

		FormBox->AddChild(LoadingSpinner);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(LoadingSpinner->Slot))
		{
			Slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		// ---------------------------------------------------------------
		// Spacer (10px)
		// ---------------------------------------------------------------
		USpacer* Spacer2 = CreateWidget<USpacer>(Tree, Outer, FName("Spacer2"));
		Spacer2->SetSize(FVector2D(0.0f, 10.0f));
		FormBox->AddChild(Spacer2);

		// ---------------------------------------------------------------
		// AppleSignInButton (black with white text, Apple HIG)
		// ---------------------------------------------------------------
		AppleSignInButton = MakeButton(
			Tree, Outer, FName("AppleSignInButton"),
			TEXT("\xEF\xA3\xBF  Sign in with Apple"),  // Apple logo U+F8FF
			FLinearColor(0.0f, 0.0f, 0.0f, 1.0f),     // black
			FLinearColor::White);

		FormBox->AddChild(AppleSignInButton);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(AppleSignInButton->Slot))
		{
			Slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}

		// ---------------------------------------------------------------
		// GuestButton (smaller, dimmer, for dev/testing fallback)
		// ---------------------------------------------------------------
		GuestButton = MakeButton(
			Tree, Outer, FName("GuestButton"),
			TEXT("Continue as Guest"),
			FLinearColor(0.15f, 0.15f, 0.15f, 1.0f),  // dark grey
			FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));     // grey text

		FormBox->AddChild(GuestButton);
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(GuestButton->Slot))
		{
			Slot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		// ---------------------------------------------------------------
		// Set the root of the widget tree.
		// ---------------------------------------------------------------
		Tree->RootWidget = RootCanvas;
	}

	return true;
}

// ===========================================================================
// NativeConstruct -- bind delegates (widget tree already built in Initialize)
// ===========================================================================

void UPHCAuthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// =====================================================================
	// Button bindings (works for both Blueprint-provided and C++-built UI)
	// =====================================================================
	if (AppleSignInButton)
	{
		AppleSignInButton->OnClicked.AddDynamic(this, &UPHCAuthWidget::OnAppleSignInClicked);
	}
	if (LoginButton)
	{
		LoginButton->OnClicked.AddDynamic(this, &UPHCAuthWidget::OnLoginClicked);
	}
	if (RegisterButton)
	{
		RegisterButton->OnClicked.AddDynamic(this, &UPHCAuthWidget::OnRegisterClicked);
	}
	if (GuestButton)
	{
		GuestButton->OnClicked.AddDynamic(this, &UPHCAuthWidget::OnGuestClicked);
	}
	if (ToggleButton)
	{
		ToggleButton->OnClicked.AddDynamic(this, &UPHCAuthWidget::OnToggleClicked);
	}

	// --- Subscribe to auth results ----------------------------------------
	UPHCAuthManager* AuthManager = GetAuthManager();
	if (AuthManager)
	{
		AuthManager->OnAuthResult.AddDynamic(this, &UPHCAuthWidget::OnAuthResult);
	}

	// --- Initial visual state ---------------------------------------------
	bShowingLogin = true;

	if (PanelSwitcher)
	{
		PanelSwitcher->SetActiveWidgetIndex(0); // Login panel
	}

	if (ErrorText)
	{
		ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (LoadingSpinner)
	{
		LoadingSpinner->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (ToggleButtonText)
	{
		ToggleButtonText->SetText(FText::FromString(TEXT("Create Account")));
	}

	// Username field is only needed in register mode.
	if (UsernameInput)
	{
		UsernameInput->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPHCAuthWidget::NativeDestruct()
{
	// --- Unsubscribe -------------------------------------------------------
	UPHCAuthManager* AuthManager = GetAuthManager();
	if (AuthManager)
	{
		AuthManager->OnAuthResult.RemoveDynamic(this, &UPHCAuthWidget::OnAuthResult);
	}

	Super::NativeDestruct();
}

// ===========================================================================
// Button handlers
// ===========================================================================

void UPHCAuthWidget::OnLoginClicked()
{
	if (!EmailInput || !PasswordInput)
	{
		return;
	}

	const FString Email = EmailInput->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordInput->GetText().ToString();

	// --- Basic client-side validation -------------------------------------
	if (Email.IsEmpty())
	{
		ShowError(TEXT("Please enter your email address."));
		return;
	}
	if (!Email.Contains(TEXT("@")) || !Email.Contains(TEXT(".")))
	{
		ShowError(TEXT("Please enter a valid email address."));
		return;
	}
	if (Password.IsEmpty())
	{
		ShowError(TEXT("Please enter your password."));
		return;
	}

	// --- Fire request ------------------------------------------------------
	UPHCAuthManager* AuthManager = GetAuthManager();
	if (!AuthManager)
	{
		ShowError(TEXT("Authentication system not available."));
		return;
	}

	SetLoadingState(true);
	AuthManager->Login(Email, Password);
}

void UPHCAuthWidget::OnRegisterClicked()
{
	if (!EmailInput || !PasswordInput || !UsernameInput)
	{
		return;
	}

	const FString Username = UsernameInput->GetText().ToString().TrimStartAndEnd();
	const FString Email = EmailInput->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordInput->GetText().ToString();

	// --- Client-side validation --------------------------------------------
	if (Username.IsEmpty())
	{
		ShowError(TEXT("Please choose a username."));
		return;
	}
	if (Username.Len() < 3)
	{
		ShowError(TEXT("Username must be at least 3 characters."));
		return;
	}
	if (Email.IsEmpty())
	{
		ShowError(TEXT("Please enter your email address."));
		return;
	}
	if (!Email.Contains(TEXT("@")) || !Email.Contains(TEXT(".")))
	{
		ShowError(TEXT("Please enter a valid email address."));
		return;
	}
	if (Password.IsEmpty())
	{
		ShowError(TEXT("Please enter a password."));
		return;
	}
	if (Password.Len() < 8)
	{
		ShowError(TEXT("Password must be at least 8 characters."));
		return;
	}

	// --- Fire request ------------------------------------------------------
	UPHCAuthManager* AuthManager = GetAuthManager();
	if (!AuthManager)
	{
		ShowError(TEXT("Authentication system not available."));
		return;
	}

	SetLoadingState(true);
	AuthManager->Register(Username, Email, Password);
}

void UPHCAuthWidget::OnGuestClicked()
{
	UPHCAuthManager* AuthManager = GetAuthManager();
	if (!AuthManager)
	{
		ShowError(TEXT("Authentication system not available."));
		return;
	}

	SetLoadingState(true);
	AuthManager->GuestLogin();
}

void UPHCAuthWidget::OnAppleSignInClicked()
{
	UPHCAuthManager* AuthManager = GetAuthManager();
	if (!AuthManager)
	{
		ShowError(TEXT("Authentication system not available."));
		return;
	}

	SetLoadingState(true);
	AuthManager->AppleSignIn();
}

void UPHCAuthWidget::OnToggleClicked()
{
	bShowingLogin = !bShowingLogin;

	// Hide any previous error.
	if (ErrorText)
	{
		ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PanelSwitcher)
	{
		PanelSwitcher->SetActiveWidgetIndex(bShowingLogin ? 0 : 1);
	}

	// Show / hide the username field.
	if (UsernameInput)
	{
		UsernameInput->SetVisibility(bShowingLogin ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// Update toggle-button label.
	if (ToggleButtonText)
	{
		const FText Label = bShowingLogin
			? FText::FromString(TEXT("Create Account"))
			: FText::FromString(TEXT("Back to Login"));
		ToggleButtonText->SetText(Label);
	}

	// Show / hide the correct action button.
	if (LoginButton)
	{
		LoginButton->SetVisibility(bShowingLogin ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RegisterButton)
	{
		RegisterButton->SetVisibility(bShowingLogin ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

// ===========================================================================
// Auth result callback
// ===========================================================================

void UPHCAuthWidget::OnAuthResult(bool bSuccess, const FString& ErrorMessage)
{
	SetLoadingState(false);

	if (bSuccess)
	{
		// Clear fields for security.
		if (PasswordInput)
		{
			PasswordInput->SetText(FText::GetEmpty());
		}

		// Notify the owning widget / HUD to navigate away.
		OnNavigateAway.Broadcast();
	}
	else
	{
		ShowError(ErrorMessage);
	}
}

// ===========================================================================
// Helpers
// ===========================================================================

void UPHCAuthWidget::ShowError(const FString& Message)
{
	if (!ErrorText)
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("PHCAuthWidget: ErrorText not bound — cannot display: %s"), *Message);
		return;
	}

	ErrorText->SetText(FText::FromString(Message));
	ErrorText->SetVisibility(ESlateVisibility::Visible);
}

void UPHCAuthWidget::SetLoadingState(bool bLoading)
{
	if (LoadingSpinner)
	{
		LoadingSpinner->SetVisibility(bLoading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Disable / re-enable buttons so the user cannot double-submit.
	if (AppleSignInButton)
	{
		AppleSignInButton->SetIsEnabled(!bLoading);
	}
	if (LoginButton)
	{
		LoginButton->SetIsEnabled(!bLoading);
	}
	if (RegisterButton)
	{
		RegisterButton->SetIsEnabled(!bLoading);
	}
	if (GuestButton)
	{
		GuestButton->SetIsEnabled(!bLoading);
	}
	if (ToggleButton)
	{
		ToggleButton->SetIsEnabled(!bLoading);
	}

	// Also disable the text fields to prevent edits mid-flight.
	if (EmailInput)
	{
		EmailInput->SetIsEnabled(!bLoading);
	}
	if (PasswordInput)
	{
		PasswordInput->SetIsEnabled(!bLoading);
	}
	if (UsernameInput)
	{
		UsernameInput->SetIsEnabled(!bLoading);
	}

	// Clear any previous error when starting a new request.
	if (bLoading && ErrorText)
	{
		ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UPHCAuthManager* UPHCAuthWidget::GetAuthManager() const
{
	UPHCGameInstance* GI = Cast<UPHCGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("PHCAuthWidget: GameInstance is not UPHCGameInstance."));
		return nullptr;
	}

	return GI->GetAuthManager();
}
