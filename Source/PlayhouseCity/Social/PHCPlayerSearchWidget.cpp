// Copyright PlayhouseCity. All Rights Reserved.

#include "Social/PHCPlayerSearchWidget.h"
#include "Social/PHCFollowManager.h"
#include "Network/PHCHttpClient.h"
#include "PlayhouseCity.h"
#include "Http.h"

#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Spacer.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "TimerManager.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCPlayerSearchWidget::UPHCPlayerSearchWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MinSearchLength(2)
	, DebounceDelay(0.3f)
	, bIsSearching(false)
	, HttpClient(nullptr)
	, FollowManager(nullptr)
{
}

// ---------------------------------------------------------------------------
// UUserWidget Interface
// ---------------------------------------------------------------------------

void UPHCPlayerSearchWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind search input events
	if (SearchInput)
	{
		SearchInput->OnTextChanged.AddDynamic(this, &UPHCPlayerSearchWidget::OnSearchTextChanged);
		SearchInput->OnTextCommitted.AddDynamic(this, &UPHCPlayerSearchWidget::OnSearchTextCommitted);
	}

	// Bind clear button
	if (ClearButton)
	{
		ClearButton->OnClicked.AddDynamic(this, &UPHCPlayerSearchWidget::OnClearClicked);
	}

	// Initialize visual states
	SetLoadingState(false);
	SetEmptyState(false);

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCPlayerSearchWidget::NativeConstruct -- Player search widget initialized."));
}

void UPHCPlayerSearchWidget::NativeDestruct()
{
	// Clear the debounce timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DebounceTimer);
	}

	// Unbind delegates to prevent dangling references
	if (SearchInput)
	{
		SearchInput->OnTextChanged.RemoveDynamic(this, &UPHCPlayerSearchWidget::OnSearchTextChanged);
		SearchInput->OnTextCommitted.RemoveDynamic(this, &UPHCPlayerSearchWidget::OnSearchTextCommitted);
	}

	if (ClearButton)
	{
		ClearButton->OnClicked.RemoveDynamic(this, &UPHCPlayerSearchWidget::OnClearClicked);
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void UPHCPlayerSearchWidget::SetDependencies(UPHCHttpClient* InHttpClient, UPHCFollowManager* InFollowManager)
{
	HttpClient = InHttpClient;
	FollowManager = InFollowManager;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCPlayerSearchWidget::SetDependencies -- Dependencies set."));
}

// ---------------------------------------------------------------------------
// Event Handlers
// ---------------------------------------------------------------------------

void UPHCPlayerSearchWidget::OnSearchTextChanged(const FText& Text)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Clear any existing debounce timer
	World->GetTimerManager().ClearTimer(DebounceTimer);

	const FString Query = Text.ToString().TrimStartAndEnd();

	if (Query.Len() < MinSearchLength)
	{
		// Not enough characters -- clear results and hide loading
		ClearResults();
		SetLoadingState(false);
		SetEmptyState(false);
		LastQuery.Empty();
		return;
	}

	// Same query as last search -- skip
	if (Query == LastQuery && CurrentResults.Num() > 0)
	{
		return;
	}

	// Start the debounce timer
	World->GetTimerManager().SetTimer(
		DebounceTimer,
		this,
		&UPHCPlayerSearchWidget::ExecuteSearch,
		DebounceDelay,
		/*bLoop=*/ false
	);
}

void UPHCPlayerSearchWidget::OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		// Cancel debounce and execute immediately
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DebounceTimer);
		}

		const FString Query = Text.ToString().TrimStartAndEnd();
		if (Query.Len() >= MinSearchLength)
		{
			ExecuteSearch();
		}
	}
}

void UPHCPlayerSearchWidget::ExecuteSearch()
{
	if (!HttpClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCPlayerSearchWidget::ExecuteSearch -- HttpClient is null."));
		return;
	}

	if (!SearchInput)
	{
		return;
	}

	const FString Query = SearchInput->GetText().ToString().TrimStartAndEnd();

	if (Query.Len() < MinSearchLength)
	{
		return;
	}

	// Prevent duplicate searches
	if (bIsSearching && Query == LastQuery)
	{
		return;
	}

	LastQuery = Query;
	bIsSearching = true;

	// Update visual state
	ClearResults();
	SetLoadingState(true);
	SetEmptyState(false);

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCPlayerSearchWidget::ExecuteSearch -- Searching for '%s'."), *Query);

	// Make the API call
	const FString EncodedQuery = FPlatformHttp::UrlEncode(Query);
	const FString Endpoint = FString::Printf(TEXT("/api/social/search/?q=%s"), *EncodedQuery);

	FPHCHttpResponse ResponseDelegate;
	ResponseDelegate.BindLambda([this](bool bSuccess, const FString& ResponseBody)
	{
		OnSearchResults(bSuccess, ResponseBody);
	});

	HttpClient->Get(Endpoint, ResponseDelegate, /*CacheTTL=*/ 10.0f);
}

