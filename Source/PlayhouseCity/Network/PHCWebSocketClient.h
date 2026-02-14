// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "Core/PHCTypes.h"
#include "Engine/TimerHandle.h"
#include "PHCWebSocketClient.generated.h"

class IWebSocket;

// -------------------------------------------------------------------
//  Multicast delegate declarations
// -------------------------------------------------------------------

/** Fired when the WebSocket connection is successfully established. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPHCOnWsConnected);

/** Fired when the WebSocket connection is closed (cleanly or otherwise). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPHCOnWsDisconnected, int32, StatusCode, const FString&, Reason);

/** Fired when a chat message is received from the server. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPHCOnWsChatReceived, const FPHCChatMessage&, ChatMessage);

/** Fired when another player joins the room. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPHCOnWsPlayerJoined, int32, PlayerId, const FString&, PlayerName);

/** Fired when another player leaves the room. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPHCOnWsPlayerLeft, int32, PlayerId);

/** Fired for generic game actions (mini-game moves, interactions, etc.). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPHCOnWsGameAction, const FString&, ActionType, const FString&, Payload);

/** Fired when the room theme is changed by an admin or vote. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPHCOnWsThemeChanged, const FString&, ThemeId);

/** Fired to synchronize music playback across room members. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPHCOnWsMusicSync, const FString&, TrackId, float, Position);

/** Fired on any WebSocket-level error. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPHCOnWsError, const FString&, ErrorMessage);

// Native (C++) multicast delegates for subsystem-level listeners.
DECLARE_MULTICAST_DELEGATE(FPHCOnWsConnectedNative);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPHCOnWsDisconnectedNative, int32 /*StatusCode*/, const FString& /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPHCOnWsChatReceivedNative, const FPHCChatMessage& /*ChatMessage*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPHCOnWsPlayerJoinedNative, int32 /*PlayerId*/, const FString& /*PlayerName*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPHCOnWsPlayerLeftNative, int32 /*PlayerId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPHCOnWsGameActionNative, const FString& /*ActionType*/, const FString& /*Payload*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPHCOnWsThemeChangedNative, const FString& /*ThemeId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FPHCOnWsMusicSyncNative, const FString& /*TrackId*/, float /*Position*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPHCOnWsErrorNative, const FString& /*ErrorMessage*/);

// -------------------------------------------------------------------
//  UPHCWebSocketClient
// -------------------------------------------------------------------

