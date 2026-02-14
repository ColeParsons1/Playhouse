// Copyright PlayhouseCity. All Rights Reserved.

#include "Rooms/PHCVoiceRoomManager.h"
#include "PlayhouseCity.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCVoiceRoomManager::UPHCVoiceRoomManager()
	: CurrentRoomId(-1)
	, bIsMuted(false)
	, bIsInRoom(false)
	, bIsCapturing(false)
	, WebSocketClient(nullptr)
{
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void UPHCVoiceRoomManager::Initialize(UPHCWebSocketClient* WsClient)
{
	WebSocketClient = WsClient;

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCVoiceRoomManager::Initialize -- Voice room manager initialized."));
}

// ---------------------------------------------------------------------------
// Room Management
// ---------------------------------------------------------------------------

void UPHCVoiceRoomManager::JoinRoom(int32 RoomId)
{
	if (bIsInRoom)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCVoiceRoomManager::JoinRoom -- Already in room %d. Leaving first."), CurrentRoomId);
		LeaveRoom();
	}

	// Build the join payload
	TSharedRef<FJsonObject> PayloadObj = MakeShared<FJsonObject>();
	PayloadObj->SetNumberField(TEXT("room_id"), RoomId);

	FString PayloadString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
	FJsonSerializer::Serialize(PayloadObj, Writer);

	SendVoiceMessage(TEXT("voice_join"), PayloadString);

	// Optimistically set state -- the server will confirm or reject
	CurrentRoomId = RoomId;
	bIsInRoom = true;
	bIsMuted = false;
	Participants.Empty();
	PlayerMuteStates.Empty();

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::JoinRoom -- Requesting to join voice room %d."), RoomId);

	OnRoomJoined.Broadcast(RoomId);
}

void UPHCVoiceRoomManager::LeaveRoom()
{
	if (!bIsInRoom)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCVoiceRoomManager::LeaveRoom -- Not currently in a voice room."));
		return;
	}

	// Stop voice capture before leaving
	StopVoiceCapture();

	// Notify server
	TSharedRef<FJsonObject> PayloadObj = MakeShared<FJsonObject>();
	PayloadObj->SetNumberField(TEXT("room_id"), CurrentRoomId);

	FString PayloadString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
	FJsonSerializer::Serialize(PayloadObj, Writer);

	SendVoiceMessage(TEXT("voice_leave"), PayloadString);

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::LeaveRoom -- Left voice room %d."), CurrentRoomId);

	// Reset local state
	const int32 PreviousRoomId = CurrentRoomId;
	CurrentRoomId = -1;
	bIsInRoom = false;
	bIsMuted = false;
	Participants.Empty();
	PlayerMuteStates.Empty();

	OnRoomLeft.Broadcast();
}

// ---------------------------------------------------------------------------
// Mute Controls
// ---------------------------------------------------------------------------

void UPHCVoiceRoomManager::SetMuted(bool bMute)
{
	if (bIsMuted == bMute)
	{
		return;
	}

	bIsMuted = bMute;

	// Notify the server of the mute state change
	if (bIsInRoom)
	{
		TSharedRef<FJsonObject> PayloadObj = MakeShared<FJsonObject>();
		PayloadObj->SetNumberField(TEXT("room_id"), CurrentRoomId);
		PayloadObj->SetBoolField(TEXT("muted"), bIsMuted);

		FString PayloadString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
		FJsonSerializer::Serialize(PayloadObj, Writer);

		SendVoiceMessage(TEXT("voice_mute"), PayloadString);
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::SetMuted -- Mute state set to %s."),
		bIsMuted ? TEXT("MUTED") : TEXT("UNMUTED"));

	OnMuteChanged.Broadcast(bIsMuted);
}

