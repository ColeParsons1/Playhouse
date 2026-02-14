// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Panels/PHCProfilePanel.h"
#include "UI/PHCMainHUD.h"
#include "Core/PHCGameInstance.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCProfilePanel, Log, All);

// ===================================================================
UPHCProfilePanel::UPHCProfilePanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ===================================================================
bool UPHCProfilePanel::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult) return false;

	if (!UsernameText)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bg"));
		Bg->SetBrushColor(FLinearColor(0.051f, 0.051f, 0.102f, 1.0f));
		Bg->SetPadding(FMargin(20.f));

		UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Bg);
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ProfileVBox"));
		Bg->SetContent(VBox);

		// Title
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleLabel"));
		Title->SetText(FText::FromString(TEXT("Profile")));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 24;
		Title->SetFont(TitleFont);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(Title);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);

		// Username (large)
		UsernameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UsernameText"));
		UsernameText->SetText(FText::FromString(TEXT("@player")));
		FSlateFontInfo UserFont = UsernameText->GetFont();
		UserFont.Size = 20;
		UsernameText->SetFont(UserFont);
		UsernameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UVerticalBoxSlot* UserSlot = VBox->AddChildToVerticalBox(UsernameText);
		UserSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		UserSlot->SetHorizontalAlignment(HAlign_Center);

		// Display name
		DisplayNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DisplayNameText"));
		DisplayNameText->SetText(FText::FromString(TEXT("")));
		FSlateFontInfo DispFont = DisplayNameText->GetFont();
		DispFont.Size = 14;
		DisplayNameText->SetFont(DispFont);
		DisplayNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.8f)));
		UVerticalBoxSlot* DispSlot = VBox->AddChildToVerticalBox(DisplayNameText);
		DispSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		DispSlot->SetHorizontalAlignment(HAlign_Center);

		// Level
		LevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LevelText"));
		LevelText->SetText(FText::FromString(TEXT("Level 1")));
		FSlateFontInfo LvlFont = LevelText->GetFont();
		LvlFont.Size = 16;
		LevelText->SetFont(LvlFont);
		LevelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.8f, 1.f)));
		UVerticalBoxSlot* LvlSlot = VBox->AddChildToVerticalBox(LevelText);
		LvlSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		LvlSlot->SetHorizontalAlignment(HAlign_Center);

		// Coins + Gems row
		UHorizontalBox* EconBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EconBox"));
		UVerticalBoxSlot* EconSlot = VBox->AddChildToVerticalBox(EconBox);
		EconSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		EconSlot->SetHorizontalAlignment(HAlign_Center);

		CoinsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CoinsText"));
		CoinsText->SetText(FText::FromString(TEXT("Coins: 0")));
		FSlateFontInfo CoinFont = CoinsText->GetFont();
		CoinFont.Size = 14;
		CoinsText->SetFont(CoinFont);
		CoinsText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
		UHorizontalBoxSlot* CoinSlot = EconBox->AddChildToHorizontalBox(CoinsText);
		CoinSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));

		GemsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GemsText"));
		GemsText->SetText(FText::FromString(TEXT("Gems: 0")));
		FSlateFontInfo GemFont = GemsText->GetFont();
		GemFont.Size = 14;
		GemsText->SetFont(GemFont);
		GemsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.8f, 1.f)));
		EconBox->AddChildToHorizontalBox(GemsText);

		// Followers / Following row
		UHorizontalBox* SocialBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SocialBox"));
		UVerticalBoxSlot* SocialSlot = VBox->AddChildToVerticalBox(SocialBox);
		SocialSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		SocialSlot->SetHorizontalAlignment(HAlign_Center);

		FollowersText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FollowersText"));
		FollowersText->SetText(FText::FromString(TEXT("0 Followers")));
		FSlateFontInfo FollFont = FollowersText->GetFont();
		FollFont.Size = 12;
		FollowersText->SetFont(FollFont);
		FollowersText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.8f)));
		UHorizontalBoxSlot* FollSlot = SocialBox->AddChildToHorizontalBox(FollowersText);
		FollSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));

		FollowingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FollowingText"));
		FollowingText->SetText(FText::FromString(TEXT("0 Following")));
		FSlateFontInfo FingFont = FollowingText->GetFont();
		FingFont.Size = 12;
		FollowingText->SetFont(FingFont);
		FollowingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.8f)));
		SocialBox->AddChildToHorizontalBox(FollowingText);

		// Logout button
		LogoutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LogoutButton"));
		FButtonStyle LogoutStyle = LogoutButton->GetStyle();
		LogoutStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.7f, 0.15f, 0.15f, 1.f));
		LogoutStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f, 1.f));
		LogoutStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.5f, 0.1f, 0.1f, 1.f));
		LogoutButton->SetStyle(LogoutStyle);

		UTextBlock* LogoutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LogoutLabel"));
		LogoutLabel->SetText(FText::FromString(TEXT("Log Out")));
		FSlateFontInfo LogoutFont = LogoutLabel->GetFont();
		LogoutFont.Size = 14;
		LogoutLabel->SetFont(LogoutFont);
		LogoutLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		LogoutLabel->SetJustification(ETextJustify::Center);
		LogoutButton->AddChild(LogoutLabel);

		UVerticalBoxSlot* LogoutSlot = VBox->AddChildToVerticalBox(LogoutButton);
		LogoutSlot->SetHorizontalAlignment(HAlign_Center);
		LogoutSlot->SetPadding(FMargin(40.f, 0.f));
	}

	return true;
}

