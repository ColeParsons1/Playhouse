// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Agora Voice Manager — thin C++ wrapper around Agora iOS SDK.
 * Manages global voice channel for all players.
 */
class PLAYHOUSECITY_API FAgoraVoiceManager
{
public:
	static FAgoraVoiceManager& Get();

	/** Initialize Agora with App ID. Call once at startup. */
	void Initialize(const FString& AppId);

	/** Join a voice channel. Fetches a token from the backend, then joins. */
	void JoinChannel(const FString& ChannelName);

	/** Join with a specific token (called after async token fetch). */
	void JoinChannelWithToken(const FString& ChannelName, const FString& Token);

	/** Leave the current voice channel. */
	void LeaveChannel();

	/** Mute/unmute the local microphone. */
	void SetMuted(bool bMute);

	/** Check if currently in a channel. */
	bool IsInChannel() const { return bInChannel; }

	/** Check if muted. */
	bool IsMuted() const { return bMuted; }

	/** Number of remote users currently in channel. */
	int32 GetRemoteUserCount() const { return RemoteUserCount; }

	/** Last Agora error code (0 = none). */
	int32 GetLastError() const { return LastErrorCode; }

	/** Shutdown and release Agora resources. */
	void Shutdown();

	int32 RemoteUserCount = 0;
	int32 LastErrorCode = 0;

private:
	FAgoraVoiceManager() = default;
	bool bInitialized = false;
	bool bInChannel = false;
	bool bMuted = false;

	// Platform-specific Agora handle (void* to avoid ObjC in header)
	void* AgoraHandle = nullptr;
};
