#include <string.h>
#include <stdlib.h>
#include "baidu_rtc_common_define.h"
#include "baidu_rtc_signal_client.h"
#include "utilities.h"
#include "baidu_rtc_client_session.h"

#include <unistd.h>   //调用 sleep() 函数
#include <pthread.h>  //调用 pthread_create() 函数

#define TIMER_POLL    50
#define EVENT_CNT     50
#define KEEPALIVE_CNT 500
#define KEEPALIVE_PERIOID  (KEEPALIVE_CNT*TIMER_POLL)  //25s

#ifndef BRTC_CPU_ARCH
#define SPACE_BRTC_CPU_ARCH
#else
#define SPACE_BRTC_CPU_ARCH " " BRTC_CPU_ARCH
#endif

#define BAIDU_RTC_SDK_VERSION   "3.0.1" SPACE_BRTC_CPU_ARCH

// #define    BRTC_SIG_TASK_SIZE      2048
// static OS_STK 			BRTCSigTaskStk[BRTC_SIG_TASK_SIZE];
// #define  BRTC_SIG_TASK_PRIO        35
// static tls_os_task_t signal_task = NULL;

static int sipInit = -1;
BaiduRtcClient* g_client = NULL;
void onCirrusDisconnected(void* handle) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }
}

void onPublisherJoined(void* handle, unsigned long long handleId) {
    uart_printf("%s: handle %lld is !\n", __func__, handleId);
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    g_client->mPulisherHandleId = handleId;
    if (g_client->mAutoPublish) {
        g_client->publishOwnFeed(g_client);
    }
}

void onPublisherRemoteJsep(void* handle, unsigned long long handleId, const char* jsep) {
    uart_printf("%s: handle %lld is !\n", __func__, handleId);
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    struct client_session *s = client_session_lookup(handleId);

    if (s) {
        size_t len = strlen(jsep);
        struct mbuf *msg = mbuf_alloc(len);
        mbuf_write_mem(msg, jsep, len);
        msg->pos = 0;
        client_session_set_remote_description(s, msg);
        mem_deref(msg);
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }
}

void onOffer(struct BaiduRtcClient* client, char *jsep) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    struct client_session *s;
    client_session_new(&s, client->mListernerHandleId, false);
    s->brtc_client = client;
    client_session_create_pc(s, SDP_ANSWER);

    if (s) {
        size_t len = strlen(jsep) +1;
        struct mbuf *msg = mbuf_alloc(len);
        mbuf_write_mem(msg, jsep, len);
        msg->pos = 0;
        client_session_set_remote_description(s, msg);
        mem_deref(msg);
        client_session_create_answer(s, SDP_ANSWER);
    }
}

void subscriberHandleRemoteJsep(void* handle, unsigned long long handleId, const char* jsep) {
    uart_printf("%s: handle %lld is !\n", __func__, handleId);
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    g_client->mListernerHandleId = handleId;
    onOffer(g_client, jsep);
}

void onLeaving(void* handle, unsigned long long handleId) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_ROOM_EVENT_REMOTE_LEAVING;
        msg.data.feedId = handleId;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onListenerNACKs(void* handle, int64_t nacks) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }
}

void onNACKs(void* handle, int64_t nacks) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_STATE_STREAM_SLOW_LINK_NACKS;
        msg.data.errorCode = nacks;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onUnpublished(void* handle) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }
}

void onRTCLoginOK(void* handle) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_ROOM_EVENT_LOGIN_OK;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onRTCLoginTimeout(void* handle) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_ROOM_EVENT_LOGIN_TIMEOUT;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }

}

void onRTCLoginError(void* handle) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_ROOM_EVENT_LOGIN_ERROR;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }

}

void onRTCConnectError(void* handle) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_ROOM_EVENT_LOGIN_ERROR;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onRTCWebrtcUp(void* handle, unsigned long long handleId) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (handleId == g_client->mPulisherHandleId) {
        struct client_session  *s=  client_session_lookup(g_client->mPulisherHandleId);

        if (s) {
            peerconnection_start_media(s->pc);
        }
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_STATE_STREAM_UP;
        msg.data.feedId = handleId;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }

}

