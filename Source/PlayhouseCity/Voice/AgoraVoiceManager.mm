// Copyright PlayhouseCity. All Rights Reserved.

#include "Voice/AgoraVoiceManager.h"
#include "PlayhouseCity.h"

#if PLATFORM_IOS
#import <AgoraRtcKit/AgoraRtcEngineKit.h>

// Agora delegate for callbacks
@interface PHCAgoraDelegate : NSObject <AgoraRtcEngineDelegate>
@end

@implementation PHCAgoraDelegate

- (void)rtcEngine:(AgoraRtcEngineKit *)engine didJoinChannel:(NSString *)channel withUid:(NSUInteger)uid elapsed:(NSInteger)elapsed
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Joined channel %s, uid=%lu"), *FString(channel), (unsigned long)uid);
}

- (void)rtcEngine:(AgoraRtcEngineKit *)engine didJoinedOfUid:(NSUInteger)uid elapsed:(NSInteger)elapsed
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Remote user joined, uid=%lu"), (unsigned long)uid);
}

- (void)rtcEngine:(AgoraRtcEngineKit *)engine didOfflineOfUid:(NSUInteger)uid reason:(AgoraUserOfflineReason)reason
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Remote user left, uid=%lu"), (unsigned long)uid);
}

@end

static AgoraRtcEngineKit* GRtcEngine = nil;
static PHCAgoraDelegate* GDelegate = nil;

#endif // PLATFORM_IOS

FAgoraVoiceManager& FAgoraVoiceManager::Get()
{
	static FAgoraVoiceManager Instance;
	return Instance;
}

void FAgoraVoiceManager::Initialize(const FString& AppId)
{
	if (bInitialized) return;

#if PLATFORM_IOS
	GDelegate = [[PHCAgoraDelegate alloc] init];
	GRtcEngine = [AgoraRtcEngineKit sharedEngineWithAppId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*AppId)]
												 delegate:GDelegate];

	// Audio-only configuration
	[GRtcEngine setChannelProfile:AgoraChannelProfileCommunication];
	[GRtcEngine enableAudio];
	[GRtcEngine disableVideo];

	// Optimize for voice
	[GRtcEngine setAudioProfile:AgoraAudioProfileDefault];
	[GRtcEngine setAudioScenario:AgoraAudioScenarioChatRoom];

	AgoraHandle = (__bridge void*)GRtcEngine;
	bInitialized = true;
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora Voice initialized with AppId"));
#else
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora Voice: not available on this platform"));
#endif
}

void FAgoraVoiceManager::JoinChannel(const FString& ChannelName)
{
	if (!bInitialized || bInChannel) return;

#if PLATFORM_IOS
	if (GRtcEngine)
	{
		// Join with auto-generated UID (0 = server assigns)
		[GRtcEngine joinChannelByToken:nil
							 channelId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*ChannelName)]
								  info:nil
									uid:0
							joinSuccess:nil];
		bInChannel = true;
		bMuted = false;
		UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Joining channel '%s'"), *ChannelName);
	}
#endif
}

void FAgoraVoiceManager::LeaveChannel()
{
	if (!bInChannel) return;

#if PLATFORM_IOS
	if (GRtcEngine)
	{
		[GRtcEngine leaveChannel:nil];
		bInChannel = false;
		UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Left channel"));
	}
#endif
}

void FAgoraVoiceManager::SetMuted(bool bMute)
{
	bMuted = bMute;

#if PLATFORM_IOS
	if (GRtcEngine)
	{
		[GRtcEngine muteLocalAudioStream:bMute];
		UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Mute = %s"), bMute ? TEXT("ON") : TEXT("OFF"));
	}
#endif
}

void FAgoraVoiceManager::Shutdown()
{
	if (!bInitialized) return;

#if PLATFORM_IOS
	if (GRtcEngine)
	{
		[GRtcEngine leaveChannel:nil];
		[AgoraRtcEngineKit destroy];
		GRtcEngine = nil;
		GDelegate = nil;
	}
#endif

	AgoraHandle = nullptr;
	bInitialized = false;
	bInChannel = false;
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora Voice shutdown"));
}
