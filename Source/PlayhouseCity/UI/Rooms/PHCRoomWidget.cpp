// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Rooms/PHCRoomWidget.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"

#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"

#include "WebSocketsModule.h"
#include "IWebSocket.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCRoomWidget::UPHCRoomWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCRoomWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button callbacks.
	if (SendButton)
	{
		SendButton->OnClicked.AddDynamic(this, &UPHCRoomWidget::OnSendClicked);
	}
	if (MuteToggle)
	{
		MuteToggle->OnClicked.AddDynamic(this, &UPHCRoomWidget::OnMuteToggled);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked.AddDynamic(this, &UPHCRoomWidget::OnLeaveClicked);
	}
	if (GameLaunchButton)
	{
		GameLaunchButton->OnClicked.AddDynamic(this, &UPHCRoomWidget::OnGameLaunchClicked);
	}
	if (ChatInput)
	{
		ChatInput->OnTextCommitted.AddDynamic(this, &UPHCRoomWidget::OnChatSubmitted);
	}
}

void UPHCRoomWidget::NativeDestruct()
{
	DisconnectWebSocket();
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void UPHCRoomWidget::JoinRoom(int32 RoomId)
{
	CurrentRoomId = RoomId;

	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCRoom: HttpClient unavailable."));
		return;
	}

	// Fetch room details.
	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCRoomWidget::OnRoomDetailsReceived);
	Http->GetRoomDetails(RoomId, Callback);
}

void UPHCRoomWidget::LeaveRoom()
{
	DisconnectWebSocket();

	UPHCHttpClient* Http = GetHttpClient();
	if (Http && CurrentRoomId > 0)
	{
		FPHCJsonResponseDynamic Callback;
		Callback.BindDynamic(this, &UPHCRoomWidget::OnLeaveRoomResult);
		Http->LeaveRoom(CurrentRoomId, Callback);
	}
	else
	{
		// No HTTP client; just clean up locally.
		CurrentRoomId = 0;
		OnRoomLeft.Broadcast();
		RemoveFromParent();
	}
}

void UPHCRoomWidget::UpdateParticipants(const TArray<FPHCPlayerProfile>& Players)
{
	if (!ParticipantsList)
	{
		return;
	}

	ParticipantsList->ClearChildren();
	ParticipantPlayerIds.Empty(Players.Num());

	for (const FPHCPlayerProfile& Player : Players)
	{
		ParticipantPlayerIds.Add(Player.PlayerId);

		// Create a simple text entry for each participant.
		UTextBlock* NameText = NewObject<UTextBlock>(this);
		if (NameText)
		{
			const FString DisplayStr = FString::Printf(TEXT("%s (Lv.%d)"), *Player.DisplayName, Player.Level);
			NameText->SetText(FText::FromString(DisplayStr));

			FSlateFontInfo FontInfo = NameText->GetFont();
			FontInfo.Size = 12;
			NameText->SetFont(FontInfo);

			ParticipantsList->AddChild(NameText);
		}
	}

	// Update count display.
	if (ParticipantCountText)
	{
		const FString CountStr = FString::Printf(TEXT("%d / %d"), Players.Num(), RoomInfo.MaxPlayers);
		ParticipantCountText->SetText(FText::FromString(CountStr));
	}
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------

void UPHCRoomWidget::OnChatReceived(const FPHCChatMessage& Message)
{
	// Enforce history limit.
	if (ChatHistory.Num() >= MaxChatHistory)
	{
		ChatHistory.RemoveAt(0);
	}
	ChatHistory.Add(Message);

	AddChatBubble(Message);
}

void UPHCRoomWidget::OnSendClicked()
{
	if (!ChatInput)
	{
		return;
	}

	const FText InputText = ChatInput->GetText();
	if (InputText.IsEmpty())
	{
		return;
	}

	SendChatMessage(InputText.ToString());
	ChatInput->SetText(FText::GetEmpty());
}

void UPHCRoomWidget::OnChatSubmitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}

	if (Text.IsEmpty())
	{
		return;
	}

	SendChatMessage(Text.ToString());

	if (ChatInput)
	{
		ChatInput->SetText(FText::GetEmpty());
	}
}

