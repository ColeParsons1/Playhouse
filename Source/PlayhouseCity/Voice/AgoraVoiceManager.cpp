// Copyright PlayhouseCity. All Rights Reserved.
// Non-iOS stub — Agora voice only works on iOS.

#if !PLATFORM_IOS

#include "Voice/AgoraVoiceManager.h"
#include "PlayhouseCity.h"

FAgoraVoiceManager& FAgoraVoiceManager::Get()
{
	static FAgoraVoiceManager Instance;
	return Instance;
}

void FAgoraVoiceManager::Initialize(const FString& AppId)
{
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora Voice: not available on this platform"));
}

void FAgoraVoiceManager::JoinChannel(const FString& ChannelName) {}
void FAgoraVoiceManager::LeaveChannel() {}
void FAgoraVoiceManager::SetMuted(bool bMute) { bMuted = bMute; }
void FAgoraVoiceManager::Shutdown() {}

#endif // !PLATFORM_IOS