void UPHCVoiceRoomManager::MutePlayer(int32 PlayerId)
{
	// Toggle the local mute state for this player
	bool* bCurrentState = PlayerMuteStates.Find(PlayerId);
	if (bCurrentState)
	{
		*bCurrentState = !(*bCurrentState);
	}
	else
	{
		PlayerMuteStates.Add(PlayerId, true);
	}

	const bool bNowMuted = PlayerMuteStates.FindChecked(PlayerId);
	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::MutePlayer -- Player %d is now %s locally."),
		PlayerId, bNowMuted ? TEXT("MUTED") : TEXT("UNMUTED"));
}

bool UPHCVoiceRoomManager::IsPlayerMuted(int32 PlayerId) const
{
	const bool* bMuted = PlayerMuteStates.Find(PlayerId);
	return bMuted ? *bMuted : false;
}

// ---------------------------------------------------------------------------
// Participant Queries
// ---------------------------------------------------------------------------

TArray<int32> UPHCVoiceRoomManager::GetParticipants() const
{
	return Participants;
}

// ---------------------------------------------------------------------------
// WebSocket Message Handler
// ---------------------------------------------------------------------------

void UPHCVoiceRoomManager::OnWebSocketMessage(const FString& Type, const FString& Payload)
{
	// Parse the JSON payload
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCVoiceRoomManager::OnWebSocketMessage -- Failed to parse payload for type '%s'."), *Type);
		return;
	}

	if (Type == TEXT("voice_player_joined"))
	{
		const int32 PlayerId = static_cast<int32>(JsonObject->GetNumberField(TEXT("player_id")));
		HandlePlayerJoined(PlayerId);
	}
	else if (Type == TEXT("voice_player_left"))
	{
		const int32 PlayerId = static_cast<int32>(JsonObject->GetNumberField(TEXT("player_id")));
		HandlePlayerLeft(PlayerId);
	}
	else if (Type == TEXT("voice_speaking"))
	{
		const int32 PlayerId = static_cast<int32>(JsonObject->GetNumberField(TEXT("player_id")));
		const bool bSpeaking = JsonObject->GetBoolField(TEXT("speaking"));
		HandleSpeakingUpdate(PlayerId, bSpeaking);
	}
	else if (Type == TEXT("voice_room_participants"))
	{
		// Full participant list received (e.g., on initial join)
		const TArray<TSharedPtr<FJsonValue>>* ParticipantArray = nullptr;
		if (JsonObject->TryGetArrayField(TEXT("participants"), ParticipantArray))
		{
			Participants.Empty();
			for (const TSharedPtr<FJsonValue>& Value : *ParticipantArray)
			{
				if (Value.IsValid())
				{
					Participants.Add(static_cast<int32>(Value->AsNumber()));
				}
			}
		}

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHCVoiceRoomManager::OnWebSocketMessage -- Received participant list with %d players."),
			Participants.Num());
	}
	else if (Type == TEXT("voice_join_rejected"))
	{
		// Server rejected our join request -- revert state
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCVoiceRoomManager::OnWebSocketMessage -- Join rejected for room %d. Reason: %s"),
			CurrentRoomId, *JsonObject->GetStringField(TEXT("reason")));

		CurrentRoomId = -1;
		bIsInRoom = false;
		Participants.Empty();
		PlayerMuteStates.Empty();

		OnRoomLeft.Broadcast();
	}
	else if (Type == TEXT("voice_mute_update"))
	{
		// Another player changed their mute state -- informational only
		const int32 PlayerId = static_cast<int32>(JsonObject->GetNumberField(TEXT("player_id")));
		const bool bMuted = JsonObject->GetBoolField(TEXT("muted"));

		UE_LOG(LogPlayhouseCity, Verbose,
			TEXT("PHCVoiceRoomManager::OnWebSocketMessage -- Player %d mute state: %s"),
			PlayerId, bMuted ? TEXT("MUTED") : TEXT("UNMUTED"));
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Verbose,
			TEXT("PHCVoiceRoomManager::OnWebSocketMessage -- Unhandled voice message type: '%s'."), *Type);
	}
}

// ---------------------------------------------------------------------------
// Voice Capture (v2 placeholder)
// ---------------------------------------------------------------------------

