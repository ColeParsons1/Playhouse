// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Panels/PHCCreateRoomPanel.h"
#include "UI/PHCMainHUD.h"

#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCCreateRoom, Log, All);

// ===================================================================
UPHCCreateRoomPanel::UPHCCreateRoomPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ===================================================================
bool UPHCCreateRoomPanel::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult) return false;

	// Only build if not already provided by Blueprint
	if (!RoomNameInput)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bg"));
		Bg->SetBrushColor(FLinearColor(0.051f, 0.051f, 0.102f, 1.0f));
		Bg->SetPadding(FMargin(20.f));

		UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Bg);
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleLabel"));
		Title->SetText(FText::FromString(TEXT("Create Room")));
		FSlateFontInfo Font = Title->GetFont();
		Font.Size = 24;
		Title->SetFont(Font);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		Bg->SetContent(Title);
	}

	return true;
}

// ===================================================================
void UPHCCreateRoomPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// --- Populate theme selector ---
	PopulateThemeSelector();

	// --- Bind callbacks ---
	if (CreateButton)
	{
		CreateButton->OnClicked.AddDynamic(this, &UPHCCreateRoomPanel::OnCreateClicked);
	}

	if (MaxPlayersSlider)
	{
		MaxPlayersSlider->OnValueChanged.AddDynamic(this, &UPHCCreateRoomPanel::OnMaxPlayersChanged);
		// Set default value.
		MaxPlayersSlider->SetValue(PlayerCountToSlider(DefaultPlayers));
	}

	if (PrivateRoomCheckBox)
	{
		PrivateRoomCheckBox->OnCheckStateChanged.AddDynamic(this, &UPHCCreateRoomPanel::OnPrivateToggled);
	}

	// Initialize form to default state.
	ResetForm();

	UE_LOG(LogPHCCreateRoom, Verbose, TEXT("CreateRoomPanel constructed"));
}

// ===================================================================
//  Public API
// ===================================================================

void UPHCCreateRoomPanel::ResetForm()
{
	if (RoomNameInput)
	{
		RoomNameInput->SetText(FText::GetEmpty());
	}
	if (ThemeSelector && ThemeSelector->GetOptionCount() > 0)
	{
		ThemeSelector->SetSelectedIndex(0);
	}
	if (MaxPlayersSlider)
	{
		MaxPlayersSlider->SetValue(PlayerCountToSlider(DefaultPlayers));
	}
	if (MaxPlayersLabel)
	{
		MaxPlayersLabel->SetText(FText::FromString(FString::Printf(TEXT("%d Players"), DefaultPlayers)));
	}
	if (PrivateRoomCheckBox)
	{
		PrivateRoomCheckBox->SetIsChecked(false);
	}
	if (PasswordInput)
	{
		PasswordInput->SetText(FText::GetEmpty());
		PasswordInput->SetVisibility(ESlateVisibility::Collapsed);
	}

	bCreating = false;

	if (CreateButton)
	{
		CreateButton->SetIsEnabled(true);
	}
}

// ===================================================================
//  Theme Selector
// ===================================================================

void UPHCCreateRoomPanel::PopulateThemeSelector()
{
	if (!ThemeSelector)
	{
		return;
	}

	ThemeSelector->ClearOptions();

	// Iterate all theme enum values (skip MAX).
	const UEnum* ThemeEnum = StaticEnum<EPHCRoomTheme>();
	if (!ThemeEnum)
	{
		return;
	}

	for (int32 i = 0; i < ThemeEnum->NumEnums() - 1; ++i) // -1 to skip MAX
	{
		const FString DisplayName = ThemeEnum->GetDisplayNameTextByIndex(i).ToString();
		if (!DisplayName.IsEmpty())
		{
			ThemeSelector->AddOption(DisplayName);
		}
	}

	if (ThemeSelector->GetOptionCount() > 0)
	{
		ThemeSelector->SetSelectedIndex(0);
	}
}

// ===================================================================
//  Callbacks
// ===================================================================

