// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Panels/PHCRoomsPanel.h"
#include "UI/PHCMainHUD.h"

#include "Components/ListView.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
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
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCRoomsPanel, Log, All);

// ===================================================================
//  Room List Entry Widget
// ===================================================================

void UPHCRoomListEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	CachedItem = Cast<UPHCRoomListItem>(ListItemObject);
	if (!CachedItem)
	{
		return;
	}

	const FPHCRoomInfo& Info = CachedItem->RoomInfo;

	if (RoomNameText)
	{
		RoomNameText->SetText(FText::FromString(Info.RoomName));
	}
	if (HostNameText)
	{
		HostNameText->SetText(FText::FromString(Info.HostName));
	}
	if (PlayerCountText)
	{
		PlayerCountText->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), Info.PlayerCount, Info.MaxPlayers)));
	}
	if (StatusBadge)
	{
		const UEnum* StatusEnum = StaticEnum<EPHCRoomStatus>();
		FString StatusStr = StatusEnum
			? StatusEnum->GetDisplayNameTextByIndex(static_cast<int32>(Info.Status)).ToString()
			: TEXT("Unknown");
		StatusBadge->SetText(FText::FromString(StatusStr));

		// Color code the status badge.
		FSlateColor BadgeColor;
		switch (Info.Status)
		{
		case EPHCRoomStatus::Open:   BadgeColor = FSlateColor(FLinearColor::Green);  break;
		case EPHCRoomStatus::InGame: BadgeColor = FSlateColor(FLinearColor(1.f, 0.8f, 0.f)); break;
		case EPHCRoomStatus::Full:   BadgeColor = FSlateColor(FLinearColor(1.f, 0.5f, 0.f)); break;
		case EPHCRoomStatus::Closed: BadgeColor = FSlateColor(FLinearColor::Red);    break;
		default:                     BadgeColor = FSlateColor(FLinearColor::White);   break;
		}
		StatusBadge->SetColorAndOpacity(BadgeColor);
	}
}

// ===================================================================
//  Rooms Panel
// ===================================================================

UPHCRoomsPanel::UPHCRoomsPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPHCRoomsPanel::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult) return false;

	if (!RoomListView)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bg"));
		Bg->SetBrushColor(FLinearColor(0.051f, 0.051f, 0.102f, 1.0f));
		Bg->SetPadding(FMargin(12.f));

		UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Bg);
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RoomsVBox"));
		Bg->SetContent(VBox);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleLabel"));
		Title->SetText(FText::FromString(TEXT("Rooms")));
		FSlateFontInfo Font = Title->GetFont();
		Font.Size = 24;
		Title->SetFont(Font);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(Title);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);

		// Scrollable room list
		RoomScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RoomScrollBox"));
		UVerticalBoxSlot* ScrollSlot = VBox->AddChildToVerticalBox(RoomScrollBox);
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	return true;
}

void UPHCRoomsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// --- Bind callbacks ---
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &UPHCRoomsPanel::OnRefreshClicked);
	}
	if (SearchInput)
	{
		SearchInput->OnTextChanged.AddDynamic(this, &UPHCRoomsPanel::OnSearchChanged);
	}
	if (FilterCombo)
	{
		FilterCombo->ClearOptions();
		const UEnum* FilterEnum = StaticEnum<EPHCRoomFilter>();
		if (FilterEnum)
		{
			for (int32 i = 0; i < FilterEnum->NumEnums() - 1; ++i) // skip MAX
			{
				FilterCombo->AddOption(FilterEnum->GetDisplayNameTextByIndex(i).ToString());
			}
		}
		FilterCombo->SetSelectedIndex(0);
		FilterCombo->OnSelectionChanged.AddDynamic(this, &UPHCRoomsPanel::OnFilterChanged);
	}
	if (RoomListView)
	{
		RoomListView->OnItemClicked().AddUObject(this, &UPHCRoomsPanel::OnRoomClicked);
	}

	// Start initial load.
	RefreshRoomList();

	UE_LOG(LogPHCRoomsPanel, Verbose, TEXT("RoomsPanel constructed"));
}

// ===================================================================
//  Public API
// ===================================================================

void UPHCRoomsPanel::RefreshRoomList()
{
	if (bIsLoading)
	{
		return;
	}

	SetLoading(true);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://tfplayhouse.com/api/rooms/"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// TODO: Attach auth token from GameInstance.
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UPHCRoomsPanel::OnRoomDataReceived);
	HttpRequest->ProcessRequest();

	UE_LOG(LogPHCRoomsPanel, Log, TEXT("Room list refresh requested"));
}

// ===================================================================
//  Callbacks
// ===================================================================