void onRTCMediaStreamingEvent(void* handle, unsigned long long handleId, int type, bool sending) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = sending ? RTC_MESSAGE_STATE_SENDING_MEDIA_OK:RTC_MESSAGE_STATE_SENDING_MEDIA_FAILED;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onRTCHangUp(void* handle, unsigned long long handleId) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_STATE_STREAM_DOWN;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onRTCFeedComing(void* handle, unsigned long long feedId, char* name) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_ROOM_EVENT_REMOTE_COMING;
        msg.data.feedId = feedId;
        msg.extra_info = name;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onRTCUserMessage(void* handle, unsigned long long feedId, char* msg) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage m;
        m.msgType = RTC_ROOM_EVENT_ON_USER_MESSAGE;
        m.data.feedId = feedId;
        m.extra_info = msg;
        m.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&m);
    }
}

void onRTCUserAttribute(void* handle, unsigned long long feedId, char* attr) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_ROOM_EVENT_ON_USER_ATTRIBUTE;
        msg.data.feedId = feedId;
        msg.extra_info = attr;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onRTCUserJoinedRoom(void* handle, unsigned long long feedId, char* name) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_ROOM_EVENT_ON_USER_JOINED_ROOM;
        msg.data.feedId = feedId;
        msg.extra_info = name;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}

void onRTCUserLeavingRoom(void* handle, unsigned long long feedId) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_ROOM_EVENT_ON_USER_LEAVING_ROOM;
        msg.data.feedId = feedId;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }

    // if (sipInit == 0) {
    //     brtc_uninit_sip_client();
    //     sipInit = -1;
    // }
}

void onRTCError(void* handle, int error_code, char* error) {
    if (!g_client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!g_client->mObserver) {
        uart_printf("%s: g_client->mObserver is NULL!\n", __func__);
        return;
    }

    if (strcmp((char*)handle, g_client->mObserver->handle)) {
        return;
    }

    if (g_client->mRtcMessageListener) {
        RtcMessage msg;
        msg.msgType = RTC_MESSAGE_ROOM_EVENT_SERVER_ERROR;
        msg.data.errorCode = error_code;
        msg.extra_info = error;
        msg.obj = g_client->mRtcMessageListener;
        g_client->mRtcMessageListener(&msg);
    }
}
volatile int g_exit = 0;
void keepAlive_thread(void* arg) {
    int i = 0;
    g_exit = 0;
    struct BaiduRtcClient* client = (struct BaiduRtcClient*) arg;
    if (!client || !client->mFRTCconn || !client->mFRTCconn->mWSCtx || !client->mFRTCconn->mWSConnetion) {
        uart_printf(" %s: client(%p), mFRTCconn(%p), mWSCtx(%p), mWSConnetion(%p) is NULL!\n",
            __func__, client, client->mFRTCconn, client->mFRTCconn->mWSCtx, client->mFRTCconn->mWSConnetion);
        return;
    }

    for (i = 0; i < EVENT_CNT; i++) {
        brtc_websocket_timer_poll(client->mFRTCconn->mWSCtx, TIMER_POLL);
        if (client->mFRTCconn->mRTCConnected) {
            break;
        }
    }

    while (client->mFRTCconn->mRTCConnected) {
        client->mFRTCconn->mbAliving = false;
        client->mFRTCconn->keepAlive(client->mFRTCconn);
        for (i = 0; i < KEEPALIVE_CNT; i++) {
            brtc_websocket_timer_poll(client->mFRTCconn->mWSCtx, TIMER_POLL);
            if (g_exit) {
                uart_printf("keepAlive_thread exit 1\n");
                return;
            }
        }
        if (!client->mFRTCconn->mbAliving) {
            client->mFRTCconn->mObserver->onRTCConnectError(client->mFRTCconn->mObserver->handle);
            client->mFRTCconn->mObserver->onCirrusDisconnected(client->mFRTCconn->mObserver->handle);
            return;
        }
    }
    uart_printf("keepAlive_thread exit 0\n");
}

