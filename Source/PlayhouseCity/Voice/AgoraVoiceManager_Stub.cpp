// Copyright PlayhouseCity. All Rights Reserved.
// Stub for non-Apple platforms (Linux, Windows, Android)
#if !PLATFORM_IOS && !PLATFORM_MAC
#include "Voice/AgoraVoiceManager.h"
#include "PlayhouseCity.h"
FAgoraVoiceManager& FAgoraVoiceManager::Get() { static FAgoraVoiceManager Instance; return Instance; }
void FAgoraVoiceManager::Initialize(const FString& AppId) { UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: not available on this platform")); }
void FAgoraVoiceManager::JoinChannel(const FString& ChannelName) {}
void FAgoraVoiceManager::LeaveChannel() {}
void FAgoraVoiceManager::SetMuted(bool bMute) { bMuted = bMute; }
void FAgoraVoiceManager::Shutdown() {}
#endif