void UPHCRoomWidget::OnPlayerJoined(int32 PlayerId, const FString& Name)
{
	if (ParticipantPlayerIds.Contains(PlayerId))
	{
		return; // Already listed.
	}

	ParticipantPlayerIds.Add(PlayerId);

	if (ParticipantsList)
	{
		UTextBlock* NameText = NewObject<UTextBlock>(this);
		if (NameText)
		{
			NameText->SetText(FText::FromString(Name));
			FSlateFontInfo FontInfo = NameText->GetFont();
			FontInfo.Size = 12;
			NameText->SetFont(FontInfo);
			ParticipantsList->AddChild(NameText);
		}
	}

	// Update count.
	RoomInfo.PlayerCount = ParticipantPlayerIds.Num();
	if (ParticipantCountText)
	{
		const FString CountStr = FString::Printf(TEXT("%d / %d"), RoomInfo.PlayerCount, RoomInfo.MaxPlayers);
		ParticipantCountText->SetText(FText::FromString(CountStr));
	}

	// Add a system chat message.
	FPHCChatMessage JoinMsg;
	JoinMsg.SenderName = TEXT("System");
	JoinMsg.SenderId = 0;
	JoinMsg.Message = FString::Printf(TEXT("%s joined the room."), *Name);
	JoinMsg.Timestamp = FDateTime::UtcNow().ToIso8601();
	OnChatReceived(JoinMsg);
}

void UPHCRoomWidget::OnPlayerLeft(int32 PlayerId)
{
	const int32 Index = ParticipantPlayerIds.IndexOfByKey(PlayerId);
	if (Index == INDEX_NONE)
	{
		return;
	}

	ParticipantPlayerIds.RemoveAt(Index);

	// Rebuild participant list (simplest approach).
	if (ParticipantsList && ParticipantsList->GetChildrenCount() > Index)
	{
		ParticipantsList->RemoveChildAt(Index);
	}

	RoomInfo.PlayerCount = ParticipantPlayerIds.Num();
	if (ParticipantCountText)
	{
		const FString CountStr = FString::Printf(TEXT("%d / %d"), RoomInfo.PlayerCount, RoomInfo.MaxPlayers);
		ParticipantCountText->SetText(FText::FromString(CountStr));
	}
}

void UPHCRoomWidget::OnMuteToggled()
{
	bIsMuted = !bIsMuted;
	UE_LOG(LogTemp, Log, TEXT("PHCRoom: Mute toggled. Now %s."), bIsMuted ? TEXT("muted") : TEXT("unmuted"));

	// Visual feedback on the mute button.
	if (MuteToggle)
	{
		const FLinearColor Color = bIsMuted
			? FLinearColor(1.0f, 0.3f, 0.3f, 1.0f)
			: FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		MuteToggle->SetColorAndOpacity(Color);
	}
}

void UPHCRoomWidget::OnGameLaunchClicked()
{
	UE_LOG(LogTemp, Log, TEXT("PHCRoom: Game launch requested for room %d."), CurrentRoomId);
	// In production, this opens a game-selection popup overlay and then sends
	// a WebSocket message to start the selected game for all room participants.
}

void UPHCRoomWidget::OnLeaveClicked()
{
	// In production, show a confirmation dialog. For now, leave immediately.
	UE_LOG(LogTemp, Log, TEXT("PHCRoom: Leave requested for room %d."), CurrentRoomId);
	LeaveRoom();
}

