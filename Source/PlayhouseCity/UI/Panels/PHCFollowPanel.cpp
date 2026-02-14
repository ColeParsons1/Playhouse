// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Panels/PHCFollowPanel.h"
#include "UI/PHCMainHUD.h"

#include "Components/ListView.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCFollowPanel, Log, All);

// ===================================================================
//  Follow List Entry Widget
// ===================================================================

void UPHCFollowListEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	CachedItem = Cast<UPHCFollowListItem>(ListItemObject);
	if (!CachedItem)
	{
		return;
	}

	const FPHCFollowInfo& Info = CachedItem->FollowInfo;

	if (UsernameText)
	{
		UsernameText->SetText(FText::FromString(Info.Username));
	}
	if (OnlineStatusText)
	{
		OnlineStatusText->SetText(FText::FromString(Info.bIsOnline ? TEXT("Online") : TEXT("Offline")));
		OnlineStatusText->SetColorAndOpacity(FSlateColor(
			Info.bIsOnline ? FLinearColor::Green : FLinearColor(0.5f, 0.5f, 0.5f)));
	}
	if (FollowToggleLabel)
	{
		FollowToggleLabel->SetText(FText::FromString(Info.bIsFollowing ? TEXT("Unfollow") : TEXT("Follow")));
	}

	// Bind the toggle button if not already bound.
	if (FollowToggleButton && !FollowToggleButton->OnClicked.IsBound())
	{
		FollowToggleButton->OnClicked.AddDynamic(this, &UPHCFollowListEntryWidget::OnFollowToggleClicked);
	}
}

void UPHCFollowListEntryWidget::OnFollowToggleClicked()
{
	if (!CachedItem)
	{
		return;
	}

	const bool bNewFollow = !CachedItem->FollowInfo.bIsFollowing;
	OnFollowToggleEntry.ExecuteIfBound(CachedItem->FollowInfo.PlayerId, bNewFollow);
}

// ===================================================================
//  Follow Panel
// ===================================================================

UPHCFollowPanel::UPHCFollowPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPHCFollowPanel::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult) return false;

	if (!FollowersButton)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bg"));
		Bg->SetBrushColor(FLinearColor(0.051f, 0.051f, 0.102f, 1.0f));
		Bg->SetPadding(FMargin(12.f));

		UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Bg);
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FollowVBox"));
		Bg->SetContent(VBox);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleLabel"));
		Title->SetText(FText::FromString(TEXT("Players")));
		FSlateFontInfo Font = Title->GetFont();
		Font.Size = 24;
		Title->SetFont(Font);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(Title);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);

		// Scrollable player list
		FollowScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FollowScrollBox"));
		UVerticalBoxSlot* ScrollSlot = VBox->AddChildToVerticalBox(FollowScrollBox);
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	return true;
}

void UPHCFollowPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind tab buttons.
	if (FollowersButton)
	{
		FollowersButton->OnClicked.AddDynamic(this, &UPHCFollowPanel::OnFollowersButtonClicked);
	}
	if (FollowingButton)
	{
		FollowingButton->OnClicked.AddDynamic(this, &UPHCFollowPanel::OnFollowingButtonClicked);
	}
	if (PlayerListView)
	{
		PlayerListView->OnItemClicked().AddUObject(this, &UPHCFollowPanel::OnPlayerClicked);
	}

	// Default to followers tab.
	ShowFollowers();

	UE_LOG(LogPHCFollowPanel, Verbose, TEXT("FollowPanel constructed"));
}

// ===================================================================
//  Public API
// ===================================================================

void UPHCFollowPanel::ShowFollowers()
{
	if (bIsLoading)
	{
		return;
	}

	ActiveTab = EPHCFollowTab::Followers;
	UpdateTabStyles();
	bIsLoading = true;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://tfplayhouse.com/api/players/"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// TODO: Attach auth token from GameInstance.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPHCFollowPanel::OnFollowersReceived);
	HttpRequest->ProcessRequest();

	UE_LOG(LogPHCFollowPanel, Log, TEXT("Fetching followers list"));
}

void UPHCFollowPanel::ShowFollowing()
{
	if (bIsLoading)
	{
		return;
	}

	ActiveTab = EPHCFollowTab::Following;
	UpdateTabStyles();
	bIsLoading = true;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://tfplayhouse.com/api/players/"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPHCFollowPanel::OnFollowingReceived);
	HttpRequest->ProcessRequest();

	UE_LOG(LogPHCFollowPanel, Log, TEXT("Fetching following list"));
}