void UPHCVoiceRoomManager::StartVoiceCapture()
{
	if (bIsCapturing)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCVoiceRoomManager::StartVoiceCapture -- Voice capture is already active."));
		return;
	}

	if (!bIsInRoom)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCVoiceRoomManager::StartVoiceCapture -- Cannot start capture: not in a voice room."));
		return;
	}

	bIsCapturing = true;

	// TODO (v2): Initialize Agora/Vivox voice engine here.
	// - Create voice engine instance
	// - Configure audio input device
	// - Join the voice channel associated with CurrentRoomId
	// - Begin transmitting audio frames

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::StartVoiceCapture -- Voice capture started (placeholder, no audio transport in v1)."));
}

void UPHCVoiceRoomManager::StopVoiceCapture()
{
	if (!bIsCapturing)
	{
		return;
	}

	bIsCapturing = false;

	// TODO (v2): Tear down Agora/Vivox voice engine here.
	// - Stop transmitting audio frames
	// - Leave the voice channel
	// - Release audio input device
	// - Destroy voice engine instance

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::StopVoiceCapture -- Voice capture stopped (placeholder, no audio transport in v1)."));
}

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------

void UPHCVoiceRoomManager::SendVoiceMessage(const FString& MessageType, const FString& PayloadJson)
{
	if (!WebSocketClient)
	{
		UE_LOG(LogPlayhouseCity, Error,
			TEXT("PHCVoiceRoomManager::SendVoiceMessage -- WebSocketClient is null. Cannot send '%s'."),
			*MessageType);
		return;
	}

	// Construct the envelope: {"type": "<MessageType>", "payload": <PayloadJson>}
	TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
	Envelope->SetStringField(TEXT("type"), MessageType);

	// Parse the payload string back into a JSON object for proper nesting
	TSharedPtr<FJsonObject> PayloadObject;
	TSharedRef<TJsonReader<>> PayloadReader = TJsonReaderFactory<>::Create(PayloadJson);
	if (FJsonSerializer::Deserialize(PayloadReader, PayloadObject) && PayloadObject.IsValid())
	{
		Envelope->SetObjectField(TEXT("payload"), PayloadObject);
	}
	else
	{
		// If the payload isn't valid JSON, send it as a raw string field
		Envelope->SetStringField(TEXT("payload"), PayloadJson);
	}

	FString EnvelopeString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&EnvelopeString);
	FJsonSerializer::Serialize(Envelope, Writer);

	// TODO: Call WebSocketClient->Send(EnvelopeString) when the WebSocket
	// client interface is finalized. For now, log the outgoing message.
	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCVoiceRoomManager::SendVoiceMessage -- Sending: %s"), *EnvelopeString);
}

void UPHCVoiceRoomManager::HandlePlayerJoined(int32 PlayerId)
{
	if (!Participants.Contains(PlayerId))
	{
		Participants.Add(PlayerId);
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::HandlePlayerJoined -- Player %d joined the voice room. Total participants: %d."),
		PlayerId, Participants.Num());
}

void UPHCVoiceRoomManager::HandlePlayerLeft(int32 PlayerId)
{
	Participants.Remove(PlayerId);
	PlayerMuteStates.Remove(PlayerId);

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCVoiceRoomManager::HandlePlayerLeft -- Player %d left the voice room. Total participants: %d."),
		PlayerId, Participants.Num());
}

void UPHCVoiceRoomManager::HandleSpeakingUpdate(int32 PlayerId, bool bSpeaking)
{
	// Do not fire the delegate if the player is locally muted
	if (IsPlayerMuted(PlayerId))
	{
		return;
	}

	UE_LOG(LogPlayhouseCity, Verbose,
		TEXT("PHCVoiceRoomManager::HandleSpeakingUpdate -- Player %d is %s."),
		PlayerId, bSpeaking ? TEXT("SPEAKING") : TEXT("SILENT"));

	OnPlayerSpeaking.Broadcast(PlayerId, bSpeaking);
}
