// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCRoomWidget.generated.h"

class UTextBlock;
class UBorder;
class UVerticalBox;
class UScrollBox;
class UEditableTextBox;
class UButton;
class UPHCGameInstance;
class UPHCHttpClient;
class IWebSocket;

/**
 * Delegate broadcast when the player leaves the room.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomLeft);

/**
 * UPHCRoomWidget
 *
 * The in-room experience widget. Manages room metadata display, a real-time
 * chat panel over WebSocket, a participant list, and actions like muting,
 * leaving the room, and launching mini-games.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCRoomWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCRoomWidget(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// -- Public API ------------------------------------------------------------

	/** Join a room by ID. Fetches details, connects WebSocket, and populates UI. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void JoinRoom(int32 RoomId);

	/** Disconnect from the room WebSocket, call leave API, and navigate back. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void LeaveRoom();

	/** Update the participant list with a full list of current players. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void UpdateParticipants(const TArray<FPHCPlayerProfile>& Players);

	/** Broadcast when the room is left. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Room")
	FOnRoomLeft OnRoomLeft;

protected:
	// -- Bind Widgets ----------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> ThemeIndicator;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> ParticipantsList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> ChatScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> ChatInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> SendButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> MuteToggle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> GameLaunchButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ParticipantCountText;

	// -- Data ------------------------------------------------------------------

	/** The room this widget is currently connected to. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Room")
	int32 CurrentRoomId = 0;

	/** Detailed room information from the API. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Room")
	FPHCRoomInfo RoomInfo;

	/** Local chat history for scrollback. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Room")
	TArray<FPHCChatMessage> ChatHistory;

	// -- Chat actions ----------------------------------------------------------

	/** Called when a chat message is received over WebSocket. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnChatReceived(const FPHCChatMessage& Message);

	/** Handle the Send button click. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnSendClicked();

	/** Handle keyboard submit in the chat input field. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnChatSubmitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** Called when a new player joins the room (from WebSocket event). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnPlayerJoined(int32 PlayerId, const FString& Name);

	/** Called when a player leaves the room (from WebSocket event). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnPlayerLeft(int32 PlayerId);

	/** Toggle local mute state (chat visibility or audio). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnMuteToggled();

	/** Open a game-selection popup and launch the chosen mini-game. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnGameLaunchClicked();

	/** Confirm leaving the room, then call LeaveRoom(). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void OnLeaveClicked();

	/** Create a visually styled chat bubble widget and add to the scroll box. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Room")
	void AddChatBubble(const FPHCChatMessage& Msg);

private:
	// -- Internal helpers ------------------------------------------------------

	/** Connect the WebSocket to the room channel. */
	void ConnectWebSocket();

	/** Disconnect and clean up the WebSocket. */
	void DisconnectWebSocket();

	/** Parse an incoming WebSocket text message. */
	void HandleWebSocketMessage(const FString& MessageString);

	/** Callback after GetRoomDetails API returns. */
	UFUNCTION()
	void OnRoomDetailsReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Callback after LeaveRoom API returns. */
	UFUNCTION()
	void OnLeaveRoomResult(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Send a chat message over WebSocket. */
	void SendChatMessage(const FString& MessageText);

	/** Convenience accessor for the HttpClient via GameInstance. */
	UPHCHttpClient* GetHttpClient() const;

	/** Active WebSocket connection for this room. */
	TSharedPtr<IWebSocket> WebSocket;

	/** Whether local chat is muted. */
	bool bIsMuted = false;

	/** List of currently displayed participant player IDs (to avoid duplicates). */
	TArray<int32> ParticipantPlayerIds;

	/** Maximum number of chat messages to keep in history. */
	static constexpr int32 MaxChatHistory = 200;
};