void UPHCRoomWidget::AddChatBubble(const FPHCChatMessage& Msg)
{
	if (!ChatScrollBox || bIsMuted)
	{
		return;
	}

	// Build a horizontal box: [SenderName]: [Message]
	UHorizontalBox* BubbleRow = NewObject<UHorizontalBox>(this);
	if (!BubbleRow)
	{
		return;
	}

	// Sender name.
	UTextBlock* SenderText = NewObject<UTextBlock>(BubbleRow);
	if (SenderText)
	{
		SenderText->SetText(FText::FromString(Msg.SenderName + TEXT(": ")));
		FSlateFontInfo FontInfo = SenderText->GetFont();
		FontInfo.Size = 12;
		SenderText->SetFont(FontInfo);

		// System messages in a distinct color.
		const FSlateColor TextColor = (Msg.SenderId == 0)
			? FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			: FSlateColor(FLinearColor(0.3f, 0.8f, 1.0f, 1.0f));
		SenderText->SetColorAndOpacity(TextColor);

		BubbleRow->AddChild(SenderText);
	}

	// Message body.
	UTextBlock* MsgText = NewObject<UTextBlock>(BubbleRow);
	if (MsgText)
	{
		MsgText->SetText(FText::FromString(Msg.Message));
		FSlateFontInfo FontInfo = MsgText->GetFont();
		FontInfo.Size = 12;
		MsgText->SetFont(FontInfo);
		MsgText->SetAutoWrapText(true);

		BubbleRow->AddChild(MsgText);
	}

	ChatScrollBox->AddChild(BubbleRow);
	ChatScrollBox->ScrollToEnd();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UPHCRoomWidget::ConnectWebSocket()
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (!GI)
	{
		return;
	}

	const FString WsUrl = FString::Printf(TEXT("%sroom/%d/?token=%s"),
		*GI->WebSocketUrl,
		CurrentRoomId,
		*GI->AuthToken);

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
	{
		FModuleManager::Get().LoadModule(TEXT("WebSockets"));
	}

	WebSocket = FWebSocketsModule::Get().CreateWebSocket(WsUrl, TEXT("ws"));
	if (!WebSocket.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("PHCRoom: Failed to create WebSocket for room %d."), CurrentRoomId);
		return;
	}

	WebSocket->OnMessage().AddLambda([this](const FString& Message)
	{
		HandleWebSocketMessage(Message);
	});

	WebSocket->OnConnected().AddLambda([this]()
	{
		UE_LOG(LogTemp, Log, TEXT("PHCRoom: WebSocket connected for room %d."), CurrentRoomId);
	});

	WebSocket->OnConnectionError().AddLambda([this](const FString& Error)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCRoom: WebSocket connection error: %s"), *Error);
	});

	WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		UE_LOG(LogTemp, Log, TEXT("PHCRoom: WebSocket closed. Code=%d Reason=%s"), StatusCode, *Reason);
	});

	WebSocket->Connect();
}

void UPHCRoomWidget::DisconnectWebSocket()
{
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		WebSocket->Close();
	}
	WebSocket.Reset();
}

void UPHCRoomWidget::HandleWebSocketMessage(const FString& MessageString)
{
	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCRoom: Failed to parse WebSocket message."));
		return;
	}

	const FString Type = JsonObj->GetStringField(TEXT("type"));

	if (Type == TEXT("chat_message"))
	{
		FPHCChatMessage Msg;
		Msg.SenderName = JsonObj->GetStringField(TEXT("sender_name"));
		Msg.SenderId = static_cast<int32>(JsonObj->GetNumberField(TEXT("sender_id")));
		Msg.Message = JsonObj->GetStringField(TEXT("message"));

		FString TimestampStr;
		if (JsonObj->TryGetStringField(TEXT("timestamp"), TimestampStr))
		{
			Msg.Timestamp = TimestampStr;
		}
		else
		{
			Msg.Timestamp = FDateTime::UtcNow().ToIso8601();
		}

		OnChatReceived(Msg);
	}
	else if (Type == TEXT("player_joined"))
	{
		const int32 PlayerId = static_cast<int32>(JsonObj->GetNumberField(TEXT("player_id")));
		const FString Name = JsonObj->GetStringField(TEXT("player_name"));
		OnPlayerJoined(PlayerId, Name);
	}
	else if (Type == TEXT("player_left"))
	{
		const int32 PlayerId = static_cast<int32>(JsonObj->GetNumberField(TEXT("player_id")));
		OnPlayerLeft(PlayerId);
	}
	else if (Type == TEXT("game_started"))
	{
		const FString GameTypeStr = JsonObj->GetStringField(TEXT("game_type"));
		UE_LOG(LogTemp, Log, TEXT("PHCRoom: Game started: %s"), *GameTypeStr);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("PHCRoom: Unhandled WebSocket message type: %s"), *Type);
	}
}