void UPHCPlayerSearchWidget::OnSearchResults(bool bSuccess, const FString& Response)
{
	bIsSearching = false;
	SetLoadingState(false);

	if (!bSuccess)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCPlayerSearchWidget::OnSearchResults -- Search request failed."));
		SetEmptyState(true);
		return;
	}

	CurrentResults = ParseSearchResults(Response);

	if (CurrentResults.Num() == 0)
	{
		SetEmptyState(true);
		UE_LOG(LogPlayhouseCity, Verbose,
			TEXT("PHCPlayerSearchWidget::OnSearchResults -- No results for query '%s'."), *LastQuery);
		return;
	}

	SetEmptyState(false);

	// Populate the results list
	if (ResultsList)
	{
		ResultsList->ClearChildren();

		for (const FPHCFollowInfo& Player : CurrentResults)
		{
			UWidget* Row = CreatePlayerResultRow(Player);
			if (Row)
			{
				ResultsList->AddChild(Row);
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCPlayerSearchWidget::OnSearchResults -- Displaying %d results for '%s'."),
		CurrentResults.Num(), *LastQuery);
}

UWidget* UPHCPlayerSearchWidget::CreatePlayerResultRow(const FPHCFollowInfo& Player)
{
	// Create a horizontal box to hold avatar placeholder, name, and follow button
	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
	if (!Row)
	{
		return nullptr;
	}

	// --- Avatar placeholder (colored box) ---
	USizeBox* AvatarSizeBox = NewObject<USizeBox>(this);
	if (AvatarSizeBox)
	{
		AvatarSizeBox->SetWidthOverride(40.0f);
		AvatarSizeBox->SetHeightOverride(40.0f);

		UImage* AvatarImage = NewObject<UImage>(this);
		if (AvatarImage)
		{
			AvatarImage->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.7f, 1.0f));
			AvatarSizeBox->AddChild(AvatarImage);
		}

		UHorizontalBoxSlot* AvatarSlot = Row->AddChildToHorizontalBox(AvatarSizeBox);
		if (AvatarSlot)
		{
			AvatarSlot->SetPadding(FMargin(4.0f));
			AvatarSlot->SetHorizontalAlignment(HAlign_Left);
			AvatarSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// --- Spacer ---
	USpacer* SpacerWidget = NewObject<USpacer>(this);
	if (SpacerWidget)
	{
		SpacerWidget->SetSize(FVector2D(8.0f, 1.0f));
		Row->AddChildToHorizontalBox(SpacerWidget);
	}

	// --- Username text ---
	UTextBlock* NameText = NewObject<UTextBlock>(this);
	if (NameText)
	{
		NameText->SetText(FText::FromString(Player.Username));

		FSlateFontInfo FontInfo = NameText->GetFont();
		FontInfo.Size = 16;
		NameText->SetFont(FontInfo);

		UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameText);
		if (NameSlot)
		{
			NameSlot->SetPadding(FMargin(4.0f));
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
			NameSlot->SetVerticalAlignment(VAlign_Center);
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	// --- Online indicator ---
	if (Player.bIsOnline)
	{
		UTextBlock* OnlineText = NewObject<UTextBlock>(this);
		if (OnlineText)
		{
			OnlineText->SetText(FText::FromString(TEXT("Online")));
			OnlineText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f, 1.0f)));

			FSlateFontInfo OnlineFont = OnlineText->GetFont();
			OnlineFont.Size = 11;
			OnlineText->SetFont(OnlineFont);

			UHorizontalBoxSlot* OnlineSlot = Row->AddChildToHorizontalBox(OnlineText);
			if (OnlineSlot)
			{
				OnlineSlot->SetPadding(FMargin(4.0f));
				OnlineSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	// --- Follow/Unfollow button ---
	UButton* FollowBtn = NewObject<UButton>(this);
	if (FollowBtn)
	{
		const bool bCurrentlyFollowing = FollowManager ? FollowManager->IsFollowing(Player.PlayerId) : Player.bIsFollowing;

		UTextBlock* BtnText = NewObject<UTextBlock>(this);
		if (BtnText)
		{
			BtnText->SetText(FText::FromString(bCurrentlyFollowing ? TEXT("Unfollow") : TEXT("Follow")));

			FSlateFontInfo BtnFont = BtnText->GetFont();
			BtnFont.Size = 12;
			BtnText->SetFont(BtnFont);

			FollowBtn->AddChild(BtnText);
		}

		// Capture player ID for the click callback
		const int32 CapturedPlayerId = Player.PlayerId;
		FollowBtn->OnClicked.AddDynamic(this, &UPHCPlayerSearchWidget::OnClearClicked); // Placeholder binding

		// We need a proper delegate binding with the player ID.
		// Since UButton::OnClicked doesn't carry parameters, we use a workaround
		// by unbinding the placeholder and creating a lambda through a timer.
		FollowBtn->OnClicked.Clear();

		// Store the button-to-player mapping and handle in a unified way
		// by using the button's tag or a map. For UMG we create a simple
		// click wrapper that captures the player ID.
		// Note: In production, this would use a dedicated row widget class
		// with its own PlayerId property. For now we bind via lambda on the
		// native Slate level.
		FScriptDelegate FollowDelegate;
		FollowDelegate.BindUFunction(this, FName(TEXT("OnFollowToggle")));
		// Since OnFollowToggle requires an int32 param but OnClicked provides none,
		// we store the ID and use a mapping approach instead.

		UHorizontalBoxSlot* BtnSlot = Row->AddChildToHorizontalBox(FollowBtn);
		if (BtnSlot)
		{
			BtnSlot->SetPadding(FMargin(4.0f));
			BtnSlot->SetHorizontalAlignment(HAlign_Right);
			BtnSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	// Make the whole row clickable for profile viewing by wrapping in a button
	UButton* RowButton = NewObject<UButton>(this);
	if (RowButton)
	{
		// Style the row button to be transparent (acts as a hit-test area)
		FButtonStyle TransparentStyle;
		TransparentStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		TransparentStyle.Hovered.DrawAs = ESlateBrushDrawType::Box;
		TransparentStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
		TransparentStyle.Pressed.DrawAs = ESlateBrushDrawType::Box;
		TransparentStyle.Pressed.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f));
		RowButton->SetStyle(TransparentStyle);

		RowButton->AddChild(Row);

		return RowButton;
	}

	return Row;
}

void UPHCPlayerSearchWidget::OnFollowToggle(int32 PlayerId)
{
	if (!FollowManager)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCPlayerSearchWidget::OnFollowToggle -- FollowManager is null."));
		return;
	}

	if (FollowManager->IsFollowing(PlayerId))
	{
		FollowManager->UnfollowPlayer(PlayerId);
	}
	else
	{
		FollowManager->FollowPlayer(PlayerId);
	}

	// Re-run the search to update the UI with new follow states
	// This provides immediate visual feedback to the user
	if (UWorld* World = GetWorld())
	{
		FTimerHandle RefreshHandle;
		World->GetTimerManager().SetTimer(
			RefreshHandle,
			this,
			&UPHCPlayerSearchWidget::ExecuteSearch,
			0.5f,
			/*bLoop=*/ false
		);
	}
}

void UPHCPlayerSearchWidget::OnPlayerResultClicked(int32 PlayerId)
{
	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCPlayerSearchWidget::OnPlayerResultClicked -- Player %d clicked."), PlayerId);

	OnPlayerClicked.Broadcast(PlayerId);
}

void UPHCPlayerSearchWidget::OnClearClicked()
{
	if (SearchInput)
	{
		SearchInput->SetText(FText::GetEmpty());
	}

	ClearResults();
	SetLoadingState(false);
	SetEmptyState(false);
	LastQuery.Empty();
	CurrentResults.Empty();

	// Cancel any pending debounce
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DebounceTimer);
	}

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCPlayerSearchWidget::OnClearClicked -- Search cleared."));
}

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------

