// Copyright PlayhouseCity. All Rights Reserved.

#include "Network/PHCWebSocketClient.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Policies/CondensedJsonPrintPolicy.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCWebSocket, Log, All);

// Message type string constants matching the Django Channels consumer.
namespace PHCWsTypes
{
	static const FString Auth          = TEXT("authenticate");
	static const FString Chat          = TEXT("chat_message");
	static const FString PlayerJoined  = TEXT("player_joined");
	static const FString PlayerLeft    = TEXT("player_left");
	static const FString GameAction    = TEXT("game_action");
	static const FString ThemeChanged  = TEXT("theme_changed");
	static const FString MusicSync     = TEXT("music_sync");
	static const FString Ping          = TEXT("ping");
	static const FString Pong          = TEXT("pong");
	static const FString Error         = TEXT("error");
}

// ===================================================================
//  Construction / Destruction
// ===================================================================

UPHCWebSocketClient::UPHCWebSocketClient()
{
}

void UPHCWebSocketClient::BeginDestroy()
{
	Disconnect();
	Super::BeginDestroy();
}

// -------------------------------------------------------------------
//  Setup
// -------------------------------------------------------------------

void UPHCWebSocketClient::Initialize(const FString& BaseWsUrl)
{
	BaseUrl = BaseWsUrl;
	if (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.RemoveFromEnd(TEXT("/"));
	}
	UE_LOG(LogPHCWebSocket, Log, TEXT("WebSocket client initialized with base URL: %s"), *BaseUrl);
}

void UPHCWebSocketClient::SetAuthToken(const FString& Token)
{
	AuthToken = Token;
}

// -------------------------------------------------------------------
//  Connection lifecycle
// -------------------------------------------------------------------

void UPHCWebSocketClient::Connect(int32 RoomId)
{
	// Tear down any existing connection first.
	if (WebSocket.IsValid())
	{
		Disconnect();
	}

	CurrentRoomId = RoomId;
	const FString Url = FString::Printf(TEXT("%s/ws/room/%d/"), *BaseUrl, RoomId);

	UE_LOG(LogPHCWebSocket, Log, TEXT("Connecting to: %s"), *Url);

	// Ensure the WebSockets module is loaded.
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
	{
		FModuleManager::Get().LoadModule(TEXT("WebSockets"));
	}

	// Create socket with standard protocols.
	const FString Protocol = TEXT("ws");
	TMap<FString, FString> Headers;
	if (!AuthToken.IsEmpty())
	{
		// Some backends support token in the upgrade headers.
		Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Token %s"), *AuthToken));
	}

	WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url, Protocol, Headers);

	if (!WebSocket.IsValid())
	{
		UE_LOG(LogPHCWebSocket, Error, TEXT("Failed to create IWebSocket for URL: %s"), *Url);
		const FString Err = TEXT("Failed to create WebSocket object");
		OnError.Broadcast(Err);
		OnErrorNative.Broadcast(Err);
		return;
	}

	// Bind callbacks via lambdas (IWebSocket delegates are native, but this is a UObject).
	WebSocket->OnConnected().AddLambda([this]() { HandleConnected(); });
	WebSocket->OnConnectionError().AddLambda([this](const FString& Error) { HandleConnectionError(Error); });
	WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean) { HandleClosed(StatusCode, Reason, bWasClean); });
	WebSocket->OnMessage().AddLambda([this](const FString& Message) { HandleMessage(Message); });
	WebSocket->OnMessageSent().AddLambda([this](const FString& Message) { HandleMessageSent(Message); });

	WebSocket->Connect();
}

void UPHCWebSocketClient::Disconnect()
{
	StopPingTimer();

	if (WebSocket.IsValid())
	{
		if (WebSocket->IsConnected())
		{
			WebSocket->Close();
		}
		WebSocket.Reset();
	}

	bIsConnected = false;
}

bool UPHCWebSocketClient::IsConnected() const
{
	return bIsConnected && WebSocket.IsValid() && WebSocket->IsConnected();
}