void UPHCRoomsPanel::OnRefreshClicked()
{
	RefreshRoomList();
}

void UPHCRoomsPanel::OnSearchChanged(const FText& Text)
{
	CurrentSearchText = Text.ToString();
	ApplyFilters();
}

void UPHCRoomsPanel::OnFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const UEnum* FilterEnum = StaticEnum<EPHCRoomFilter>();
	if (FilterEnum)
	{
		for (int32 i = 0; i < FilterEnum->NumEnums() - 1; ++i)
		{
			if (FilterEnum->GetDisplayNameTextByIndex(i).ToString() == SelectedItem)
			{
				CurrentFilter = static_cast<EPHCRoomFilter>(i);
				break;
			}
		}
	}
	ApplyFilters();
}

void UPHCRoomsPanel::OnRoomClicked(UObject* Item)
{
	UPHCRoomListItem* RoomItem = Cast<UPHCRoomListItem>(Item);
	if (!RoomItem)
	{
		return;
	}

	UE_LOG(LogPHCRoomsPanel, Log, TEXT("Room clicked: %s (ID: %d)"),
		*RoomItem->RoomInfo.RoomName, RoomItem->RoomInfo.RoomId);

	UPHCMainHUD* MainHUD = GetMainHUD();
	if (MainHUD)
	{
		MainHUD->NavigateToRoom(RoomItem->RoomInfo.RoomId);
	}
}

// ===================================================================
//  HTTP
// ===================================================================

void UPHCRoomsPanel::OnRoomDataReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	SetLoading(false);

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogPHCRoomsPanel, Error, TEXT("Room list fetch failed: no connection."));
		AllRooms.Empty();
		ApplyFilters();
		return;
	}

	const int32 Code = Response->GetResponseCode();
	const FString Body = Response->GetContentAsString();

	if (Code >= 200 && Code < 300)
	{
		AllRooms = ParseRoomList(Body);
		UE_LOG(LogPHCRoomsPanel, Log, TEXT("Received %d rooms"), AllRooms.Num());
	}
	else
	{
		UE_LOG(LogPHCRoomsPanel, Warning, TEXT("Room list fetch failed. HTTP %d: %s"), Code, *Body);
		AllRooms.Empty();
	}

	ApplyFilters();
}

// ===================================================================
//  Parsing
// ===================================================================

TArray<FPHCRoomInfo> UPHCRoomsPanel::ParseRoomList(const FString& JsonString) const
{
	TArray<FPHCRoomInfo> Results;

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonArray))
	{
		UE_LOG(LogPHCRoomsPanel, Error, TEXT("Failed to parse room list JSON"));
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
		FPHCRoomInfo Info;
		Info.RoomId      = static_cast<int32>(Obj->GetNumberField(TEXT("id")));
		Info.RoomName    = Obj->GetStringField(TEXT("name"));
		Info.HostName    = Obj->GetStringField(TEXT("host_name"));
		Info.HostId      = static_cast<int32>(Obj->GetNumberField(TEXT("host_id")));
		Info.ThemeId     = Obj->GetStringField(TEXT("theme"));

		// player_count / is_active
		double TmpNum = 0;
		if (Obj->TryGetNumberField(TEXT("player_count"), TmpNum))
		{
			Info.PlayerCount = static_cast<int32>(TmpNum);
		}
		Info.MaxPlayers = 20; // server does not expose maxPlayers; use a sensible default

		bool bActive = true;
		if (Obj->TryGetBoolField(TEXT("is_active"), bActive))
		{
			Info.Status = bActive ? EPHCRoomStatus::Open : EPHCRoomStatus::Closed;
		}

		Results.Add(Info);
	}

	return Results;
}

// ===================================================================
//  Filtering
// ===================================================================

void UPHCRoomsPanel::ApplyFilters()
{
	DisplayedItems.Empty();

	for (const FPHCRoomInfo& Room : AllRooms)
	{
		// --- Status filter ---
		if (CurrentFilter == EPHCRoomFilter::Open && Room.Status != EPHCRoomStatus::Open)
		{
			continue;
		}
		// EPHCRoomFilter::Friends would require a friends list lookup; deferred to Phase 5.

		// --- Search text filter ---
		if (!CurrentSearchText.IsEmpty())
		{
			const bool bNameMatch = Room.RoomName.Contains(CurrentSearchText, ESearchCase::IgnoreCase);
			const bool bHostMatch = Room.HostName.Contains(CurrentSearchText, ESearchCase::IgnoreCase);
			if (!bNameMatch && !bHostMatch)
			{
				continue;
			}
		}

		DisplayedItems.Add(UPHCRoomListItem::Create(this, Room));
	}

	// Update list view or fallback scroll box.
	if (RoomListView)
	{
		RoomListView->ClearListItems();
		for (UPHCRoomListItem* Item : DisplayedItems)
		{
			RoomListView->AddItem(Item);
		}
	}
	else
	{
		PopulateRoomScrollBox();
	}

	UpdateEmptyState();

	UE_LOG(LogPHCRoomsPanel, Verbose, TEXT("Filters applied: showing %d / %d rooms"),
		DisplayedItems.Num(), AllRooms.Num());
}

