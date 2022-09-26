// Copyright Baidu Inc. All Rights Reserved.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_json.h"
#include "baidu_rtc_websocket_client.h"

#define RTC_JANUS_TRANSACTION_ID_LEN         13
#define RTC_JANUS_TRANSACTION_MAP_LEN        8
#define RTC_JANUS_HANDLE_MAP_LEN             2
#define RTC_JANUS_PUBLISHER_LISTERN_MAP_LEN  2

enum { 
    DEFAULT_SIZE = 512,
    MIN_SIZE = 50,
};

struct FRTCConnection;
typedef void(*OnTransactionSuccess)(struct FRTCConnection* conn, char* data);
typedef void(*OnTransactionError)(struct FRTCConnection* conn, char* data);

typedef struct RtcJanusTransaction {
    char transactionId[RTC_JANUS_TRANSACTION_ID_LEN];
    OnTransactionSuccess onSuccess;
    OnTransactionError onError;
} RtcJanusTransaction;

struct RtcJanusHandle;
typedef void(*OnCreated)(struct FRTCConnection* conn, struct RtcJanusHandle* janusHanle);
typedef void(*OnJoined)(struct FRTCConnection* conn, struct RtcJanusHandle* janusHanle);
typedef void(*OnLeaving)(struct FRTCConnection* conn, struct RtcJanusHandle* janusHanle);
typedef void(*OnError)(struct FRTCConnection* conn, struct RtcJanusHandle* janusHanle);
typedef void(*OnRemoteJsep)(struct FRTCConnection* conn, struct RtcJanusHandle* janusHanle, char* data);

typedef struct RtcJanusHandle {
    unsigned long long handleId;
    unsigned long long feedId;
    OnCreated onCreated;
    OnJoined onJoined;
    OnLeaving onLeaving;
    OnError onError;
    OnRemoteJsep onRemoteJsep;
} RtcJanusHandle;


typedef struct {
    char jtId[RTC_JANUS_TRANSACTION_ID_LEN];
    RtcJanusTransaction* jt;
} janusTransactionMap;


typedef struct {
    unsigned long long jhId;
    RtcJanusHandle* jh;
} janusHandlesMap;

typedef struct {
    unsigned long long feedId;
    int count;
} publisherWebListenerMap;

// callback interface for `FRTCConnection`
struct IRTCConnectionObserver
{

    void (*onCirrusDisconnected)(void* handle);
    void (*onPublisherJoined)(void* handle, unsigned long long handleId);
    void (*onPublisherRemoteJsep)(void* handle, unsigned long long handleId, const char* jsep);
    void (*subscriberHandleRemoteJsep)(void* handle, unsigned long long handleId, const char* jsep);
    void (*onLeaving)(void* handle, unsigned long long handleId);
    void (*onListenerNACKs)(void* handle, int64_t nacks);
    void (*onNACKs)(void* handle, int64_t nacks);
    void (*onUnpublished)(void* handle);

    void (*onRTCLoginOK)(void* handle);
    void (*onRTCLoginTimeout)(void* handle);
    void (*onRTCLoginError)(void* handle);
    void (*onRTCConnectError)(void* handle);
    void (*onRTCWebrtcUp)(void* handle, unsigned long long handleId);
    void (*onRTCMediaStreamingEvent)(void* handle, unsigned long long handleId, int type, bool sending);
    void (*onRTCHangUp)(void* handle, unsigned long long handleId);
    void (*onRTCFeedComing)(void* handle, unsigned long long feedId, char* name);
    void (*onRTCUserMessage)(void* handle, unsigned long long feedId, char* msg);
    void (*onRTCUserAttribute)(void* handle, unsigned long long feedId, char* attr);
    void (*onRTCUserJoinedRoom)(void* handle, unsigned long long feedId, char* name);
    void (*onRTCUserLeavingRoom)(void* handle, unsigned long long feedId);
    void (*onRTCError)(void* handle, int error_code, char* error);
    void* handle;
};

struct FRTCConnection
{

    void (*connecting)(struct FRTCConnection* conn, const char* url);
    void (*disconnect)(struct FRTCConnection* conn);
    void (*onRTCOpen)(struct FRTCConnection* conn);
    void (*onRTCMessage)(struct FRTCConnection* conn, const char* data, size_t len);
    void (*onRTCClosed)(struct FRTCConnection* conn);
    void (*onRTCFailure)(struct FRTCConnection* conn);
    void (*onEvent)(struct FRTCConnection* conn, char* data, size_t len);
    void (*onSessionEvent)(struct FRTCConnection* conn, char* data, size_t len);
    void (*createSession)(struct FRTCConnection* conn);
    void (*publisherCreateHandle)(struct FRTCConnection* conn);
    void (*createRoom)(struct FRTCConnection* conn, unsigned long long handleId);
    void (*publisherJoinRoom)(struct FRTCConnection* conn, unsigned long long handleId);
    void (*publisherCreateOffer)(struct FRTCConnection* conn, unsigned long long handleId, void* Desc);
    void (*trickleCandicate)(struct FRTCConnection* conn, unsigned long long handleId, void* Candidate);
    void (*trickleCandicateComplete)(struct FRTCConnection* conn, unsigned long long handleId);
    void (*keepAlive)(struct FRTCConnection* conn);

