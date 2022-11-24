#ifndef BRTC_RTC_SIG_H_
#define BRTC_RTC_SIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "baidu_rtc_interface.h"
#include "RTCConnection.h"

#define RTC_OBSERVER_HANDLE_ID_LEN    10

/*
 *
 *  连接类型定义
 *
 **/
typedef enum ConnectionType {
    RTC_SIGNAL_CHANNEL_CONNECTION,
    RTC_MEDIA_CHANNEL_CONNECTION,
}ConnectionType;

/*
 *
 *  网络连接状态
 *
 **/
typedef enum ConnectionStats {
    RTC_CONNECTION_STATS_UNKNOW = 400,
    RTC_CONNECTION_STATS_DISCONNECTED,
    RTC_CONNECTION_STATS_CONNECTING,
    RTC_CONNECTION_STATS_CONNECTED,
    RTC_CONNECTION_STATS_RECONNECTING,
    RTC_CONNECTION_STATS_FAILED,
}ConnectionStats;

/*
 *
 * 房间用户角色
 *
 * @param RTC_ROOM_USER_ROLE_PUBLISHER, 角色为流发布者
 * @param RTC_ROOM_USER_ROLE_LISTENER, 角色为流监听者
 *
 **/
typedef enum RtcRoomUserRole {
    RTC_ROOM_USER_ROLE_PUBLISHER,
    RTC_ROOM_USER_ROLE_LISTENER,
}RtcRoomUserRole;

/*
 *
 * 房间用户信息
 *
 * @param userRole, 用户角色
 * @param userId, 用户id
 * @param roomId, 房间id
 *
 **/
typedef struct RtcRoomUserInfo {
    RtcRoomUserRole userRole;
    int64_t userId;
    int64_t roomId;
}RtcRoomUserInfo;

typedef enum RtcImageType {
    RTC_IMAGE_TYPE_NONE,
    RTC_IMAGE_TYPE_JPEG,
    RTC_IMAGE_TYPE_H263,
    RTC_IMAGE_TYPE_H264,
    RTC_IMAGE_TYPE_I420P,
    RTC_IMAGE_TYPE_RGB,
    RTC_IMAGE_TYPE_VP8,
}RtcImageType;

typedef enum RtcParamSettingType {
    RTC_AUDIO_PARAM_SETTINGS_ONLY_AUDIO,
    RTC_AUDIO_PARAM_SETTINGS_AEC_DUMP,
    RTC_AUDIO_PARAM_SETTINGS_LEVEL_CONTROL,
    RTC_AUDIO_PARAM_SETTINGS_MANUAL_CONFIG,
    RTC_AUDIO_PARAM_SETTINGS_EXPORT_RECORD,
    RTC_AUDIO_PARAM_SETTINGS_EXPORT_PLAYOUT,
    RTC_AUDIO_PARAM_SETTINGS_EXPORT_RECORD_PLAYOUT_MIX,
    RTC_VIDEO_PARAM_SETTINGS_FPS,
    RTC_VIDEO_PARAM_SETTINGS_RESOLUTION,
    RTC_VIDEO_PARAM_SETTINGS_BITRATE,
    RTC_VIDEO_PARAM_SETTINGS_CODECTYPE,
    RTC_VIDEO_PARAM_SETTINGS_SESSION_MODE,
    RTC_VIDEO_PARAM_SETTINGS_CAPTURE_MODE,
    RTC_VIDEO_PARAM_SETTINGS_RENDER_MODE,
    RTC_PARAM_SETTINGS_ALL,
    RTC_PARAM_SETTINGS_OTHERS
}RtcParamSettingType;

typedef struct RtcParameterSettings {
    bool     HasVideo;
    bool     HasAudio;
    bool     HasData;
    int     AudioINFrequency;
    int     AudioINChannel;
    int     AudioOUTFrequency;
    int     AudioOUTChannel;
    int        VideoWidth;
    int        VideoHeight;
    int     VideoFps;
    int     VideoMaxkbps;
    int     VideoMinkbps;
    int     ImageINType;
    int     ImageOUTType;
    int     ConnectionTimeoutMs;
    int     ReadTimeoutMs;
    bool    AutoPublish;
    bool    AutoSubscribe;
    bool    AsPublisher;
    bool    AsListener;    
}RtcParameterSettings;

static struct RtcParameterSettings defaultSetting = {
    true,
    true,
    false,
    48000,
    1,
    16000,
    1,
    176,
    144,
    20,
    1500,
    1000,
    RTC_IMAGE_TYPE_NONE,
    RTC_IMAGE_TYPE_H263,
    5000,
    5000,
    true,
    true,
    true,
    true,
};