/**
 * UPHCWebSocketClient
 *
 * Manages a single IWebSocket connection to the PlayhouseCity Django
 * Channels backend.  Handles JSON message framing, authentication
 * handshake, keep-alive pings, and dispatches typed events via
 * multicast delegates (both Blueprint and native).
 *
 * Typical lifecycle:
 *   1. Initialize(BaseWsUrl)
 *   2. SetAuthToken(Token)
 *   3. Connect(RoomId)
 *   4. SendChatMessage / SendGameAction / ...
 *   5. Disconnect()
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCWebSocketClient : public UObject
{
	GENERATED_BODY()

public:
	UPHCWebSocketClient();
	virtual void BeginDestroy() override;

	// ---------------------------------------------------------------
	//  Setup
	// ---------------------------------------------------------------

	/**
	 * Set the base WebSocket URL (e.g. "ws://localhost:8000" or "wss://api.playhousecity.com").
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|WebSocket")
	void Initialize(const FString& BaseWsUrl);

	/**
	 * Set the auth token to send to the server immediately after connection.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|WebSocket")
	void SetAuthToken(const FString& Token);

	// ---------------------------------------------------------------
	//  Connection lifecycle
	// ---------------------------------------------------------------

	/**
	 * Open a WebSocket connection to the specified room.
	 * URL format: <BaseWsUrl>/ws/room/<RoomId>/
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|WebSocket")
	void Connect(int32 RoomId);

	/**
	 * Gracefully close the current WebSocket connection.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|WebSocket")
	void Disconnect();

	/**
	 * Returns true if the WebSocket is currently connected.
	 */
	UFUNCTION(BlueprintPure, Category = "PlayhouseCity|WebSocket")
	bool IsConnected() const;

	// ---------------------------------------------------------------
	//  Outbound messages
	// ---------------------------------------------------------------

	/** Send a chat message to the current room. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|WebSocket")
	void SendChatMessage(const FString& Message);

	/** Send a game action with an arbitrary JSON payload. */
	void SendGameAction(const FString& ActionType, TSharedPtr<FJsonObject> Payload);

	/** Blueprint-friendly variant that accepts payload as a JSON string. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|WebSocket", meta = (DisplayName = "Send Game Action"))
	void K2_SendGameAction(const FString& ActionType, const FString& PayloadJson);

	/** Notify the room of a theme change. */
	UFUNCTION(BlueprintCallable, Category = "PlayhouseCity|WebSocket")
	void SendThemeChange(const FString& ThemeId);

	// ---------------------------------------------------------------
	//  Blueprint delegates (assign in Blueprints or via AddDynamic)
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsDisconnected OnDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsChatReceived OnChatReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsPlayerJoined OnPlayerJoined;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsPlayerLeft OnPlayerLeft;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsGameAction OnGameAction;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsThemeChanged OnThemeChanged;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsMusicSync OnMusicSync;

	UPROPERTY(BlueprintAssignable, Category = "PlayhouseCity|WebSocket")
	FPHCOnWsError OnError;

	// ---------------------------------------------------------------
	//  Native C++ delegates (bind from subsystems / C++ code)
	// ---------------------------------------------------------------

	FPHCOnWsConnectedNative OnConnectedNative;
	FPHCOnWsDisconnectedNative OnDisconnectedNative;
	FPHCOnWsChatReceivedNative OnChatReceivedNative;
	FPHCOnWsPlayerJoinedNative OnPlayerJoinedNative;
	FPHCOnWsPlayerLeftNative OnPlayerLeftNative;
	FPHCOnWsGameActionNative OnGameActionNative;
	FPHCOnWsThemeChangedNative OnThemeChangedNative;
	FPHCOnWsMusicSyncNative OnMusicSyncNative;
	FPHCOnWsErrorNative OnErrorNative;

private:
	// ---------------------------------------------------------------
	//  Internal handlers
	// ---------------------------------------------------------------

	void HandleConnected();
	void HandleConnectionError(const FString& Error);
	void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleMessage(const FString& MessageString);
	void HandleMessageSent(const FString& MessageString);

	/** Route a parsed JSON message to the correct delegate based on "type". */
	void DispatchMessage(const TSharedPtr<FJsonObject>& JsonMsg);

	/** Send a raw JSON object over the socket. */
	void SendJsonMessage(const TSharedRef<FJsonObject>& JsonObject);

	/** Convert an FJsonObject to a compact JSON string. */
	static FString JsonObjectToString(const TSharedRef<FJsonObject>& JsonObject);

	/** Send the authentication handshake message. */
	void SendAuthHandshake();

	// ---------------------------------------------------------------
	//  Keepalive / ping
	// ---------------------------------------------------------------

	void StartPingTimer();
	void StopPingTimer();
	void SendPing();

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** The underlying IWebSocket instance. */
	TSharedPtr<IWebSocket> WebSocket;

	/** Base URL without trailing slash (e.g. "ws://localhost:8000"). */
	FString BaseUrl;

	/** Auth token to send after connection. */
	FString AuthToken;

	/** The room ID we are currently connected to (or attempting). */
	int32 CurrentRoomId = 0;

	/** Whether we consider ourselves connected. */
	bool bIsConnected = false;

	/** Timer handle for the periodic ping. */
	FTimerHandle PingTimerHandle;

	/** Interval in seconds between keepalive pings. */
	static constexpr float PingIntervalSeconds = 30.f;
};
