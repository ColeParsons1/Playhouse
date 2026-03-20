// Copyright PlayhouseCity. All Rights Reserved.
// Agora C++ SDK implementation for non-Apple platforms (Windows, Android, Linux).
// Uses the native IRtcEngine C++ API which works across all platforms.

// Only compile on platforms where the C++ SDK is available.
// Currently gated behind AGORA_NATIVE_CPP — enable per-platform when SDK libs are added.
#if defined(AGORA_NATIVE_CPP)

#include "Voice/AgoraVoiceManager.h"
#include "PlayhouseCity.h"

THIRD_PARTY_INCLUDES_START
#include "AgoraBase.h"
#include "IAgoraRtcEngine.h"
THIRD_PARTY_INCLUDES_END

using namespace agora;
using namespace agora::rtc;

// Event handler that forwards Agora callbacks to UE logging + FAgoraVoiceManager state
class FPHCAgoraEventHandler : public IRtcEngineEventHandler
{
public:
	virtual void onJoinChannelSuccess(const char* channel, uid_t uid, int elapsed) override
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora C++: Joined channel '%hs', uid=%u, elapsed=%dms"),
			channel, uid, elapsed);
	}

	virtual void onUserJoined(uid_t uid, int elapsed) override
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora C++: Remote user JOINED, uid=%u"), uid);
		FAgoraVoiceManager::Get().RemoteUserCount++;
	}

	virtual void onUserOffline(uid_t uid, USER_OFFLINE_REASON_TYPE reason) override
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora C++: Remote user LEFT, uid=%u reason=%d"), uid, (int)reason);
		FAgoraVoiceManager::Get().RemoteUserCount = FMath::Max(0, FAgoraVoiceManager::Get().RemoteUserCount - 1);
	}

	virtual void onError(int err, const char* msg) override
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("Agora C++ ERROR: code=%d msg='%hs'"), err, msg ? msg : "");
		FAgoraVoiceManager::Get().LastErrorCode = err;
	}

	virtual void onLeaveChannel(const RtcStats& stats) override
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora C++: Left channel, duration=%us"), stats.duration);
	}
};

static IRtcEngine* GRtcEngine = nullptr;
static FPHCAgoraEventHandler GEventHandler;

FAgoraVoiceManager& FAgoraVoiceManager::Get()
{
	static FAgoraVoiceManager Instance;
	return Instance;
}

void FAgoraVoiceManager::Initialize(const FString& AppId)
{
	if (bInitialized) return;

	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora C++: Creating engine..."));

	GRtcEngine = createAgoraRtcEngine();
	if (!GRtcEngine)
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("Agora C++: createAgoraRtcEngine() FAILED"));
		return;
	}

	RtcEngineContext ctx;
	ctx.appId = TCHAR_TO_UTF8(*AppId);
	ctx.eventHandler = &GEventHandler;
	ctx.channelProfile = CHANNEL_PROFILE_COMMUNICATION;
#if PLATFORM_ANDROID
	// Android requires the JNI context
	extern void* GAndroidJNI_Activity;
	ctx.context = GAndroidJNI_Activity;
#else
	ctx.context = nullptr;
#endif

	int ret = GRtcEngine->initialize(ctx);
	if (ret != 0)
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("Agora C++: initialize() failed with code %d"), ret);
		GRtcEngine->release();
		GRtcEngine = nullptr;
		return;
	}

	GRtcEngine->enableAudio();
	GRtcEngine->setAudioProfile(AUDIO_PROFILE_DEFAULT);
	GRtcEngine->setAudioScenario(AUDIO_SCENARIO_GAME_STREAMING);
	GRtcEngine->enableAudioVolumeIndication(2000, 3, true);

	AgoraHandle = GRtcEngine;
	bInitialized = true;
	UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora C++: Voice initialized - SUCCESS"));
}

void FAgoraVoiceManager::JoinChannel(const FString& ChannelName)
{
	if (!bInitialized || bInChannel || !GRtcEngine) return;

	const char* TempToken = "REDACTED_AGORA_TEMP_TOKEN";

	int ret = GRtcEngine->joinChannel(TempToken, TCHAR_TO_UTF8(*ChannelName), nullptr, 0);
	if (ret == 0)
	{
		bInChannel = true;
		bMuted = false;
		UE_LOG(LogPlayhouseCity, Warning, TEXT("Agora C++: Joining channel '%s'"), *ChannelName);
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("Agora C++: joinChannel failed with code %d"), ret);
	}
}

void FAgoraVoiceManager::LeaveChannel()
{
	if (!bInChannel || !GRtcEngine) return;
	GRtcEngine->leaveChannel();
	bInChannel = false;
}

void FAgoraVoiceManager::SetMuted(bool bMute)
{
	bMuted = bMute;
	if (GRtcEngine)
	{
		GRtcEngine->muteLocalAudioStream(bMute);
	}
}

void FAgoraVoiceManager::Shutdown()
{
	if (!bInitialized || !GRtcEngine) return;
	GRtcEngine->leaveChannel();
	GRtcEngine->release();
	GRtcEngine = nullptr;
	AgoraHandle = nullptr;
	bInitialized = false;
	bInChannel = false;
}

#endif // AGORA_NATIVE_CPP
