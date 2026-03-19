// Copyright PlayhouseCity. All Rights Reserved.
// iOS Agora Voice implementation.

#if PLATFORM_IOS

#include "Voice/AgoraVoiceManager.h"
#include "PlayhouseCity.h"
#import <AgoraRtcKit/AgoraRtcEngineKit.h>

@interface PHCAgoraDelegate : NSObject <AgoraRtcEngineDelegate>
@end

@implementation PHCAgoraDelegate

- (void)rtcEngine:(AgoraRtcEngineKit *)engine didJoinChannel:(NSString *)channel withUid:(NSUInteger)uid elapsed:(NSInteger)elapsed
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Joined channel, uid=%lu"), (unsigned long)uid);
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

FAgoraVoiceManager& FAgoraVoiceManager::Get()
{
	static FAgoraVoiceManager Instance;
	return Instance;
}

void FAgoraVoiceManager::Initialize(const FString& AppId)
{
	if (bInitialized) return;
	GDelegate = [[PHCAgoraDelegate alloc] init];
	GRtcEngine = [AgoraRtcEngineKit sharedEngineWithAppId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*AppId)]
												 delegate:GDelegate];
	[GRtcEngine setChannelProfile:AgoraChannelProfileCommunication];
	[GRtcEngine enableAudio];
	[GRtcEngine disableVideo];
	[GRtcEngine setAudioProfile:AgoraAudioProfileDefault];
	[GRtcEngine setAudioScenario:AgoraAudioScenarioChatRoom];
	AgoraHandle = (__bridge void*)GRtcEngine;
	bInitialized = true;
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora Voice initialized"));
}

void FAgoraVoiceManager::JoinChannel(const FString& ChannelName)
{
	if (!bInitialized || bInChannel) return;
	if (GRtcEngine)
	{
		[GRtcEngine joinChannelByToken:nil
							 channelId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*ChannelName)]
								  info:nil uid:0 joinSuccess:nil];
		bInChannel = true;
		bMuted = false;
		UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Joining channel '%s'"), *ChannelName);
	}
}

void FAgoraVoiceManager::LeaveChannel()
{
	if (!bInChannel) return;
	if (GRtcEngine) { [GRtcEngine leaveChannel:nil]; }
	bInChannel = false;
}

void FAgoraVoiceManager::SetMuted(bool bMute)
{
	bMuted = bMute;
	if (GRtcEngine) { [GRtcEngine muteLocalAudioStream:bMute]; }
}

void FAgoraVoiceManager::Shutdown()
{
	if (!bInitialized) return;
	if (GRtcEngine) { [GRtcEngine leaveChannel:nil]; [AgoraRtcEngineKit destroy]; GRtcEngine = nil; GDelegate = nil; }
	AgoraHandle = nullptr;
	bInitialized = false;
	bInChannel = false;
}

#endif // PLATFORM_IOS