static bool init(struct BaiduRtcClient* client) {
    int err = 0;
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return false;
    }

    if (client->mIsInitialized) {
        uart_printf("%s: Baidu rtc client initialized yet!\n", __func__);
        return false;
    }

    if (client->mFRTCconn == NULL) {
        client->mFRTCconn = malloc(sizeof(struct FRTCConnection));
        if (client->mFRTCconn == NULL) {
            uart_printf("%s: FRTCConnection malloc failed!\n", __func__);
            return false;
        }
    }

    if (client->mObserver == NULL) {
        client->mObserver = malloc(sizeof(struct IRTCConnectionObserver));
        if (client->mObserver == NULL) {
            uart_printf("%s: client->mObserver malloc failed!\n", __func__);
            return false;
        }
    }
    memset(client->mFRTCconn, 0, sizeof(struct FRTCConnection));
    memset(client->mObserver, 0, sizeof(struct IRTCConnectionObserver));

    // sipInit = brtc_init_sip_client(client->mAppId, client->mTokenStr);
    FRTCConnection_construct(client->mFRTCconn);

    client->mObserver->onCirrusDisconnected = onCirrusDisconnected;
    client->mObserver->onPublisherJoined = onPublisherJoined;
    client->mObserver->onPublisherRemoteJsep = onPublisherRemoteJsep;
    client->mObserver->subscriberHandleRemoteJsep = subscriberHandleRemoteJsep;
    client->mObserver->onLeaving = onLeaving;
    client->mObserver->onRTCConnectError = onRTCConnectError;
    client->mObserver->onRTCError = onRTCError;
    client->mObserver->onRTCFeedComing = onRTCFeedComing;
    client->mObserver->onRTCHangUp = onRTCHangUp;
    client->mObserver->onRTCLoginError = onRTCLoginError;
    client->mObserver->onRTCLoginOK = onRTCLoginOK;
    client->mObserver->onRTCLoginTimeout = onRTCLoginTimeout;
    client->mObserver->onRTCMediaStreamingEvent = onRTCMediaStreamingEvent;
    client->mObserver->onRTCUserAttribute = onRTCUserAttribute;
    client->mObserver->onRTCUserJoinedRoom = onRTCUserJoinedRoom;
    client->mObserver->onRTCUserLeavingRoom = onRTCUserLeavingRoom;
    client->mObserver->onRTCUserMessage = onRTCUserMessage;
    client->mObserver->onRTCWebrtcUp = onRTCWebrtcUp;
    client->mObserver->onUnpublished = onUnpublished;
    client->mObserver->onNACKs = onNACKs;
    client->mObserver->handle = malloc(RTC_OBSERVER_HANDLE_ID_LEN);
    generateString((char*)client->mObserver->handle, RTC_OBSERVER_HANDLE_ID_LEN);
    client->mFRTCconn->setObserver(client->mFRTCconn, client->mObserver);
    // tls_os_task_create(&signal_task, NULL,
    //                    keepAlive_thread,
    //                    client,
    //                    (void *)BRTCSigTaskStk,          /* task's stack start address */
    //                    BRTC_SIG_TASK_SIZE * sizeof(OS_STK), /* task's stack size, unit:byte */
    //                    BRTC_SIG_TASK_PRIO,
    //                    0);
    // if (signal_task == NULL) {
    //     uart_printf("task creat failed (%d)\n");
    //     return false;
    // }

    // vTaskDelay(20 / portTICK_PERIOD_MS);
    client->mIsInitialized = true;
    return true;
}

static void deInit(struct BaiduRtcClient* client) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (!client->mIsInitialized) {
        uart_printf(" %s: It didn't initialize yet!\n", __func__);
        return;
    }

    // if (sipInit == 0) {
    //     brtc_uninit_sip_client();
    //     sipInit = -1;
    // }

    if (client->mFRTCconn->mRTCConnected) {
        client->mFRTCconn->disconnect(client->mFRTCconn);
    }

    if (client->mIsLoginRoom) {
        client->mIsLoginRoom = false;
    }

    if (client->mIsInitialized) {
        client->mIsInitialized = false;
    }

    // if (signal_task) {
    //     tls_os_task_suspend(signal_task);
    //     signal_task = NULL;
    // }

    uart_printf(" %s: is ok\n", __func__);
}