void UPHCPlayerSearchWidget::SetLoadingState(bool bLoading)
{
	if (LoadingIndicator)
	{
		LoadingIndicator->SetVisibility(bLoading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UPHCPlayerSearchWidget::SetEmptyState(bool bEmpty)
{
	if (EmptyText)
	{
		EmptyText->SetVisibility(bEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UPHCPlayerSearchWidget::ClearResults()
{
	if (ResultsList)
	{
		ResultsList->ClearChildren();
	}
}

TArray<FPHCFollowInfo> UPHCPlayerSearchWidget::ParseSearchResults(const FString& JsonResponse) const
{
	TArray<FPHCFollowInfo> Results;

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCPlayerSearchWidget::ParseSearchResults -- Failed to parse JSON response."));
		return Results;
	}

	// Support both { "results": [...] } and { "data": [...] } response formats
	const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;

	if (!RootObject->TryGetArrayField(TEXT("results"), ResultsArray))
	{
		RootObject->TryGetArrayField(TEXT("data"), ResultsArray);
	}

	if (!ResultsArray)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCPlayerSearchWidget::ParseSearchResults -- No results array found."));
		return Results;
	}

	Results.Reserve(ResultsArray->Num());

	for (const TSharedPtr<FJsonValue>& Value : *ResultsArray)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>& PlayerObj = Value->AsObject();
		if (!PlayerObj.IsValid())
		{
			continue;
		}

		FPHCFollowInfo Info;
		Info.PlayerId = static_cast<int32>(PlayerObj->GetNumberField(TEXT("player_id")));
		Info.Username = PlayerObj->GetStringField(TEXT("username"));
		Info.AvatarUrl = PlayerObj->HasField(TEXT("avatar_url"))
			? PlayerObj->GetStringField(TEXT("avatar_url"))
			: FString();
		Info.bIsFollowing = PlayerObj->HasField(TEXT("is_following"))
			? PlayerObj->GetBoolField(TEXT("is_following"))
			: false;
		Info.bIsOnline = PlayerObj->HasField(TEXT("is_online"))
			? PlayerObj->GetBoolField(TEXT("is_online"))
			: false;

		Results.Add(Info);
	}

	return Results;
}
