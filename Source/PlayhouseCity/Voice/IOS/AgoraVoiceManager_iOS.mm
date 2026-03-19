// Copyright PlayhouseCity. All Rights Reserved.
#if PLATFORM_IOS
#include "Voice/AgoraVoiceManager.h"
#include "PlayhouseCity.h"
#import <AgoraRtcKit/AgoraRtcEngineKit.h>

@interface PHCAgoraDelegate : NSObject <AgoraRtcEngineDelegate>
@end
@implementation PHCAgoraDelegate
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didJoinChannel:(NSString *)channel withUid:(NSUInteger)uid elapsed:(NSInteger)elapsed {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: SUCCESS joined channel '%s', uid=%lu, elapsed=%ldms"),
		*FString(channel), (unsigned long)uid, (long)elapsed);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didJoinedOfUid:(NSUInteger)uid elapsed:(NSInteger)elapsed {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: Remote user JOINED, uid=%lu"), (unsigned long)uid);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didOfflineOfUid:(NSUInteger)uid reason:(AgoraUserOfflineReason)reason {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: Remote user LEFT, uid=%lu reason=%d"), (unsigned long)uid, (int)reason);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didOccurError:(AgoraErrorCode)errorCode {
	UE_LOG(LogPlayhouseCity, Error, TEXT("Agora ERROR: code=%d"), (int)errorCode);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didOccurWarning:(AgoraWarningCode)warningCode {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora WARNING: code=%d"), (int)warningCode);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine remoteAudioStateChangedOfUid:(NSUInteger)uid state:(AgoraAudioRemoteState)state reason:(AgoraAudioRemoteReason)reason elapsed:(NSInteger)elapsed {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: Remote audio uid=%lu state=%d reason=%d"), (unsigned long)uid, (int)state, (int)reason);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine reportAudioVolumeIndicationOfSpeakers:(NSArray<AgoraRtcAudioVolumeInfo *> *)speakers totalVolume:(NSInteger)totalVolume {
	for (AgoraRtcAudioVolumeInfo* info in speakers) {
		if (info.uid != 0) {
			UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Remote speaker uid=%lu vol=%ld"), (unsigned long)info.uid, (long)info.volume);
		}
	}
}
@end

static AgoraRtcEngineKit* GRtcEngine = nil;
static PHCAgoraDelegate* GDelegate = nil;

FAgoraVoiceManager& FAgoraVoiceManager::Get() { static FAgoraVoiceManager Instance; return Instance; }

void FAgoraVoiceManager::Initialize(const FString& AppId) {
	if (bInitialized) return;
	GDelegate = [[PHCAgoraDelegate alloc] init];
	GRtcEngine = [AgoraRtcEngineKit sharedEngineWithAppId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*AppId)] delegate:GDelegate];
	[GRtcEngine setChannelProfile:AgoraChannelProfileCommunication];
	[GRtcEngine enableAudio];
	[GRtcEngine disableVideo];
	[GRtcEngine setAudioProfile:AgoraAudioProfileDefault];
	[GRtcEngine setAudioScenario:AgoraAudioScenarioChatRoom];
	[GRtcEngine setDefaultAudioRouteToSpeakerphone:YES];
	[GRtcEngine setEnableSpeakerphone:YES];
	[GRtcEngine enableAudioVolumeIndication:2000 smooth:3 reportVad:YES];
	AgoraHandle = (__bridge void*)GRtcEngine;
	bInitialized = true;
	UE_LOG(LogPlayhouseCity, Log, TEXT("Agora Voice initialized"));
}

void FAgoraVoiceManager::JoinChannel(const FString& ChannelName) {
	if (!bInitialized || bInChannel) return;
	if (GRtcEngine) {
		[GRtcEngine joinChannelByToken:nil channelId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*ChannelName)] info:nil uid:0 joinSuccess:nil];
		bInChannel = true; bMuted = false;
		UE_LOG(LogPlayhouseCity, Log, TEXT("Agora: Joining channel '%s'"), *ChannelName);
	}
}

void FAgoraVoiceManager::LeaveChannel() {
	if (!bInChannel) return;
	if (GRtcEngine) { [GRtcEngine leaveChannel:nil]; }
	bInChannel = false;
}

void FAgoraVoiceManager::SetMuted(bool bMute) {
	bMuted = bMute;
	if (GRtcEngine) { [GRtcEngine muteLocalAudioStream:bMute]; }
}

void FAgoraVoiceManager::Shutdown() {
	if (!bInitialized) return;
	if (GRtcEngine) { [GRtcEngine leaveChannel:nil]; [AgoraRtcEngineKit destroy]; GRtcEngine = nil; GDelegate = nil; }
	AgoraHandle = nullptr; bInitialized = false; bInChannel = false;
}
#endif