void UPHCFollowPanel::OnFollowToggle(int32 PlayerId, bool bFollow)
{
	UE_LOG(LogPHCFollowPanel, Log, TEXT("Follow toggle: PlayerId=%d, bFollow=%s"),
		PlayerId, bFollow ? TEXT("true") : TEXT("false"));

	// Build JSON body.
	TSharedPtr<FJsonObject> JsonPayload = MakeShareable(new FJsonObject);
	JsonPayload->SetNumberField(TEXT("playerId"), PlayerId);
	JsonPayload->SetBoolField(TEXT("follow"), bFollow);

	FString PayloadString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
	FJsonSerializer::Serialize(JsonPayload.ToSharedRef(), Writer);

	const FString Endpoint = bFollow
		? TEXT("https://tfplayhouse.com/api/players/follow/")
		: TEXT("https://tfplayhouse.com/api/players/unfollow/");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Endpoint);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(PayloadString);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPHCFollowPanel::OnFollowToggleResponse);
	HttpRequest->ProcessRequest();
}

// ===================================================================
//  Callbacks
// ===================================================================

void UPHCFollowPanel::OnFollowersButtonClicked()
{
	ShowFollowers();
}

void UPHCFollowPanel::OnFollowingButtonClicked()
{
	ShowFollowing();
}

void UPHCFollowPanel::OnPlayerClicked(UObject* Item)
{
	UPHCFollowListItem* FollowItem = Cast<UPHCFollowListItem>(Item);
	if (!FollowItem)
	{
		return;
	}

	UE_LOG(LogPHCFollowPanel, Log, TEXT("Player clicked: %s (ID: %d)"),
		*FollowItem->FollowInfo.Username, FollowItem->FollowInfo.PlayerId);

	// TODO: Phase 5 - Show player profile popup overlay via MainHUD.
	// UPHCMainHUD* MainHUD = GetMainHUD();
	// if (MainHUD) { MainHUD->ShowPlayerProfilePopup(FollowItem->FollowInfo.PlayerId); }
}

// ===================================================================
//  HTTP Handlers
// ===================================================================

void UPHCFollowPanel::OnFollowersReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bIsLoading = false;

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogPHCFollowPanel, Error, TEXT("Followers fetch failed: no connection."));
		OnFollowDataReceived(TArray<FPHCFollowInfo>());
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();

	if (Code >= 200 && Code < 300)
	{
		TArray<FPHCFollowInfo> Players = ParseFollowList(Body);
		UE_LOG(LogPHCFollowPanel, Log, TEXT("Received %d followers"), Players.Num());
		OnFollowDataReceived(Players);
	}
	else
	{
		UE_LOG(LogPHCFollowPanel, Warning, TEXT("Followers fetch failed. HTTP %d: %s"), Code, *Body);
		OnFollowDataReceived(TArray<FPHCFollowInfo>());
	}
}

void UPHCFollowPanel::OnFollowingReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bIsLoading = false;

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogPHCFollowPanel, Error, TEXT("Following fetch failed: no connection."));
		OnFollowDataReceived(TArray<FPHCFollowInfo>());
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();

	if (Code >= 200 && Code < 300)
	{
		TArray<FPHCFollowInfo> Players = ParseFollowList(Body);
		UE_LOG(LogPHCFollowPanel, Log, TEXT("Received %d following"), Players.Num());
		OnFollowDataReceived(Players);
	}
	else
	{
		UE_LOG(LogPHCFollowPanel, Warning, TEXT("Following fetch failed. HTTP %d: %s"), Code, *Body);
		OnFollowDataReceived(TArray<FPHCFollowInfo>());
	}
}

void UPHCFollowPanel::OnFollowToggleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogPHCFollowPanel, Error, TEXT("Follow toggle request failed: no connection."));
		return;
	}

	const int32 Code = Response->GetResponseCode();
	if (Code >= 200 && Code < 300)
	{
		UE_LOG(LogPHCFollowPanel, Log, TEXT("Follow toggle succeeded. Refreshing list."));
		// Refresh the current sub-tab.
		if (ActiveTab == EPHCFollowTab::Followers)
		{
			ShowFollowers();
		}
		else
		{
			ShowFollowing();
		}
	}
	else
	{
		UE_LOG(LogPHCFollowPanel, Warning, TEXT("Follow toggle failed. HTTP %d: %s"),
			Code, *Response->GetContentAsString());
	}
}

// ===================================================================
//  Parsing
// ===================================================================