// -------------------------------------------------------------------
//  Outbound messages
// -------------------------------------------------------------------

void UPHCWebSocketClient::SendChatMessage(const FString& Message)
{
	if (!IsConnected())
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("Cannot send chat message: not connected."));
		return;
	}

	TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("type"), PHCWsTypes::Chat);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), Message);
	Msg->SetObjectField(TEXT("data"), Data);

	SendJsonMessage(Msg);
}

void UPHCWebSocketClient::SendGameAction(const FString& ActionType, TSharedPtr<FJsonObject> Payload)
{
	if (!IsConnected())
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("Cannot send game action: not connected."));
		return;
	}

	TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("type"), PHCWsTypes::GameAction);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("action_type"), ActionType);

	if (Payload.IsValid())
	{
		Data->SetObjectField(TEXT("payload"), Payload.ToSharedRef());
	}
	else
	{
		Data->SetObjectField(TEXT("payload"), MakeShared<FJsonObject>());
	}

	Msg->SetObjectField(TEXT("data"), Data);
	SendJsonMessage(Msg);
}

void UPHCWebSocketClient::K2_SendGameAction(const FString& ActionType, const FString& PayloadJson)
{
	TSharedPtr<FJsonObject> PayloadObj;
	if (!PayloadJson.IsEmpty())
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
		FJsonSerializer::Deserialize(Reader, PayloadObj);
	}
	SendGameAction(ActionType, PayloadObj);
}

void UPHCWebSocketClient::SendThemeChange(const FString& ThemeId)
{
	if (!IsConnected())
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("Cannot send theme change: not connected."));
		return;
	}

	TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("type"), PHCWsTypes::ThemeChanged);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("theme_id"), ThemeId);
	Msg->SetObjectField(TEXT("data"), Data);

	SendJsonMessage(Msg);
}

// -------------------------------------------------------------------
//  Internal handlers
// -------------------------------------------------------------------

void UPHCWebSocketClient::HandleConnected()
{
	bIsConnected = true;
	UE_LOG(LogPHCWebSocket, Log, TEXT("WebSocket connected to room %d"), CurrentRoomId);

	// Authenticate immediately.
	SendAuthHandshake();

	// Start keepalive pings.
	StartPingTimer();

	// Broadcast to listeners.
	OnConnected.Broadcast();
	OnConnectedNative.Broadcast();
}

void UPHCWebSocketClient::HandleConnectionError(const FString& Error)
{
	bIsConnected = false;
	UE_LOG(LogPHCWebSocket, Error, TEXT("WebSocket connection error: %s"), *Error);

	StopPingTimer();

	OnError.Broadcast(Error);
	OnErrorNative.Broadcast(Error);
}

void UPHCWebSocketClient::HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	bIsConnected = false;
	UE_LOG(LogPHCWebSocket, Log, TEXT("WebSocket closed. Status: %d  Reason: %s  Clean: %s"),
		StatusCode, *Reason, bWasClean ? TEXT("Yes") : TEXT("No"));

	StopPingTimer();

	OnDisconnected.Broadcast(StatusCode, Reason);
	OnDisconnectedNative.Broadcast(StatusCode, Reason);
}

void UPHCWebSocketClient::HandleMessage(const FString& MessageString)
{
	UE_LOG(LogPHCWebSocket, Verbose, TEXT("WS RX: %s"), *MessageString.Left(512));

	TSharedPtr<FJsonObject> JsonMsg;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageString);

	if (!FJsonSerializer::Deserialize(Reader, JsonMsg) || !JsonMsg.IsValid())
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("Failed to parse WebSocket message as JSON: %s"), *MessageString.Left(256));
		return;
	}

	DispatchMessage(JsonMsg);
}

void UPHCWebSocketClient::HandleMessageSent(const FString& MessageString)
{
	UE_LOG(LogPHCWebSocket, Verbose, TEXT("WS TX confirmed (%d bytes)"), MessageString.Len());
}

// -------------------------------------------------------------------
//  Message dispatch
// -------------------------------------------------------------------