// ===================================================================
//  UI State Helpers
// ===================================================================

void UPHCRoomsPanel::SetLoading(bool bLoading)
{
	bIsLoading = bLoading;

	if (LoadingIndicator)
	{
		LoadingIndicator->SetVisibility(bLoading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(!bLoading);
	}

	UpdateEmptyState();
}

void UPHCRoomsPanel::UpdateEmptyState()
{
	if (!EmptyStateText)
	{
		return;
	}

	if (bIsLoading)
	{
		EmptyStateText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bEmpty = DisplayedItems.Num() == 0;
	EmptyStateText->SetVisibility(bEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (bEmpty)
	{
		if (!CurrentSearchText.IsEmpty())
		{
			EmptyStateText->SetText(FText::FromString(TEXT("No rooms match your search.")));
		}
		else
		{
			EmptyStateText->SetText(FText::FromString(TEXT("No rooms available. Create one!")));
		}
	}
}

void UPHCRoomsPanel::PopulateRoomScrollBox()
{
	if (!RoomScrollBox)
	{
		return;
	}

	RoomScrollBox->ClearChildren();

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	for (const UPHCRoomListItem* Item : DisplayedItems)
	{
		const FPHCRoomInfo& Room = Item->RoomInfo;

		// Each room row is a Border with a horizontal layout.
		UBorder* RowBorder = NewObject<UBorder>(this);
		RowBorder->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.20f, 1.f));
		RowBorder->SetPadding(FMargin(12.f, 10.f));

		UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);
		RowBorder->SetContent(HBox);

		// Room name + host.
		UVerticalBox* InfoVBox = NewObject<UVerticalBox>(this);
		UHorizontalBoxSlot* InfoSlot = HBox->AddChildToHorizontalBox(InfoVBox);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InfoSlot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* NameText = NewObject<UTextBlock>(this);
		NameText->SetText(FText::FromString(Room.RoomName));
		FSlateFontInfo NameFont = NameText->GetFont();
		NameFont.Size = 14;
		NameText->SetFont(NameFont);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		InfoVBox->AddChildToVerticalBox(NameText);

		UTextBlock* HostText = NewObject<UTextBlock>(this);
		HostText->SetText(FText::FromString(FString::Printf(TEXT("Host: %s"), *Room.HostName)));
		FSlateFontInfo HostFont = HostText->GetFont();
		HostFont.Size = 10;
		HostText->SetFont(HostFont);
		HostText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.7f)));
		InfoVBox->AddChildToVerticalBox(HostText);

		// Player count + status on the right.
		UVerticalBox* RightVBox = NewObject<UVerticalBox>(this);
		UHorizontalBoxSlot* RightSlot = HBox->AddChildToHorizontalBox(RightVBox);
		RightSlot->SetVerticalAlignment(VAlign_Center);
		RightSlot->SetHorizontalAlignment(HAlign_Right);

		UTextBlock* CountText = NewObject<UTextBlock>(this);
		CountText->SetText(FText::FromString(FString::Printf(TEXT("%d players"), Room.PlayerCount)));
		FSlateFontInfo CountFont = CountText->GetFont();
		CountFont.Size = 11;
		CountText->SetFont(CountFont);
		CountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.9f)));
		RightVBox->AddChildToVerticalBox(CountText);

		UTextBlock* StatusText = NewObject<UTextBlock>(this);
		StatusText->SetText(FText::FromString(
			Room.Status == EPHCRoomStatus::Open ? TEXT("Open") : TEXT("Closed")));
		FSlateFontInfo StatusFont = StatusText->GetFont();
		StatusFont.Size = 10;
		StatusText->SetFont(StatusFont);
		StatusText->SetColorAndOpacity(FSlateColor(
			Room.Status == EPHCRoomStatus::Open ? FLinearColor::Green : FLinearColor::Red));
		RightVBox->AddChildToVerticalBox(StatusText);

		// Add spacer between rows.
		USizeBox* Spacer = NewObject<USizeBox>(this);
		Spacer->SetMinDesiredHeight(6.f);

		RoomScrollBox->AddChild(RowBorder);
		RoomScrollBox->AddChild(Spacer);
	}
}

UPHCMainHUD* UPHCRoomsPanel::GetMainHUD() const
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