// ===================================================================
void UPHCProfilePanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button callbacks.
	if (EditAvatarButton)
	{
		EditAvatarButton->OnClicked.AddDynamic(this, &UPHCProfilePanel::OnEditAvatarClicked);
	}
	if (EditProfileButton)
	{
		EditProfileButton->OnClicked.AddDynamic(this, &UPHCProfilePanel::OnEditProfileClicked);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UPHCProfilePanel::OnSettingsClicked);
	}
	if (LogoutButton)
	{
		LogoutButton->OnClicked.AddDynamic(this, &UPHCProfilePanel::OnLogoutClicked);
	}

	// Load local player profile on construct.
	RefreshProfile();

	UE_LOG(LogPHCProfilePanel, Verbose, TEXT("ProfilePanel constructed"));
}

// ===================================================================
//  Public API
// ===================================================================

void UPHCProfilePanel::RefreshProfile()
{
	// Use the locally cached profile from the GameInstance instead of
	// hitting /api/players/ (which returns ALL players, not just ours).
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI && GI->bIsLoggedIn)
	{
		CachedProfile = GI->LocalPlayerProfile;
		OnProfileReceived(CachedProfile);
		UE_LOG(LogPHCProfilePanel, Log, TEXT("Profile loaded from GameInstance: %s (Level %d)"),
			*CachedProfile.Username, CachedProfile.Level);
	}
	else
	{
		UE_LOG(LogPHCProfilePanel, Warning, TEXT("No GameInstance or not logged in -- cannot load profile."));
	}
}

// ===================================================================
//  Button Callbacks
// ===================================================================

void UPHCProfilePanel::OnEditAvatarClicked()
{
	UE_LOG(LogPHCProfilePanel, Log, TEXT("Edit Avatar clicked"));

	// TODO: Phase 5 - Create and show avatar editor overlay.
	// UPHCMainHUD* MainHUD = GetMainHUD();
	// if (MainHUD) { MainHUD->ShowOverlayWidget(AvatarEditorWidget); }
}

void UPHCProfilePanel::OnEditProfileClicked()
{
	UE_LOG(LogPHCProfilePanel, Log, TEXT("Edit Profile clicked"));

	// TODO: Phase 5 - Create and show profile edit popup.
	// This would allow editing display name, bio, etc.
}

void UPHCProfilePanel::OnSettingsClicked()
{
	UE_LOG(LogPHCProfilePanel, Log, TEXT("Settings clicked"));

	// TODO: Phase 5 - Create and show settings popup.
	// Audio, notifications, privacy, etc.
}