bool loginRoom(struct BaiduRtcClient* client, const char* roomName, const char* userId, const char* displayName, const char* token) {
    int err = 0;
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return false;
    }

    if (client->mIsLoginRoom) {
        uart_printf("%s: it has logined room yet!\n", __func__);
        return true;
    }
    if (!client->mFRTCconn) {
        uart_printf("%s: client->mFRTCconn is NULL\n", __func__);
        return false;
    }

    client->mFRTCconn->setSDKTag(client->mFRTCconn, "BRTC.RTOS.SDK V" BAIDU_RTC_SDK_VERSION);
    client->mFRTCconn->setAppId(client->mFRTCconn, client->mAppId);
    client->mFRTCconn->setTokenStr(client->mFRTCconn, client->mTokenStr);
    client->mFRTCconn->setCerPath(client->mFRTCconn, client->mCer);
    client->mFRTCconn->setRoomName(client->mFRTCconn, client->mRoomName);
    client->mFRTCconn->setUserId(client->mFRTCconn, client->mUserId);
    client->mFRTCconn->setCerPath(client->mFRTCconn, client->mCer);
    client->mFRTCconn->setFeedId(client->mFRTCconn, client->mFeedID); //mFeedID/mVideoCodec need dup
    client->mFRTCconn->setVideoCodec(client->mFRTCconn, client->mVideoCodec);
    client->mFRTCconn->setHasAudio(client->mFRTCconn, client->mParamSettings.HasAudio);
    client->mFRTCconn->setHasVideo(client->mFRTCconn, client->mParamSettings.HasVideo);
    client->mFRTCconn->setHasData(client->mFRTCconn, client->mParamSettings.HasData);
    client->mFRTCconn->setMediaServerIP(client->mFRTCconn, client->mMediaServerIP);
    client->mFRTCconn->setAutoSubscribe(client->mFRTCconn, client->mAutoSubscribe);
    client->mFRTCconn->setAsListener(client->mFRTCconn, client->mParamSettings.AsListener);
    client->mFRTCconn->setAsPublisher(client->mFRTCconn, client->mParamSettings.AsPublisher);

    client->mParamSettings.HasVideo = false;

    client->mFRTCconn->mRoomName = roomName;
    client->mFRTCconn->mUserId = userId;
    client->mFRTCconn->mTokenStr = token;
    client->mFRTCconn->setHasVideo(client->mFRTCconn, false);
    client->mFRTCconn->setDisplayName(client->mFRTCconn, displayName);

    client->mFRTCconn->connecting(client->mFRTCconn, client->mMediaServerURL);

    pthread_t myThread1;
    // usleep(500);
    pthread_create(&myThread1, NULL, keepAlive_thread, client);

    // if (sipInit == 0) {
    //     err = brtc_connect_sip(client->mRoomName, client->mUserId, client->mParamSettings.HasVideo?1:0);
    //     sipInit = -1;
    // }

    if (err == 0) {
        client->mIsLoginRoom = true;
        uart_printf("%s: success\n", __func__);
        return true;
    } else {
        client->mIsLoginRoom = false;
        uart_printf("%s: failed\n", __func__);
        return false;
    }
}

bool logoutRoom(struct BaiduRtcClient* client) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return false;
    }

    if (!client->mIsLoginRoom) {
        uart_printf("%s: it didn't login room!\n", __func__);
        return true;
    }

    // if (sipInit == 0) {
    //     brtc_uninit_sip_client();
    //     sipInit = -1;
    // }

/*
    brtc_hangup_sip();*/
    if (client->mFRTCconn->mRTCConnected) {
        client->mFRTCconn->disconnect(client->mFRTCconn);
        g_exit = 1;
    }

    if (client->mIsLoginRoom) {
        client->mIsLoginRoom = false;
    }
    uart_printf("%s: success\n", __func__);
    return true;
}

void setParamSettings(struct BaiduRtcClient* client, 
                        RtcParamSettingType paramType, RtcParameterSettings* paramSettings) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    switch (paramType) {
    case RTC_PARAM_SETTINGS_ALL:
        if (paramSettings != NULL) {
            memcpy(&client->mParamSettings, paramSettings, sizeof(RtcParameterSettings));
        }
        break;
    case RTC_VIDEO_PARAM_SETTINGS_BITRATE:
        if (paramSettings != NULL) {
            client->mParamSettings.VideoMaxkbps = paramSettings->VideoMaxkbps;
        }
        break;
    default:
	break;
    }
}

