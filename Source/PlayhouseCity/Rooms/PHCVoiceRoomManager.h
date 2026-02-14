// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PHCVoiceRoomManager.generated.h"

class UPHCWebSocketClient;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast when the local player successfully joins a voice room. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPHCRoomJoined, int32, RoomId);

/** Broadcast when the local player leaves the current voice room. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPHCRoomLeft);

/** Broadcast when any participant's speaking state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPHCPlayerSpeaking, int32, PlayerId, bool, bSpeaking);

/** Broadcast when the local player's mute state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPHCMuteChanged, bool, bMuted);

// ---------------------------------------------------------------------------
// UPHCVoiceRoomManager
// ---------------------------------------------------------------------------

/**
 * UPHCVoiceRoomManager
 *
 * Manages voice room functionality for PlayhouseCity. Handles joining and
 * leaving voice rooms, muting, and participant tracking. Voice data is
 * coordinated through WebSocket signaling with the Django backend.
 *
 * Future versions (v2) will integrate Agora or Vivox SDKs for actual
 * real-time voice transport. The current implementation handles all
 * signaling and state management so the transport layer can be swapped in.
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCVoiceRoomManager : public UObject
{
	GENERATED_BODY()

public:
	UPHCVoiceRoomManager();

	// ---------------------------------------------------------------
	//  Initialization
	// ---------------------------------------------------------------

	/**
	 * Initialize the voice room manager with a WebSocket client reference.
	 * Must be called before any other methods.
	 *
	 * @param WsClient  The WebSocket client used for voice signaling.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void Initialize(UPHCWebSocketClient* WsClient);

	// ---------------------------------------------------------------
	//  Room Management
	// ---------------------------------------------------------------

	/**
	 * Join a voice room by ID. Sends a join message to the server via
	 * WebSocket and waits for confirmation before broadcasting OnRoomJoined.
	 *
	 * @param RoomId  The unique identifier of the room to join.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void JoinRoom(int32 RoomId);

	/**
	 * Leave the current voice room. Sends a leave message, stops voice
	 * capture, clears participant state, and broadcasts OnRoomLeft.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void LeaveRoom();

	// ---------------------------------------------------------------
	//  Mute Controls
	// ---------------------------------------------------------------

	/**
	 * Set the local player's mute state and notify the server.
	 *
	 * @param bMute  True to mute, false to unmute.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void SetMuted(bool bMute);

	/** Returns true if the local player is currently muted. */
	UFUNCTION(BlueprintPure, Category = "PHC|Voice")
	bool IsMuted() const { return bIsMuted; }

	/**
	 * Locally mute or unmute another player. This does not affect what
	 * other participants hear -- it only suppresses playback locally.
	 *
	 * @param PlayerId  The ID of the player to mute/unmute locally.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void MutePlayer(int32 PlayerId);

	/**
	 * Check whether a specific player is locally muted.
	 *
	 * @param PlayerId  The player to query.
	 * @return True if the player is locally muted.
	 */
	UFUNCTION(BlueprintPure, Category = "PHC|Voice")
	bool IsPlayerMuted(int32 PlayerId) const;

	// ---------------------------------------------------------------
	//  Participant Queries
	// ---------------------------------------------------------------

	/**
	 * Get the list of player IDs currently in the voice room.
	 *
	 * @return Array of player IDs in the current room (empty if not in a room).
	 */
	UFUNCTION(BlueprintPure, Category = "PHC|Voice")
	TArray<int32> GetParticipants() const;

	/** Returns the ID of the room the local player is currently in, or -1. */
	UFUNCTION(BlueprintPure, Category = "PHC|Voice")
	int32 GetCurrentRoomId() const { return CurrentRoomId; }

	/** Returns true if the local player is currently in a voice room. */
	UFUNCTION(BlueprintPure, Category = "PHC|Voice")
	bool IsInRoom() const { return bIsInRoom; }

	// ---------------------------------------------------------------
	//  WebSocket Message Handler
	// ---------------------------------------------------------------

	/**
	 * Process an incoming WebSocket message related to voice rooms.
	 * Called by the WebSocket client's message router.
	 *
	 * @param Type     The message type identifier (e.g. "voice_join", "voice_leave", "voice_speaking").
	 * @param Payload  The JSON payload string.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void OnWebSocketMessage(const FString& Type, const FString& Payload);

	// ---------------------------------------------------------------
	//  Voice Capture (v2 placeholder)
	// ---------------------------------------------------------------

	/**
	 * Start capturing voice input from the local microphone.
	 * This is a placeholder for future Agora/Vivox integration.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void StartVoiceCapture();

	/**
	 * Stop capturing voice input from the local microphone.
	 * This is a placeholder for future Agora/Vivox integration.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Voice")
	void StopVoiceCapture();

	// ---------------------------------------------------------------
	//  Delegates
	// ---------------------------------------------------------------

	/** Fired when the local player successfully joins a voice room. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Voice")
	FOnPHCRoomJoined OnRoomJoined;

	/** Fired when the local player leaves the current voice room. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Voice")
	FOnPHCRoomLeft OnRoomLeft;

	/** Fired when any player's speaking state changes. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Voice")
	FOnPHCPlayerSpeaking OnPlayerSpeaking;

	/** Fired when the local player's mute state changes. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Voice")
	FOnPHCMuteChanged OnMuteChanged;

private:
	// ---------------------------------------------------------------
	//  Internal Helpers
	// ---------------------------------------------------------------

	/** Send a JSON message over the WebSocket for voice signaling. */
	void SendVoiceMessage(const FString& MessageType, const FString& PayloadJson);

	/** Handle a participant joining the current room. */
	void HandlePlayerJoined(int32 PlayerId);

	/** Handle a participant leaving the current room. */
	void HandlePlayerLeft(int32 PlayerId);

	/** Handle a speaking state update from a participant. */
	void HandleSpeakingUpdate(int32 PlayerId, bool bSpeaking);

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** The ID of the voice room the local player is currently in. -1 if none. */
	UPROPERTY()
	int32 CurrentRoomId;

	/** Whether the local player's microphone is muted. */
	UPROPERTY()
	bool bIsMuted;

	/** Whether the local player is currently in a voice room. */
	UPROPERTY()
	bool bIsInRoom;

	/** Whether voice capture is currently active (v2 placeholder). */
	UPROPERTY()
	bool bIsCapturing;

	/** Per-player local mute states. Key = PlayerId, Value = true if muted locally. */
	UPROPERTY()
	TMap<int32, bool> PlayerMuteStates;

	/** Set of player IDs currently in the same voice room. */
	UPROPERTY()
	TArray<int32> Participants;

	/** Cached reference to the WebSocket client for signaling. */
	UPROPERTY()
	TObjectPtr<UPHCWebSocketClient> WebSocketClient;
};
