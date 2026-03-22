// Copyright PlayhouseCity. All Rights Reserved.
// Unified Agora voice implementation: iOS, Mac, and stub for other platforms.

#include "Voice/AgoraVoiceManager.h"
#include "PlayhouseCity.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_IOS || PLATFORM_MAC
#if PLATFORM_IOS
#import <AgoraRtcKit/AgoraRtcEngineKit.h>
#else
#import "AgoraRtcEngineKit.h"
#endif

@interface PHCAgoraDelegate : NSObject <AgoraRtcEngineDelegate>
@end
@implementation PHCAgoraDelegate
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didJoinChannel:(NSString *)channel withUid:(NSUInteger)uid elapsed:(NSInteger)elapsed {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: SUCCESS joined channel '%s', uid=%lu, elapsed=%ldms"),
		*FString(channel), (unsigned long)uid, (long)elapsed);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didJoinedOfUid:(NSUInteger)uid elapsed:(NSInteger)elapsed {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: Remote user JOINED, uid=%lu"), (unsigned long)uid);
	FAgoraVoiceManager::Get().RemoteUserCount++;
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didOfflineOfUid:(NSUInteger)uid reason:(AgoraUserOfflineReason)reason {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: Remote user LEFT, uid=%lu reason=%d"), (unsigned long)uid, (int)reason);
	FAgoraVoiceManager::Get().RemoteUserCount = FMath::Max(0, FAgoraVoiceManager::Get().RemoteUserCount - 1);
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didOccurError:(AgoraErrorCode)errorCode {
	UE_LOG(LogPlayhouseCity, Error, TEXT("Agora ERROR: code=%d"), (int)errorCode);
	FAgoraVoiceManager::Get().LastErrorCode = (int32)errorCode;
}
- (void)rtcEngine:(AgoraRtcEngineKit *)engine didOccurWarning:(AgoraWarningCode)warningCode {
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora WARNING: code=%d"), (int)warningCode);
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
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: creating engine..."));
	GRtcEngine = [AgoraRtcEngineKit sharedEngineWithAppId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*AppId)] delegate:GDelegate];
	if (!GRtcEngine) {
		UE_LOG(LogPlayhouseCity, Error, TEXT("Agora: sharedEngineWithAppId FAILED"));
		return;
	}
	[GRtcEngine setChannelProfile:AgoraChannelProfileCommunication];
	[GRtcEngine enableAudio];
	[GRtcEngine disableVideo];
	[GRtcEngine setAudioProfile:AgoraAudioProfileDefault];
	[GRtcEngine setAudioScenario:AgoraAudioScenarioGameStreaming];
#if PLATFORM_IOS
	[GRtcEngine setDefaultAudioRouteToSpeakerphone:YES];
	[GRtcEngine setEnableSpeakerphone:YES];
#endif
	[GRtcEngine enableAudioVolumeIndication:2000 smooth:3 reportVad:YES];
	AgoraHandle = (__bridge void*)GRtcEngine;
	bInitialized = true;
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora Voice initialized - SUCCESS"));
}

void FAgoraVoiceManager::JoinChannel(const FString& ChannelName) {
	if (!bInitialized || bInChannel) return;

	// Fetch token from backend
	FString Url = FString::Printf(TEXT("https://tfplayhouse.com/api/agora/token/"));
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(FString::Printf(TEXT("{\"channel\":\"%s\",\"uid\":0}"), *ChannelName));

	FString CachedChannel = ChannelName;
	Req->OnProcessRequestComplete().BindLambda(
		[this, CachedChannel](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
			{
				TSharedPtr<FJsonObject> Json;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
				{
					FString Token = Json->GetStringField(TEXT("token"));
					JoinChannelWithToken(CachedChannel, Token);
					return;
				}
			}
			UE_LOG(LogPlayhouseCity, Error, TEXT("Agora: Failed to fetch token from backend, joining without token"));
			JoinChannelWithToken(CachedChannel, TEXT(""));
		});
	Req->ProcessRequest();
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: Fetching token for channel '%s'"), *ChannelName);
}

void FAgoraVoiceManager::JoinChannelWithToken(const FString& ChannelName, const FString& Token) {
	if (!bInitialized || bInChannel) return;
	if (GRtcEngine) {
		NSString* TokenStr = Token.IsEmpty() ? nil : [NSString stringWithUTF8String:TCHAR_TO_UTF8(*Token)];
		[GRtcEngine joinChannelByToken:TokenStr channelId:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*ChannelName)] info:nil uid:0 joinSuccess:nil];
		bInChannel = true; bMuted = false;
		UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora: Joining channel '%s' with %s token"), *ChannelName, Token.IsEmpty() ? TEXT("no") : TEXT("server"));
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

#endif // PLATFORM_IOS || PLATFORM_MAC