void UPHCProfilePanel::OnLogoutClicked()
{
	UE_LOG(LogPHCProfilePanel, Log, TEXT("Logout clicked"));

	// Send logout request to server to invalidate the token.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://tfplayhouse.com/api/players/logout/"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// TODO: Attach auth token.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPHCProfilePanel::OnLogoutResponse);
	HttpRequest->ProcessRequest();
}

// ===================================================================
//  HTTP Handlers
// ===================================================================

void UPHCProfilePanel::OnProfileResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bIsLoading = false;

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogPHCProfilePanel, Error, TEXT("Profile fetch failed: no connection."));
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();

	if (Code >= 200 && Code < 300)
	{
		CachedProfile = ParseProfile(Body);
		OnProfileReceived(CachedProfile);
		UE_LOG(LogPHCProfilePanel, Log, TEXT("Profile loaded: %s (Level %d)"),
			*CachedProfile.Username, CachedProfile.Level);
	}
	else
	{
		UE_LOG(LogPHCProfilePanel, Warning, TEXT("Profile fetch failed. HTTP %d: %s"), Code, *Body);
	}
}

void UPHCProfilePanel::OnLogoutResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	// Regardless of server response, clear local auth state.
	UE_LOG(LogPHCProfilePanel, Log, TEXT("Logout response received. Clearing local session."));

	// The AuthManager handles actual token clearing and state reset.
	// This is triggered through the GameInstance in a production setup.
	// TODO: Call GameInstance->GetAuthManager()->Logout() which handles:
	//   - Token clearing
	//   - Navigation back to login screen
	//   - Clearing cached data

	// For now, log the action.
	if (bConnectedSuccessfully && Response.IsValid())
	{
		const int32 Code = Response->GetResponseCode();
		if (Code >= 200 && Code < 300)
		{
			UE_LOG(LogPHCProfilePanel, Log, TEXT("Server-side logout successful."));
		}
		else
		{
			UE_LOG(LogPHCProfilePanel, Warning, TEXT("Server-side logout returned HTTP %d"), Code);
		}
	}
}

// ===================================================================
//  Parsing
// ===================================================================

FPHCPlayerProfile UPHCProfilePanel::ParseProfile(const FString& JsonString) const
{
	FPHCPlayerProfile Profile;

	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogPHCProfilePanel, Error, TEXT("Failed to parse profile JSON"));
		return Profile;
	}

	Profile.PlayerId      = static_cast<int32>(JsonObj->GetNumberField(TEXT("id")));
	Profile.Username      = JsonObj->GetStringField(TEXT("name"));
	Profile.DisplayName   = JsonObj->GetStringField(TEXT("name"));
	Profile.Coins         = static_cast<int32>(JsonObj->GetNumberField(TEXT("coins")));
	Profile.Gems          = static_cast<int32>(JsonObj->GetNumberField(TEXT("gems")));
	Profile.Level         = static_cast<int32>(JsonObj->GetNumberField(TEXT("level")));
	Profile.XP            = static_cast<int32>(JsonObj->GetNumberField(TEXT("xp")));

	double TmpFollowers = 0;
	if (JsonObj->TryGetNumberField(TEXT("followers"), TmpFollowers))
	{
		Profile.FollowerCount = static_cast<int32>(TmpFollowers);
	}

	bool bIsCpu = false;
	JsonObj->TryGetBoolField(TEXT("is_cpu"), bIsCpu);
	Profile.bIsGuest = bIsCpu;

	return Profile;
}

// ===================================================================
//  Display
// ===================================================================