static RtcParameterSettings* getDefaultSettings() {
        return &defaultSetting;
}

typedef void(*IVideoFrameObserver)(int64_t feedid, const char *img, int len, RtcImageType imgtype, int width, int height);
typedef void(*IAudioFrameObserver)(int64_t feedid, const char *audio, int len, int samlplerate, int channels);
typedef void(*IDataFrameObserver)(int64_t feedid, const char *data, int len);

typedef struct BaiduRtcClient {
    bool (*init)(struct BaiduRtcClient* client);
    void (*deInit)(struct BaiduRtcClient* client);

    bool (*loginRoom)(struct BaiduRtcClient* client, const char* roomName, const char* userId, const char* displayName, const char* token);
    bool (*logoutRoom)(struct BaiduRtcClient* client);

    void (*setParamSettings)(struct BaiduRtcClient* client, RtcParamSettingType paramType, RtcParameterSettings* paramSettings);
    void (*setFeedId)(struct BaiduRtcClient* client, const char* feedId);
    void (*setVideoCodec)(struct BaiduRtcClient* client, const char* vc);

    void (*setCER)(struct BaiduRtcClient* client, const char *cerFile);
    void (*setMediaServerURL)(struct BaiduRtcClient* client, const char* url);
    void (*setAppID)(struct BaiduRtcClient* client, const char* appid);
    void (*setMediaServerIP)(struct BaiduRtcClient* client, const char* ip);
    void (*sendAudio)(struct BaiduRtcClient* client, const char* data, int len);
    void (*sendImage)(struct BaiduRtcClient* client, const char* data, int len);
    void (*sendData)(struct BaiduRtcClient* client, const char* data, int len);
    void (*sendMessageToUser)(struct BaiduRtcClient* client, const char* msg, const char* id);

    void (*registerRtcMessageListener)(struct BaiduRtcClient* client, IRtcMessageListener msgListener);
    void (*registerVideoFrameObserver)(struct BaiduRtcClient* client, IVideoFrameObserver iVfo, int iVfoNum);
    void (*registerAudioFrameObserver)(struct BaiduRtcClient* client, IAudioFrameObserver iAfo, int iAfoNum);
    void (*registerDataFrameObserver)(struct BaiduRtcClient* client, IDataFrameObserver* iDfo[], int iDfoNum);

    void (*startPublish)(struct BaiduRtcClient* client);
    void (*stopPublish)(struct BaiduRtcClient* client);
    void(*destoryClient)(struct BaiduRtcClient* client);

    void (*publishOwnFeed)(struct BaiduRtcClient* client);

    void (*subscribeStreaming)(struct BaiduRtcClient* client, const char * feedId, IAudioFrameObserver* afo, IVideoFrameObserver* vfo, IDataFrameObserver* dfo);
    void (*stopSubscribeStreaming)(struct BaiduRtcClient* client, const char* feedId);

    void (*getRoomStates)(struct BaiduRtcClient* client, RtcRoomUserInfo** userInfoList, int* userNumber);
    ConnectionStats (*getConnectionStates)(struct BaiduRtcClient* client, ConnectionType connectionType);

    bool mIsInitialized;
    bool mIsLoginRoom;
    struct FRTCConnection* mFRTCconn;
    struct IRTCConnectionObserver* mObserver;
    IRtcMessageListener mRtcMessageListener;
    IVideoFrameObserver mVideoFrameObserver;
    IAudioFrameObserver mAudioFrameObserver;
    IDataFrameObserver* mDataFrameObserver;

    const char* mUserId;
    const char* mRoomName;
    const char* mAppId;
    const char* mTokenStr;

    const char* mCer;
    const char* mFeedID;
    const char* mSDKTag;
    const char* mMediaServerURL;
    const char* mMediaServerIP;
    const char* mVideoCodec;
    bool mAutoSubscribe;
    bool mSipclient;
    unsigned long long mListernerHandleId; //need to get value by subscriberHandleRemoteJsep
    unsigned long long mPulisherHandleId;
    bool mAutoPublish;
    RtcImageType mVideoCodecImageType;
    RtcParameterSettings mParamSettings;
}BaiduRtcClient;

BaiduRtcClient* createClient();

int baidu_rtc_signal_timer_poll(BaiduRtcClient* client);
int baidu_rtc_signal_keepalive(BaiduRtcClient* client);
int baidu_rtc_signal_keepalive_check_result(BaiduRtcClient* client);

#ifdef __cplusplus
}
#endif

#endif // BRTC_RTC_SIG_H_