void UPHCRoomWidget::OnRoomDetailsReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCRoom: Failed to get room details. Status=%d"), StatusCode);
		return;
	}

	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCRoom: Failed to parse room details JSON."));
		return;
	}

	// Populate RoomInfo.
	RoomInfo.RoomId = CurrentRoomId;
	RoomInfo.RoomName = JsonObj->GetStringField(TEXT("name"));
	RoomInfo.HostName = JsonObj->GetStringField(TEXT("host_name"));
	RoomInfo.HostId = static_cast<int32>(JsonObj->GetNumberField(TEXT("host_id")));
	RoomInfo.ThemeId = JsonObj->GetStringField(TEXT("theme"));
	RoomInfo.PlayerCount = static_cast<int32>(JsonObj->GetNumberField(TEXT("player_count")));
	RoomInfo.MaxPlayers = static_cast<int32>(JsonObj->GetNumberField(TEXT("max_players")));

	// Update UI.
	if (RoomNameText)
	{
		RoomNameText->SetText(FText::FromString(RoomInfo.RoomName));
	}
	if (ThemeIndicator)
	{
		// Set theme color based on theme ID.
		FLinearColor ThemeColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);
		if (RoomInfo.ThemeId == TEXT("sunset"))
		{
			ThemeColor = FLinearColor(1.0f, 0.5f, 0.2f, 1.0f);
		}
		else if (RoomInfo.ThemeId == TEXT("forest"))
		{
			ThemeColor = FLinearColor(0.2f, 0.7f, 0.3f, 1.0f);
		}
		else if (RoomInfo.ThemeId == TEXT("ocean"))
		{
			ThemeColor = FLinearColor(0.1f, 0.4f, 0.8f, 1.0f);
		}
		ThemeIndicator->SetBrushColor(ThemeColor);
	}
	if (ParticipantCountText)
	{
		const FString CountStr = FString::Printf(TEXT("%d / %d"), RoomInfo.PlayerCount, RoomInfo.MaxPlayers);
		ParticipantCountText->SetText(FText::FromString(CountStr));
	}

	// Parse participant list if included.
	const TArray<TSharedPtr<FJsonValue>>* PlayersArray = nullptr;
	if (JsonObj->TryGetArrayField(TEXT("participants"), PlayersArray) && PlayersArray)
	{
		TArray<FPHCPlayerProfile> Players;
		for (const TSharedPtr<FJsonValue>& Val : *PlayersArray)
		{
			const TSharedPtr<FJsonObject>* PlayerObj = nullptr;
			if (Val->TryGetObject(PlayerObj) && PlayerObj && (*PlayerObj).IsValid())
			{
				FPHCPlayerProfile Prof;
				Prof.PlayerId = static_cast<int32>((*PlayerObj)->GetNumberField(TEXT("id")));
				Prof.DisplayName = (*PlayerObj)->GetStringField(TEXT("display_name"));
				Prof.Level = static_cast<int32>((*PlayerObj)->GetNumberField(TEXT("level")));
				Players.Add(Prof);
			}
		}
		UpdateParticipants(Players);
	}

	// Connect the WebSocket now that we have room details.
	ConnectWebSocket();

	// Call the join API.
	UPHCHttpClient* Http = GetHttpClient();
	if (Http)
	{
		FPHCJsonResponseDynamic JoinCallback;
		// Fire-and-forget join; WebSocket will handle real-time updates.
		Http->JoinRoom(CurrentRoomId, FString(), JoinCallback);
	}
}

void UPHCRoomWidget::OnLeaveRoomResult(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("PHCRoom: Successfully left room %d."), CurrentRoomId);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCRoom: Leave room API failed. Status=%d"), StatusCode);
	}

	CurrentRoomId = 0;
	OnRoomLeft.Broadcast();
	RemoveFromParent();
}

void UPHCRoomWidget::SendChatMessage(const FString& MessageText)
{
	if (!WebSocket.IsValid() || !WebSocket->IsConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCRoom: Cannot send chat - WebSocket not connected."));
		return;
	}

	// Build JSON message.
	TSharedRef<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetStringField(TEXT("type"), TEXT("chat_message"));
	JsonObj->SetStringField(TEXT("message"), MessageText);

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObj, Writer);

	WebSocket->Send(OutputString);

	// Add to local chat immediately (optimistic UI).
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	FPHCChatMessage LocalMsg;
	LocalMsg.SenderName = GI ? GI->LocalPlayerProfile.DisplayName : TEXT("Me");
	LocalMsg.SenderId = GI ? GI->LocalPlayerProfile.PlayerId : 0;
	LocalMsg.Message = MessageText;
	LocalMsg.Timestamp = FDateTime::UtcNow().ToIso8601();
	OnChatReceived(LocalMsg);
}

UPHCHttpClient* UPHCRoomWidget::GetHttpClient() const
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	return GI ? GI->GetHttpClient() : nullptr;
}