void UPHCWebSocketClient::DispatchMessage(const TSharedPtr<FJsonObject>& JsonMsg)
{
	FString Type;
	if (!JsonMsg->TryGetStringField(TEXT("type"), Type))
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("WebSocket message missing 'type' field"));
		return;
	}

	// Retrieve the "data" sub-object (may be absent for some types).
	const TSharedPtr<FJsonObject>* DataPtr = nullptr;
	JsonMsg->TryGetObjectField(TEXT("data"), DataPtr);
	const TSharedPtr<FJsonObject> Data = DataPtr ? *DataPtr : nullptr;

	// ---- Chat message ----
	if (Type == PHCWsTypes::Chat)
	{
		FPHCChatMessage ChatMsg;
		if (Data.IsValid())
		{
			Data->TryGetNumberField(TEXT("sender_id"), ChatMsg.SenderId);
			Data->TryGetStringField(TEXT("sender_name"), ChatMsg.SenderName);
			Data->TryGetStringField(TEXT("message"), ChatMsg.Message);
			Data->TryGetStringField(TEXT("timestamp"), ChatMsg.Timestamp);
		}
		OnChatReceived.Broadcast(ChatMsg);
		OnChatReceivedNative.Broadcast(ChatMsg);
	}
	// ---- Player joined ----
	else if (Type == PHCWsTypes::PlayerJoined)
	{
		int32 PlayerId = 0;
		FString PlayerName;
		if (Data.IsValid())
		{
			Data->TryGetNumberField(TEXT("player_id"), PlayerId);
			Data->TryGetStringField(TEXT("player_name"), PlayerName);
		}
		UE_LOG(LogPHCWebSocket, Log, TEXT("Player joined: %d (%s)"), PlayerId, *PlayerName);
		OnPlayerJoined.Broadcast(PlayerId, PlayerName);
		OnPlayerJoinedNative.Broadcast(PlayerId, PlayerName);
	}
	// ---- Player left ----
	else if (Type == PHCWsTypes::PlayerLeft)
	{
		int32 PlayerId = 0;
		if (Data.IsValid())
		{
			Data->TryGetNumberField(TEXT("player_id"), PlayerId);
		}
		UE_LOG(LogPHCWebSocket, Log, TEXT("Player left: %d"), PlayerId);
		OnPlayerLeft.Broadcast(PlayerId);
		OnPlayerLeftNative.Broadcast(PlayerId);
	}
	// ---- Game action ----
	else if (Type == PHCWsTypes::GameAction)
	{
		FString ActionType;
		FString Payload;
		if (Data.IsValid())
		{
			Data->TryGetStringField(TEXT("action_type"), ActionType);

			// Payload may be a nested object or a string.
			const TSharedPtr<FJsonObject>* PayloadObj = nullptr;
			if (Data->TryGetObjectField(TEXT("payload"), PayloadObj) && PayloadObj && (*PayloadObj).IsValid())
			{
				const TSharedRef<FJsonObject> PayloadRef = (*PayloadObj).ToSharedRef();
				Payload = JsonObjectToString(PayloadRef);
			}
			else
			{
				Data->TryGetStringField(TEXT("payload"), Payload);
			}
		}
		OnGameAction.Broadcast(ActionType, Payload);
		OnGameActionNative.Broadcast(ActionType, Payload);
	}
	// ---- Theme changed ----
	else if (Type == PHCWsTypes::ThemeChanged)
	{
		FString ThemeId;
		if (Data.IsValid())
		{
			Data->TryGetStringField(TEXT("theme_id"), ThemeId);
		}
		UE_LOG(LogPHCWebSocket, Log, TEXT("Theme changed: %s"), *ThemeId);
		OnThemeChanged.Broadcast(ThemeId);
		OnThemeChangedNative.Broadcast(ThemeId);
	}
	// ---- Music sync ----
	else if (Type == PHCWsTypes::MusicSync)
	{
		FString TrackId;
		float Position = 0.f;
		if (Data.IsValid())
		{
			Data->TryGetStringField(TEXT("track_id"), TrackId);
			double PositionD = 0.0;
			if (Data->TryGetNumberField(TEXT("position"), PositionD))
			{
				Position = static_cast<float>(PositionD);
			}
		}
		OnMusicSync.Broadcast(TrackId, Position);
		OnMusicSyncNative.Broadcast(TrackId, Position);
	}
	// ---- Pong (keepalive response) ----
	else if (Type == PHCWsTypes::Pong)
	{
		UE_LOG(LogPHCWebSocket, Verbose, TEXT("Pong received"));
	}
	// ---- Error from server ----
	else if (Type == PHCWsTypes::Error)
	{
		FString ErrorMessage = TEXT("Unknown server error");
		if (Data.IsValid())
		{
			Data->TryGetStringField(TEXT("message"), ErrorMessage);
		}
		UE_LOG(LogPHCWebSocket, Warning, TEXT("Server error: %s"), *ErrorMessage);
		OnError.Broadcast(ErrorMessage);
		OnErrorNative.Broadcast(ErrorMessage);
	}
	else
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("Unhandled WebSocket message type: %s"), *Type);
	}
}