    void (*subscriberCreateHandle)(struct FRTCConnection* conn, unsigned long long feedId, const char * display);
    void (*subscriberJoinRoom)(struct FRTCConnection* conn, unsigned long long handleId, unsigned long long feedId);
    void (*subscriberCreateAnswer)(struct FRTCConnection* conn, unsigned long long handleId, void* Desc);
    void (*subscriberConfig)(struct FRTCConnection* conn, unsigned long long handleId, bool audio, bool video,bool data);
    void (*unpublish)(struct FRTCConnection* conn, unsigned long long handleId);
    void (*detach)(struct FRTCConnection* conn, unsigned long long handleId);
    void(*sendMessage)(struct FRTCConnection* conn, char* buf);
    void (*sendMessageToUser)(struct FRTCConnection* conn, const char* msg, unsigned long long id, bool internal);

    void (*setUserId)(struct FRTCConnection* conn, const char* userId);
    void (*setRoomName)(struct FRTCConnection* conn, const char* roomName);
    void (*setAppId)(struct FRTCConnection* conn, const char* appId);
    void (*setTokenStr)(struct FRTCConnection* conn, const char* token);
    void (*setDisplayName)(struct FRTCConnection* conn, const char* displayRame);
    void (*setCerPath)(struct FRTCConnection* conn, const char* cerPath);
    void (*setAsPublisher)(struct FRTCConnection* conn, bool asPublisher);
    void (*setAsListener)(struct FRTCConnection* conn, bool asListener);
    void (*setFeedId)(struct FRTCConnection* conn, const char* feedIdStr);
    void (*setHasAudio)(struct FRTCConnection* conn, bool hasAudio);
    void (*setHasVideo)(struct FRTCConnection* conn, bool hasVideo);
    void (*setHasData)(struct FRTCConnection* conn, bool hasData);
    void (*setMediaServerIP)(struct FRTCConnection* conn, const char* mediaserverIP);
    void (*setSDKTag)(struct FRTCConnection* conn, const char* sdkTag);
    void (*setAutoSubscribe)(struct FRTCConnection* conn, bool autoSubscribe);
    void (*setVideoCodec)(struct FRTCConnection* conn, const char* videoCodec);
    void(*setObserver)(struct FRTCConnection* conn, struct IRTCConnectionObserver* observer);

    const char* mUserId;
    const char* mRoomName;
    const char* mAppId;
    const char* mTokenStr;
    const char* mDisplayName;
    const char* mCerPath;

    unsigned long long mSessionId;
    unsigned long long mRoomId;
    unsigned long long mFeedId;

    bool mAsPublisher;
    bool mAsListener;

    bool mMediaServerStandalone_needRoomID;
    char* mBRTCServerVersion;// brtc_v0 default as old janus, brtc_v1 for signalserver ARCH, brtc_v2 for multistream.
    volatile bool mFeedIdIsOnline;

    bool mHasAudio;
    bool mHasVideo;
    bool mHasData;
    bool mAutoSubscribe;
    const char* mVideoCodec;
    const char* mAudioCodec;
    const char* mMediaServerIP;
    const char* mSDKTag;
    const char* mLiveStreamingURL;

    struct IRTCConnectionObserver* mObserver;
    bool mRTCConnected;
    bool mbAliving;
    bool mbQuitWatchDog;

    janusTransactionMap mJanusTransactionMap[RTC_JANUS_TRANSACTION_MAP_LEN];
    janusHandlesMap mJanusHandlesMap[RTC_JANUS_HANDLE_MAP_LEN];
    publisherWebListenerMap mPublisherWebListenerMap[RTC_JANUS_PUBLISHER_LISTERN_MAP_LEN];

    baidurtc_user_data mUd;
    void* mWSCtx;
    void* mWSConnetion;
    char mFullURL[DEFAULT_SIZE];
    const char* mOriginURI;
};

void generateString(char* dest, const unsigned int len);
void FRTCConnection_construct(struct FRTCConnection* fRtcConn);

#ifdef __cplusplus
}
#endif