void UPHCCreateRoomPanel::OnCreateClicked()
{
	if (bCreating)
	{
		return; // Prevent double-submit.
	}

	FString ErrorMessage;
	if (!ValidateForm(ErrorMessage))
	{
		UE_LOG(LogPHCCreateRoom, Warning, TEXT("Validation failed: %s"), *ErrorMessage);
		// TODO: Show error toast in UI.
		return;
	}

	bCreating = true;
	if (CreateButton)
	{
		CreateButton->SetIsEnabled(false);
	}

	// Gather form data.
	const FString RoomName = RoomNameInput ? RoomNameInput->GetText().ToString() : TEXT("New Room");
	const FString Theme = ThemeSelector ? ThemeSelector->GetSelectedOption() : TEXT("Default");
	const int32 PlayerCount = MaxPlayersSlider
		? SliderToPlayerCount(MaxPlayersSlider->GetValue())
		: DefaultPlayers;
	const bool bPrivate = PrivateRoomCheckBox ? PrivateRoomCheckBox->IsChecked() : false;
	const FString Password = (bPrivate && PasswordInput)
		? PasswordInput->GetText().ToString()
		: TEXT("");

	// Build JSON payload.
	TSharedPtr<FJsonObject> JsonPayload = MakeShareable(new FJsonObject);
	JsonPayload->SetStringField(TEXT("name"), RoomName);
	JsonPayload->SetStringField(TEXT("theme"), Theme);
	JsonPayload->SetNumberField(TEXT("maxPlayers"), PlayerCount);
	JsonPayload->SetBoolField(TEXT("isPrivate"), bPrivate);
	if (bPrivate && !Password.IsEmpty())
	{
		JsonPayload->SetStringField(TEXT("password"), Password);
	}

	FString PayloadString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
	FJsonSerializer::Serialize(JsonPayload.ToSharedRef(), Writer);

	// Create and send HTTP request.
	// In production this would go through a centralized HttpClient on the GameInstance.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://tfplayhouse.com/api/rooms/"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// TODO: Attach auth token from GameInstance->AuthToken.
	HttpRequest->SetContentAsString(PayloadString);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPHCCreateRoomPanel::OnCreateRoomResponse);
	HttpRequest->ProcessRequest();

	UE_LOG(LogPHCCreateRoom, Log, TEXT("Create room request sent: %s"), *PayloadString);
}

void UPHCCreateRoomPanel::OnMaxPlayersChanged(float Value)
{
	const int32 PlayerCount = SliderToPlayerCount(Value);
	if (MaxPlayersLabel)
	{
		MaxPlayersLabel->SetText(FText::FromString(
			FString::Printf(TEXT("%d Players"), PlayerCount)));
	}
}

void UPHCCreateRoomPanel::OnPrivateToggled(bool bIsChecked)
{
	if (PasswordInput)
	{
		PasswordInput->SetVisibility(bIsChecked
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);

		if (!bIsChecked)
		{
			PasswordInput->SetText(FText::GetEmpty());
		}
	}
}

// ===================================================================
//  Validation
// ===================================================================

bool UPHCCreateRoomPanel::ValidateForm(FString& OutErrorMessage) const
{
	// Room name.
	if (RoomNameInput)
	{
		const FString Name = RoomNameInput->GetText().ToString().TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			OutErrorMessage = TEXT("Room name cannot be empty.");
			return false;
		}
		if (Name.Len() > MaxRoomNameLength)
		{
			OutErrorMessage = FString::Printf(TEXT("Room name cannot exceed %d characters."), MaxRoomNameLength);
			return false;
		}
	}

	// Password (if private).
	if (PrivateRoomCheckBox && PrivateRoomCheckBox->IsChecked())
	{
		if (PasswordInput)
		{
			const FString Pwd = PasswordInput->GetText().ToString();
			if (Pwd.IsEmpty())
			{
				OutErrorMessage = TEXT("Private rooms require a password.");
				return false;
			}
			if (Pwd.Len() < 4)
			{
				OutErrorMessage = TEXT("Password must be at least 4 characters.");
				return false;
			}
		}
	}

	return true;
}

// ===================================================================
//  Slider Conversion
// ===================================================================

int32 UPHCCreateRoomPanel::SliderToPlayerCount(float Value) const
{
	// Map 0..1 linearly to MinPlayers..MaxPlayers, snapped to int.
	const float Clamped = FMath::Clamp(Value, 0.f, 1.f);
	return FMath::RoundToInt32(FMath::Lerp(static_cast<float>(MinPlayers), static_cast<float>(MaxPlayers), Clamped));
}

float UPHCCreateRoomPanel::PlayerCountToSlider(int32 Count) const
{
	if (MaxPlayers == MinPlayers)
	{
		return 0.f;
	}
	return static_cast<float>(Count - MinPlayers) / static_cast<float>(MaxPlayers - MinPlayers);
}

// ===================================================================
//  HTTP Response
// ===================================================================

void UPHCCreateRoomPanel::OnCreateRoomResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bCreating = false;
	if (CreateButton)
	{
		CreateButton->SetIsEnabled(true);
	}

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogPHCCreateRoom, Error, TEXT("Create room request failed: no connection."));
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();

	if (Code >= 200 && Code < 300)
	{
		UE_LOG(LogPHCCreateRoom, Log, TEXT("Room created successfully. Response: %s"), *Body);

		// Parse the room ID from the response.
		TSharedPtr<FJsonObject> JsonResponse;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
		{
			const int32 RoomId = static_cast<int32>(JsonResponse->GetNumberField(TEXT("id")));

			// Navigate to the newly created room via the MainHUD.
			UPHCMainHUD* MainHUD = GetMainHUD();
			if (MainHUD)
			{
				MainHUD->NavigateToRoom(RoomId);
			}
		}

		ResetForm();
	}
	else
	{
		UE_LOG(LogPHCCreateRoom, Warning, TEXT("Create room failed. HTTP %d: %s"), Code, *Body);
		// TODO: Parse error message and display in UI.
	}
}

// ===================================================================
//  Helpers
// ===================================================================

UPHCMainHUD* UPHCCreateRoomPanel::GetMainHUD() const
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