TArray<FPHCFollowInfo> UPHCFollowPanel::ParseFollowList(const FString& JsonString) const
{
	TArray<FPHCFollowInfo> Results;

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonArray))
	{
		UE_LOG(LogPHCFollowPanel, Error, TEXT("Failed to parse follow list JSON"));
		return Results;
	}

	for (const TSharedPtr<FJsonValue>& Val : JsonArray)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Val->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		// Skip CPU players.
		bool bIsCpu = false;
		Obj->TryGetBoolField(TEXT("is_cpu"), bIsCpu);
		if (bIsCpu)
		{
			continue;
		}

		FPHCFollowInfo Info;
		Info.PlayerId    = static_cast<int32>(Obj->GetNumberField(TEXT("id")));
		Info.Username    = Obj->GetStringField(TEXT("name"));
		// Server does not expose these fields; use defaults.
		Info.bIsFollowing = false;
		Info.bIsOnline    = false;

		Results.Add(Info);
	}

	return Results;
}

// ===================================================================
//  Data Population
// ===================================================================

void UPHCFollowPanel::OnFollowDataReceived(const TArray<FPHCFollowInfo>& Players)
{
	DisplayedItems.Empty();

	for (const FPHCFollowInfo& Info : Players)
	{
		DisplayedItems.Add(UPHCFollowListItem::Create(this, Info));
	}

	if (PlayerListView)
	{
		PlayerListView->ClearListItems();
		for (UPHCFollowListItem* Item : DisplayedItems)
		{
			PlayerListView->AddItem(Item);
		}
	}
	else
	{
		PopulateFollowScrollBox(Players);
	}

	UpdateEmptyState();
}

// ===================================================================
//  UI Helpers
// ===================================================================

void UPHCFollowPanel::UpdateTabStyles()
{
	// Update the tab indicator switcher if available.
	if (TabIndicator)
	{
		TabIndicator->SetActiveWidgetIndex(static_cast<int32>(ActiveTab));
	}

	// Active button gets full opacity, inactive dimmed.
	const auto ApplyStyle = [](UButton* Btn, bool bActive)
	{
		if (!Btn)
		{
			return;
		}
		FLinearColor Tint = bActive
			? FLinearColor(1.f, 1.f, 1.f, 1.f)
			: FLinearColor(0.6f, 0.6f, 0.6f, 0.8f);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(Tint);
		Btn->SetStyle(Style);
	};

	ApplyStyle(FollowersButton, ActiveTab == EPHCFollowTab::Followers);
	ApplyStyle(FollowingButton, ActiveTab == EPHCFollowTab::Following);
}

void UPHCFollowPanel::UpdateEmptyState()
{
	if (!EmptyText)
	{
		return;
	}

	const bool bEmpty = DisplayedItems.Num() == 0;
	EmptyText->SetVisibility(bEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bEmpty)
	{
		if (ActiveTab == EPHCFollowTab::Followers)
		{
			EmptyText->SetText(FText::FromString(TEXT("You don't have any followers yet.")));
		}
		else
		{
			EmptyText->SetText(FText::FromString(TEXT("You're not following anyone yet.")));
		}
	}
}

void UPHCFollowPanel::PopulateFollowScrollBox(const TArray<FPHCFollowInfo>& Players)
{
	if (!FollowScrollBox)
	{
		return;
	}

	FollowScrollBox->ClearChildren();

	for (const FPHCFollowInfo& Info : Players)
	{
		UBorder* RowBorder = NewObject<UBorder>(this);
		RowBorder->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.20f, 1.f));
		RowBorder->SetPadding(FMargin(12.f, 10.f));

		UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);
		RowBorder->SetContent(HBox);

		// Player name.
		UTextBlock* NameText = NewObject<UTextBlock>(this);
		NameText->SetText(FText::FromString(Info.Username));
		FSlateFontInfo NameFont = NameText->GetFont();
		NameFont.Size = 14;
		NameText->SetFont(NameFont);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UHorizontalBoxSlot* NameSlot = HBox->AddChildToHorizontalBox(NameText);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);

		// Player ID on the right.
		UTextBlock* IdText = NewObject<UTextBlock>(this);
		IdText->SetText(FText::FromString(FString::Printf(TEXT("ID: %d"), Info.PlayerId)));
		FSlateFontInfo IdFont = IdText->GetFont();
		IdFont.Size = 10;
		IdText->SetFont(IdFont);
		IdText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.7f)));
		UHorizontalBoxSlot* IdSlot = HBox->AddChildToHorizontalBox(IdText);
		IdSlot->SetVerticalAlignment(VAlign_Center);

		// Spacer between rows.
		USizeBox* Spacer = NewObject<USizeBox>(this);
		Spacer->SetMinDesiredHeight(6.f);

		FollowScrollBox->AddChild(RowBorder);
		FollowScrollBox->AddChild(Spacer);
	}
}

UPHCMainHUD* UPHCFollowPanel::GetMainHUD() const
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