void setFeedId(struct BaiduRtcClient* client, const char* feedId) {
    if (client) {
        client->mFeedID = feedId;
    }
}

void setCER(struct BaiduRtcClient* client, const char* cerFile) {
    if (client) {
        client->mCer = cerFile;
    }
}

void setVideoCodec(struct BaiduRtcClient* client, const char* vc) {
    int err = 0;
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (client->mFRTCconn) {
        if (!strncmp(vc, "h263", 4)) {
            client->mVideoCodecImageType = RTC_IMAGE_TYPE_H263;
        }else if (!strncmp(vc, "h264", 4)) {
            client->mVideoCodecImageType = RTC_IMAGE_TYPE_H264;
        }

        if (vc) {
            client->mVideoCodec = strdup(vc);
        }
        client->mFRTCconn->setVideoCodec(client->mFRTCconn, client->mVideoCodec);
    }
}

void setMediaServerURL(struct BaiduRtcClient* client, const char* url) {
    int err = 0;
    if (client && url) {
        client->mMediaServerURL = url;
    }
}

void setAppID(struct BaiduRtcClient* client, const char* appid) {
    if (appid) {
        client->mAppId = appid;
    }
}

void setMediaServerIP(struct BaiduRtcClient* client, const char* ip) {
    if (client) {
        client->mMediaServerIP = ip;
    }
}

void sendAudio(struct BaiduRtcClient* client, const char* data, int len) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }
}

void sendImage(struct BaiduRtcClient* client, const char* data, int len) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

}

void sendData(struct BaiduRtcClient* client, const char* data, int len) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

}

void sendMessageToUser(struct BaiduRtcClient* client, const char* msg, const char* id) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (client->mFRTCconn) {
        client->mFRTCconn->sendMessageToUser(client->mFRTCconn, msg, atoll(id), false);
    }
}

void registerRtcMessageListener(struct BaiduRtcClient* client, IRtcMessageListener msgListener) {
    if (client) {
        client->mRtcMessageListener = msgListener;
    }
}

void registerVideoFrameObserver(struct BaiduRtcClient* client, IVideoFrameObserver iVfo, int iVfoNum) {
    if (client) {
        client->mVideoFrameObserver = iVfo;
    }
}

void registerAudioFrameObserver(struct BaiduRtcClient* client, IAudioFrameObserver iAfo, int iAfoNum) {
    if (client) {
        client->mAudioFrameObserver = iAfo;
    }
}

void registerDataFrameObserver(struct BaiduRtcClient* client, IDataFrameObserver* iDfo[], int iDfoNum) {
    if (client) {
        client->mDataFrameObserver = iDfo[0];
    }
}

void subscribeStreaming(struct BaiduRtcClient* client, const char* feedId, IAudioFrameObserver* afo,
    IVideoFrameObserver* vfo, IDataFrameObserver* dfo) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }
    client->mFeedID = feedId;
    if (client->mFRTCconn) {
        client->mFRTCconn->subscriberCreateHandle(client->mFRTCconn, atoll(feedId), "subscribeStreaming");
    }
}

void stopSubscribeStreaming(struct BaiduRtcClient* client, const char* feedId) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (client->mFRTCconn) {
        client->mFRTCconn->detach(client->mFRTCconn, client->mListernerHandleId);
    }
}

void getRoomStates(struct BaiduRtcClient* client, RtcRoomUserInfo** userInfoList, int* userNumber) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

}

ConnectionStats getConnectionStates(struct BaiduRtcClient* client, ConnectionType connectionType) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return RTC_CONNECTION_STATS_UNKNOW;
    }

    return RTC_CONNECTION_STATS_UNKNOW;
}

void publishOwnFeed(struct BaiduRtcClient* client) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    struct client_session *s;
    client_session_new(&s, client->mPulisherHandleId, true);
    s->brtc_client = client;
    client_session_create_pc(s, SDP_OFFER);
    client_session_create_offer(s, SDP_OFFER);

    // sleep(15);
    uart_printf("publishOwnFeed: exit\n");

 //   client->mFRTCconn->publisherCreateOffer(client->mFRTCconn, client->mPulisherHandleId, offerSdp);

    // CreateClient(mPulisherHandleId,true);
    // AddStreams(mPulisherHandleId);
    // createOffer(mPulisherHandleId);
}