void UPHCProfilePanel::OnProfileReceived(const FPHCPlayerProfile& Profile)
{
	if (UsernameText)
	{
		UsernameText->SetText(FText::FromString(FString::Printf(TEXT("@%s"), *Profile.Username)));
	}
	if (DisplayNameText)
	{
		DisplayNameText->SetText(FText::FromString(
			Profile.DisplayName.IsEmpty() ? Profile.Username : Profile.DisplayName));
	}
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Level %d"), Profile.Level)));
	}
	if (XPBar)
	{
		// Calculate XP progress within the current level.
		const float Progress = (XPPerLevel > 0)
			? FMath::Clamp(static_cast<float>(Profile.XP % XPPerLevel) / static_cast<float>(XPPerLevel), 0.f, 1.f)
			: 0.f;
		XPBar->SetPercent(Progress);
	}
	if (CoinsText)
	{
		CoinsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Profile.Coins)));
	}
	if (GemsText)
	{
		GemsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Profile.Gems)));
	}
	if (FollowersText)
	{
		FollowersText->SetText(FText::FromString(FString::Printf(TEXT("%d Followers"), Profile.FollowerCount)));
	}
	if (FollowingText)
	{
		FollowingText->SetText(FText::FromString(FString::Printf(TEXT("%d Following"), Profile.FollowingCount)));
	}

	// Load avatar image.
	if (!Profile.AvatarUrl.IsEmpty())
	{
		LoadAvatarImage(Profile.AvatarUrl);
	}

	// Sync economy display to the HUD top bar.
	SyncEconomyToHUD(Profile);
}

// ===================================================================
//  Avatar Image
// ===================================================================

void UPHCProfilePanel::LoadAvatarImage(const FString& Url)
{
	if (Url.IsEmpty())
	{
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPHCProfilePanel::OnAvatarImageReceived);
	HttpRequest->ProcessRequest();

	UE_LOG(LogPHCProfilePanel, Verbose, TEXT("Downloading avatar image: %s"), *Url);
}

void UPHCProfilePanel::OnAvatarImageReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		UE_LOG(LogPHCProfilePanel, Warning, TEXT("Failed to download avatar image."));
		return;
	}

	const TArray<uint8>& ImageData = Response->GetContent();
	if (ImageData.Num() == 0)
	{
		return;
	}

	// Detect image format and decode.
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	// Try PNG first, then JPEG.
	EImageFormat DetectedFormat = EImageFormat::PNG;
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(DetectedFormat);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
	{
		DetectedFormat = EImageFormat::JPEG;
		ImageWrapper = ImageWrapperModule.CreateImageWrapper(DetectedFormat);
		if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(ImageData.GetData(), ImageData.Num()))
		{
			UE_LOG(LogPHCProfilePanel, Warning, TEXT("Could not decode avatar image (tried PNG and JPEG)."));
			return;
		}
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		UE_LOG(LogPHCProfilePanel, Warning, TEXT("Failed to get raw data from avatar image."));
		return;
	}

	const int32 Width = ImageWrapper->GetWidth();
	const int32 Height = ImageWrapper->GetHeight();

	// Create a transient UTexture2D.
	UTexture2D* AvatarTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!AvatarTexture)
	{
		return;
	}

	// Copy the raw pixel data into the texture.
	FTexture2DMipMap& Mip = AvatarTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
	Mip.BulkData.Unlock();
	AvatarTexture->UpdateResource();

	// Apply to the image widget.
	if (AvatarImage)
	{
		AvatarImage->SetBrushFromTexture(AvatarTexture);
		UE_LOG(LogPHCProfilePanel, Verbose, TEXT("Avatar image applied (%dx%d)"), Width, Height);
	}
}

// ===================================================================
//  Helpers
// ===================================================================

void UPHCProfilePanel::SyncEconomyToHUD(const FPHCPlayerProfile& Profile)
{
	UPHCMainHUD* MainHUD = GetMainHUD();
	if (MainHUD)
	{
		MainHUD->UpdateEconomyDisplay(Profile.Coins, Profile.Gems);
		MainHUD->SetUsernameDisplay(Profile.Username);
	}
}

UPHCMainHUD* UPHCProfilePanel::GetMainHUD() const
{
	UObject* Current = GetParent();
	while (Current)
	{
		if (UPHCMainHUD* HUD = Cast<UPHCMainHUD>(Current))
		{
			return HUD;
		}
		Current = Current->GetOuter();
	}
	return nullptr;
}