// -------------------------------------------------------------------
//  JSON helpers
// -------------------------------------------------------------------

void UPHCWebSocketClient::SendJsonMessage(const TSharedRef<FJsonObject>& JsonObject)
{
	if (!IsConnected())
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("Attempted to send message while disconnected."));
		return;
	}

	const FString JsonString = JsonObjectToString(JsonObject);
	UE_LOG(LogPHCWebSocket, Verbose, TEXT("WS TX: %s"), *JsonString.Left(512));
	WebSocket->Send(JsonString);
}

FString UPHCWebSocketClient::JsonObjectToString(const TSharedRef<FJsonObject>& JsonObject)
{
	FString Output;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return Output;
}

// -------------------------------------------------------------------
//  Auth handshake
// -------------------------------------------------------------------

void UPHCWebSocketClient::SendAuthHandshake()
{
	if (AuthToken.IsEmpty())
	{
		UE_LOG(LogPHCWebSocket, Warning, TEXT("No auth token set; skipping authentication handshake."));
		return;
	}

	TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("type"), PHCWsTypes::Auth);

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("token"), AuthToken);
	Msg->SetObjectField(TEXT("data"), Data);

	SendJsonMessage(Msg);
	UE_LOG(LogPHCWebSocket, Log, TEXT("Auth handshake sent."));
}

// -------------------------------------------------------------------
//  Keepalive / Ping
// -------------------------------------------------------------------

void UPHCWebSocketClient::StartPingTimer()
{
	StopPingTimer();

	// Use the world timer manager if we can find a world.
	UWorld* World = GetWorld();
	if (!World && GEngine)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	if (World)
	{
		World->GetTimerManager().SetTimer(
			PingTimerHandle,
			[this]() { SendPing(); },
			PingIntervalSeconds,
			true  // bLoop
		);
		UE_LOG(LogPHCWebSocket, Verbose, TEXT("Ping timer started (%.0fs interval)"), PingIntervalSeconds);
	}
	else
	{
		UE_LOG(LogPHCWebSocket, Warning,
			TEXT("Could not find a UWorld for the ping timer. Keepalive pings will not be sent."));
	}
}

void UPHCWebSocketClient::StopPingTimer()
{
	if (PingTimerHandle.IsValid())
	{
		UWorld* World = GetWorld();
		if (!World && GEngine)
		{
			World = GEngine->GetCurrentPlayWorld();
		}

		if (World)
		{
			World->GetTimerManager().ClearTimer(PingTimerHandle);
		}
		PingTimerHandle.Invalidate();
	}
}

void UPHCWebSocketClient::SendPing()
{
	if (!IsConnected())
	{
		return;
	}

	TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("type"), PHCWsTypes::Ping);
	Msg->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());

	SendJsonMessage(Msg);
	UE_LOG(LogPHCWebSocket, Verbose, TEXT("Ping sent"));
}
