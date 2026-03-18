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

	/** Join a voice channel. All players in the same channel can hear each other. */
	void JoinChannel(const FString& ChannelName);

	/** Leave the current voice channel. */
	void LeaveChannel();

	/** Mute/unmute the local microphone. */
	void SetMuted(bool bMute);

	/** Check if currently in a channel. */
	bool IsInChannel() const { return bInChannel; }

	/** Check if muted. */
	bool IsMuted() const { return bMuted; }

	/** Shutdown and release Agora resources. */
	void Shutdown();

private:
	FAgoraVoiceManager() = default;
	bool bInitialized = false;
	bool bInChannel = false;
	bool bMuted = false;

	// Platform-specific Agora handle (void* to avoid ObjC in header)
	void* AgoraHandle = nullptr;
};