void startPublish(struct BaiduRtcClient* client) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }
    publishOwnFeed(client);
}

void stopPublish(struct BaiduRtcClient* client) {
    if (!client) {
        uart_printf("%s: BaiduRtcClient is NULL!\n", __func__);
        return;
    }

    if (client->mFRTCconn) {
        client->mFRTCconn->unpublish(client->mFRTCconn, client->mPulisherHandleId);
    }
}

void destoryClient(struct BaiduRtcClient* client) {
    if (client->mFRTCconn) {
        free(client->mFRTCconn);
        client->mFRTCconn = NULL;
    }

    if (client->mObserver->handle) {
        free(client->mObserver->handle);
        client->mObserver->handle = NULL;
    }

    if (client->mObserver) {
        free(client->mObserver);
        client->mObserver = NULL;
    }

    client->mAppId = NULL;
    client->mTokenStr = NULL;
    client->mCer = NULL;
    client->mRoomName = NULL;
    client->mUserId = NULL;
    client->mMediaServerURL = NULL;
    client->mVideoCodec = NULL;

    if (client) {
        free(client);
        client = NULL;
    }
    uart_printf("%s: success\n", __func__);
}

BaiduRtcClient* createClient() {

    g_client = malloc(sizeof(struct BaiduRtcClient));
    g_client->init = init;
    g_client->deInit = deInit;
    g_client->loginRoom = loginRoom;
    g_client->logoutRoom = logoutRoom;
    g_client->setParamSettings = setParamSettings;
    g_client->setFeedId = setFeedId;
    g_client->setVideoCodec = setVideoCodec;
    g_client->setCER = setCER;
    g_client->setMediaServerURL = setMediaServerURL;
    g_client->setAppID = setAppID;
    g_client->setMediaServerIP = setMediaServerIP;
    g_client->sendAudio = sendAudio;
    g_client->sendImage = sendImage;
    g_client->sendData = sendData;
    g_client->sendMessageToUser = sendMessageToUser;
    g_client->registerRtcMessageListener = registerRtcMessageListener;
    g_client->registerVideoFrameObserver = registerVideoFrameObserver;
    g_client->registerAudioFrameObserver = registerAudioFrameObserver;
    g_client->registerDataFrameObserver = registerDataFrameObserver;
    g_client->startPublish = startPublish;
    g_client->stopPublish = stopPublish;
    g_client->destoryClient = destoryClient;
    g_client->publishOwnFeed = publishOwnFeed;
    g_client->subscribeStreaming = subscribeStreaming;
    g_client->stopSubscribeStreaming = stopSubscribeStreaming;
    g_client->getRoomStates = getRoomStates;
    g_client->getConnectionStates = getConnectionStates;
    g_client->mFRTCconn = NULL;
    g_client->mIsInitialized = false;
    g_client->mIsLoginRoom = false;
    g_client->mAutoSubscribe = true;
    g_client->mAutoPublish = true;
    g_client->mCer = "";

    g_client->mFeedID = "0";
    g_client->mSDKTag = "v1.0";
    g_client->mMediaServerURL = "wss://rtc.exp.bcelive.com/janus";
    g_client->mMediaServerIP = "";
    g_client->mVideoCodec = "h263";
    g_client->mObserver = NULL;
    g_client->mDataFrameObserver = NULL;
    g_client->mListernerHandleId = 0;
    g_client->mPulisherHandleId = 0;
    g_client->mSipclient = false;

    memset(&g_client->mRtcMessageListener, 0, sizeof(IRtcMessageListener));
    memset(&g_client->mVideoFrameObserver, 0, sizeof(IVideoFrameObserver));
    memset(&g_client->mAudioFrameObserver, 0, sizeof(IAudioFrameObserver));
    memcpy(&g_client->mParamSettings, &defaultSetting, sizeof(RtcParameterSettings));

    return g_client;
}
