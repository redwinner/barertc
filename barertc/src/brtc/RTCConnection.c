#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "mongoose.h"
#include "RTCConnection.h"
#include "utilities.h"

static unsigned long long count = 0;
const char r[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
void generateString(char* dest, const unsigned int len) {
    srand((unsigned int)mg_millis());
    unsigned int cnt = 0;
    for (cnt = 0; cnt < len-1; cnt++)
    {
        *dest = r[rand() % 62];
        dest++;
    }
    *dest = '\0';
}

static void RTCConnection_baidurtc_client(void *c, int ev, void *ev_data, void *fn_data) {
    struct FRTCConnection* conn = (struct FRTCConnection*) fn_data;
    if (conn) {
        if (ev == MG_EV_WS_OPEN) {
            printf("MG_EV_WS_OPEN, conn：%p appId addr:%p, appId: %s, mRoomName addr:%p, mRoomName:%s, mUserId:%p, mUserId:%s\n", 
                conn, conn->mAppId, conn->mAppId,conn->mRoomName, conn->mRoomName, conn->mUserId, conn->mUserId);
            conn->onRTCOpen(conn);
        } else if (ev == MG_EV_CONNECT) {
            struct mg_str host = mg_url_host(conn->mFullURL); 
            printf("MG_EV_CONNECT: [%.*s]\n", host.len, host.ptr);
            // If s_url is https://, tell client connection to use TLS
            if (mg_url_is_ssl(conn->mFullURL)) {
                struct mg_tls_opts opts = {.ca = conn->mCerPath,.srvname = host};
                mg_tls_init(c, &opts);
            }
        } else if (ev == MG_EV_WS_MSG) {
            struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
            // uart_printf("GOT ECHO REPLY: %d, [%.*s]\n", wm->data.len, (int)wm->data.len, wm->data.ptr);
            printf("rx: %.*s.\n", (int)wm->data.len, wm->data.ptr);
            conn->onRTCMessage(conn, wm->data.ptr, wm->data.len);
        } else if (ev == MG_EV_CLOSE) {
            uart_printf("MG_EV_CLOSE, conn:%p\n", conn);
            conn->onRTCClosed(conn);
        } else if (ev == MG_EV_ERROR) {
            uart_printf("MG_EV_ERROR, conn:%p\n", conn);
            conn->onRTCClosed(conn);
        }

        if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
            uart_printf("close done, conn:%p\n", conn);
            fn_data = NULL;  // Signal that we're done
        }

    }

}

static void createSession_callback(struct FRTCConnection* conn, char* data) {

    JSONStatus_t result = JSONSuccess;
    char* buf = data;
    size_t bufLength = strlen(data);
    char sessionIdQuery[] = "data.id";
    size_t sqLength = strlen(sessionIdQuery);
    char* sessionIdValue = NULL;
    size_t svLength = 0;
    result = JSON_Validate(buf, bufLength);
    if (result != JSONSuccess) {
        uart_printf("createSession_callback input buffer invalidate\n");
        return;
    }

    result = JSON_Search(buf, bufLength, sessionIdQuery, sqLength, &sessionIdValue, &svLength);
    if (result == JSONSuccess) {
        char saveSessionId[MIN_SIZE] = "";
        strncpy(saveSessionId, sessionIdValue, svLength);
        conn->mSessionId = atoll(saveSessionId);
    }

    char needRoomIdQuery[] = "standalone";
    size_t nqLength = strlen(needRoomIdQuery);
    char* needRoomIdValue = NULL;
    size_t nvLength = 0;
    result = JSON_Search(buf, bufLength, needRoomIdQuery, nqLength, &needRoomIdValue, &nvLength);
    if (result == JSONSuccess) {
        if (!strncmp(needRoomIdValue, "true", nvLength)) {
            conn->mMediaServerStandalone_needRoomID = true;
        }else {
            conn->mMediaServerStandalone_needRoomID = false;
        }
    }

    char serverVersionQuery[] = "data.version";
    size_t svqLength = strlen(serverVersionQuery);
    char* serverVersionValue = NULL;
    size_t svvLength = 0;
    result = JSON_Search(buf, bufLength, serverVersionQuery, svqLength, &serverVersionValue, &svvLength);
    if (result == JSONSuccess) {
        char saveVersionValue[MIN_SIZE] = "";
        strncpy(saveVersionValue, serverVersionValue, svvLength);
        conn->mBRTCServerVersion = saveVersionValue;
    }
    conn->publisherCreateHandle(conn);
}

static void publisherCreateHandle_callback(struct FRTCConnection* conn, char* data) {
    JSONStatus_t result = JSONSuccess;
    char* buf = data;
    size_t bufLength = strlen(buf);
    char handleIdQuery[] = "data.id";
    size_t hidqLength = strlen(handleIdQuery);
    char* handleIdValue = NULL;
    size_t hidvLength = 0;
    result = JSON_Validate(buf, bufLength);
    if (result != JSONSuccess) {
        uart_printf("publisherCreateHandle_callback input buffer invalidate\n");
        return;
    }

    result = JSON_Search(buf, bufLength, handleIdQuery, hidqLength, &handleIdValue, &hidvLength);
    if (result == JSONSuccess) {
        char saveHandleId[MIN_SIZE] = "";
        strncpy(saveHandleId, handleIdValue, hidvLength);
        unsigned long long handleId = atoll(saveHandleId);
        conn->createRoom(conn, handleId);
    }
    uart_printf("%s: is ok\n", __func__);

}

static void onPublisherJoined_callback(struct FRTCConnection* conn, struct RtcJanusHandle* janusHanle) {
    conn->mObserver->onPublisherJoined(conn->mObserver->handle, janusHanle->handleId);
    uart_printf("%s: is ok\n", __func__);
}

static void onPublisherRemoteJsep_callback(struct FRTCConnection* conn, struct RtcJanusHandle* janusHanle, char* data) {
    JSONStatus_t result = JSONSuccess;
    char* buf = data;
    size_t bufLength = strlen(buf);
    char jsepQuery[] = "jsep";
    size_t jsepqLength = strlen(jsepQuery);
    char* jsepValue = NULL;
    size_t jsepvLength = 0;
    result = JSON_Validate(buf, bufLength);
    if (result != JSONSuccess) {
        uart_printf("onPublisherRemoteJsep_callback input buffer invalidate\n");
        return;
    }

    result = JSON_Search(buf, bufLength, jsepQuery, jsepqLength, &jsepValue, &jsepvLength);
    if (result == JSONSuccess) {
        char saveJsep[MIN_SIZE * 100] = "";
        strncpy(saveJsep, jsepValue, jsepvLength);
        conn->mObserver->onPublisherRemoteJsep(conn->mObserver->handle, janusHanle->handleId, saveJsep);
    }
    uart_printf("%s: is ok\n", __func__);
}

static void subscriberHandleRemoteJsep_callback(struct FRTCConnection* conn,
                                                            struct RtcJanusHandle* janusHanle, char* data) {
    JSONStatus_t result = JSONSuccess;
    char* buf = data;
    size_t bufLength = strlen(buf);
    char jsepQuery[] = "jsep";
    size_t jsepqLength = strlen(jsepQuery);
    char* jsepValue = NULL;
    size_t jsepvLength = 0;
    result = JSON_Validate(buf, bufLength);
    if (result != JSONSuccess) {
        uart_printf("subscriberHandleRemoteJsep_callback input buffer invalidate\n");
        return;
    }

    result = JSON_Search(buf, bufLength, jsepQuery, jsepqLength, &jsepValue, &jsepvLength);
    if (result == JSONSuccess) {
        char saveJsep = jsepValue[jsepvLength];
        jsepValue[jsepvLength] = '\0';
        conn->mObserver->subscriberHandleRemoteJsep(conn->mObserver->handle, janusHanle->handleId, jsepValue);
        jsepValue[jsepvLength] = saveJsep;
    }
    uart_printf("%s: is ok\n", __func__);
}

static void createRoom_callback(struct FRTCConnection* conn, char* data) {

    JSONStatus_t result = JSONSuccess;
    char* buf = data;
    size_t bufLength = strlen(data);
    char handleIdQuery[] = "sender";
    size_t hidqLength = strlen(handleIdQuery);
    char* handleIdValue = NULL;
    size_t hidvLength = 0;
    result = JSON_Validate(buf, bufLength);
    if (result != JSONSuccess) {
        uart_printf("createRoom_onSuccess input buffer invalidate\n");
        return;
    }

    unsigned long long handleId = 0;
    result = JSON_Search(buf, bufLength, handleIdQuery, hidqLength, &handleIdValue, &hidvLength);
    if (result == JSONSuccess) {
        char saveHandleId[MIN_SIZE] = "";
        strncpy(saveHandleId, handleIdValue, hidvLength);
        handleId = atoll(saveHandleId);
    }

    char roomIdQuery[] = "plugindata.data.room";
    size_t ridqLength = strlen(roomIdQuery);
    char* roomIdValue = NULL;
    size_t ridvLength = 0;
    result = JSON_Search(buf, bufLength, roomIdQuery, ridqLength, &roomIdValue, &ridvLength);
    if (result == JSONSuccess) {
        char saveRoomId[MIN_SIZE] = "";
        strncpy(saveRoomId, roomIdValue, ridvLength);
        conn->mRoomId = atoll(saveRoomId);
    }

    char saveJsep[MIN_SIZE] = "";
    char jsepQuery[] = "jsep";
    size_t jsepqLength = strlen(jsepQuery);
    char* jsepValue = NULL;
    size_t jsepvLength = 0;
    result = JSON_Search(buf, bufLength, jsepQuery, jsepqLength, &jsepValue, &jsepvLength);
    if (result == JSONSuccess) {
        strncpy(saveJsep, jsepValue, jsepvLength);
    }

    RtcJanusHandle* jh = malloc(sizeof(struct RtcJanusHandle));
    jh->feedId = conn->mFeedId;
    jh->handleId = handleId;
    conn->mJanusHandlesMap[0].jhId = handleId;
    conn->mJanusHandlesMap[0].jh = jh;
    if (conn->mAsPublisher) {
        //sending and receiving streaming
        jh->onJoined = onPublisherJoined_callback;
        jh->onRemoteJsep = onPublisherRemoteJsep_callback;
        conn->publisherJoinRoom(conn, handleId);
    }
    else if (conn->mAsListener) {
        //only receiving streaming
        jh->onRemoteJsep = subscriberHandleRemoteJsep_callback;
        conn->subscriberJoinRoom(conn, handleId, conn->mFeedId);
    }
    uart_printf("%s: is ok\n", __func__);
}

static void subscriberCreateHandle_callback(struct FRTCConnection* conn, char* data) {

    JSONStatus_t result = JSONSuccess;
    char* buf = data;
    size_t bufLength = strlen(data);
    char handleIdQuery[] = "data.id";
    size_t hidqLength = strlen(handleIdQuery);
    char* handleIdValue = NULL;
    size_t hidvLength = 0;
    result = JSON_Validate(buf, bufLength);
    if (result != JSONSuccess) {
        uart_printf("subscriberCreateHandle_callback input buffer invalidate\n");
        return;
    }

    unsigned long long handleId = 0;
    result = JSON_Search(buf, bufLength, handleIdQuery, hidqLength, &handleIdValue, &hidvLength);
    if (result == JSONSuccess) {
        char saveHandleId[MIN_SIZE] = "";
        strncpy(saveHandleId, handleIdValue, hidvLength);
        handleId = atoll(saveHandleId);
    }

    RtcJanusHandle* jh = malloc(sizeof(struct RtcJanusHandle));
    jh->feedId = conn->mFeedId;
    jh->handleId = handleId;
    conn->mJanusHandlesMap[1].jhId = handleId;
    conn->mJanusHandlesMap[1].jh = jh;

    jh->onRemoteJsep = subscriberHandleRemoteJsep_callback;
    conn->subscriberJoinRoom(conn, handleId, conn->mFeedId);
    uart_printf("%s: is ok\n", __func__);
}

static void RtcJanusTransaction_onError(struct FRTCConnection* conn, char* data) {
}

static void onEvent(struct FRTCConnection* conn, char* data, size_t len) {

    int i = 0;
    JSONStatus_t result = JSONSuccess;
    char* eventBuf = data;
    size_t ebLength = len;
    char eventQuery[] = "sender";
    size_t eqLength = strlen(eventQuery);
    char* eventValue = NULL;
    size_t evLength = 0;
    result = JSON_Validate(eventBuf, ebLength);
    if (result != JSONSuccess) {
        uart_printf("onEvent input eventBuf invalidate\n");
        return;
    }
    result = JSON_Search(eventBuf, ebLength, eventQuery, eqLength, &eventValue, &evLength);
    if (result != JSONSuccess) {
        uart_printf("onEvent search no eventValue\n");
        //return;
    }

    char hidQuery[] = "plugindata.data.leaving";
    size_t hidLength = strlen(hidQuery);
    char* hidValue = NULL;
    size_t hvLength = 0;
    result = JSON_Search(eventBuf, ebLength, hidQuery, hidLength, &hidValue, &hvLength);
    if (result == JSONSuccess) {
        char saveHid[MIN_SIZE] = "";
        strncpy(saveHid, hidValue, hvLength);
        unsigned long long hid = atoll(saveHid);
        if (hid != 0) {
            for (i = 0; i < RTC_JANUS_PUBLISHER_LISTERN_MAP_LEN; i++) {
                if (conn->mPublisherWebListenerMap[i].feedId == hid) {
                    conn->mPublisherWebListenerMap[i].feedId = 0;
                    conn->mPublisherWebListenerMap[i].count = 0;
                    return;
                }
            }
            conn->mObserver->onLeaving(conn->mObserver->handle, hid);
            return;
        }
    }

    unsigned long long feedId = 0;
    char unPubFeedIdQuery[] = "plugindata.data.unpublished";
    size_t ufqLength = strlen(unPubFeedIdQuery);
    char* unPubFeedIdValue = NULL;
    size_t ufvLength = 0;
    result = JSON_Search(eventBuf, ebLength, unPubFeedIdQuery, ufqLength, &unPubFeedIdValue, &ufvLength);
    if (result == JSONSuccess) {
        char saveFeedId[MIN_SIZE] = "";
        strncpy(saveFeedId, unPubFeedIdValue, ufvLength);
        if (!strncmp("ok", saveFeedId, ufvLength)) {
            conn->mObserver->onUnpublished(conn->mObserver->handle);
            return;
        }
        else {
            feedId = atoll(saveFeedId);
        }
    }
    if (feedId != 0 && feedId == conn->mFeedId) {
        conn->mObserver->onLeaving(conn->mObserver->handle, feedId);
        return;
    }

    // to do many thing here.
    char saveHandleId[MIN_SIZE] = "";
    strncpy(saveHandleId, eventValue, evLength);
    unsigned long long handleId = atoll(saveHandleId);
    RtcJanusHandle* jh = NULL;
    for (i = 0; i < RTC_JANUS_HANDLE_MAP_LEN; i++) {
        if (conn->mJanusHandlesMap[i].jhId == handleId) {
            jh = conn->mJanusHandlesMap[i].jh;
            break;
        }
    }

    char videoRoomQuery[] = "plugindata.data.videoroom";
    size_t urqLength = strlen(videoRoomQuery);
    char* videoRoomValue = NULL;
    size_t urvLength = 0;
    result = JSON_Search(eventBuf, ebLength, videoRoomQuery, urqLength, &videoRoomValue, &urvLength);
    if (jh != NULL && result == JSONSuccess) {
        if (!strncmp("joined", videoRoomValue, urvLength)) {
            if (jh->onJoined) {
                jh->onJoined(conn, jh);
            }
        }
    }

    char jsepQuery[] = "jsep";
    size_t jsqLength = strlen(jsepQuery);
    char* jsepValue = NULL;
    size_t jsvLength = 0;
    result = JSON_Search(eventBuf, ebLength, jsepQuery, jsqLength, &jsepValue, &jsvLength);

    char * feedIdQuery = "plugindata.data.configured";
    size_t fqLength = strlen(feedIdQuery);
    char* feedIdValue = NULL;
    size_t fvLength = 0;
    JSONStatus_t feedResult = JSON_Search(eventBuf, ebLength, feedIdQuery, fqLength, &feedIdValue, &fvLength);

    if (feedResult != JSONSuccess) {
        feedIdQuery = "plugindata.data.id"; // for subscribeRemoteJsep
        fqLength = strlen(feedIdQuery);
        feedResult = JSON_Search(eventBuf, ebLength, feedIdQuery, fqLength, &feedIdValue, &fvLength);
    }

    if (result == JSONSuccess && feedResult == JSONSuccess) {
        if (jh && jh->onRemoteJsep && strncmp("", jsepValue, jsvLength)) {
            char saveFid[MIN_SIZE] = "";
            strncpy(saveFid, feedIdValue, fvLength);
            feedId = atoll(saveFid);
            if (feedId && feedId == conn->mFeedId) {
                conn->mFeedIdIsOnline = true;
            }
            jh->onRemoteJsep(conn, jh, eventBuf);
            free(jh);
            jh = NULL;
            return;
        }
    }

    char nacksQuery[] = "plugindata.data.nacks";
    size_t nkqLength = strlen(nacksQuery);
    char* nacksValue = NULL;
    size_t nkvLength = 0;
    result = JSON_Search(eventBuf, ebLength, nacksQuery, nkqLength, &nacksValue, &nkvLength);
    if (result == JSONSuccess &&
        !strncmp("listener_slow_link", videoRoomValue, urvLength) &&
        strncmp("", nacksValue, nkvLength)) {
        char saveNack[MIN_SIZE] = "";
        strncpy(saveNack, nacksValue, nkvLength);
        int64_t nacks = atoll(saveNack);
        conn->mObserver->onListenerNACKs(conn->mObserver->handle, nacks);
        return;
    }

    char transQuery[] = "transaction";
    size_t tsqLength = strlen(transQuery);
    char* transValue = NULL;
    size_t tsvLength = 0;
    result = JSON_Search(eventBuf, ebLength, transQuery, tsqLength, &transValue, &tsvLength);

    char errorCodeQuery[] = "plugindata.data.error_code";
    size_t ecqLength = strlen(errorCodeQuery);
    char* errorCodeValue = NULL;
    size_t ecvLength = 0;
    JSONStatus_t ecResult = JSON_Search(eventBuf, ebLength, errorCodeQuery, ecqLength, &errorCodeValue, &ecvLength);

    char errorQuery[] = "plugindata.data.error";
    size_t errorQLength = strlen(errorQuery);
    char* errorValue = NULL;
    size_t errorLength = 0;
    JSONStatus_t eResult = JSON_Search(eventBuf, ebLength, errorQuery, errorQLength, &errorValue, &errorLength);

    if (result == JSONSuccess && ecResult == JSONSuccess &&
        eResult == JSONSuccess && strncmp("", transValue, tsvLength)) {
        char saveErrorCode[MIN_SIZE] = "";
        strncpy(saveErrorCode, errorCodeValue, ecvLength);
        unsigned long long error_code = atoll(saveErrorCode);
        if (strncmp("", errorValue, errorLength)) {
            char saveError[MIN_SIZE] = "";
            strncpy(saveError, errorValue, errorLength);
            conn->mObserver->onRTCError(conn->mObserver->handle, error_code, saveError);
            return;
        }
    }

    char publisherIdQuery[] = "plugindata.data.publishers.[0].id";
    size_t pidqLength = strlen(publisherIdQuery);
    char* publisherIdValue = NULL;
    size_t pidvLength = 0;
    result = JSON_Search(eventBuf, ebLength, publisherIdQuery, pidqLength, &publisherIdValue, &pidvLength);

    char displayQuery[] = "plugindata.data.publishers.[0].display";
    size_t disqLength = strlen(displayQuery);
    char* displayValue = NULL;
    size_t disvLength = 0;
    JSONStatus_t disResult = JSON_Search(eventBuf, ebLength, displayQuery, disqLength, &displayValue, &disvLength);
    if (result == JSONSuccess && disResult == JSONSuccess) {
        char saveId[MIN_SIZE] = "";
        strncpy(saveId, publisherIdValue, pidvLength);
        unsigned long long id = atoll(saveId);
        char name[MIN_SIZE] = "noname";
        if (strncmp("", displayValue, disvLength)) {
            strncpy(name, displayValue, disvLength);
            conn->mObserver->onRTCFeedComing(conn->mObserver->handle, id,name);
            if (!strncmp("web_listener", name, disvLength)) {
                conn->mPublisherWebListenerMap[0].feedId = id;
                conn->mPublisherWebListenerMap[0].count = 1;
            }

            if (conn->mAsListener && conn->mFeedId == 0) {
                if (conn->mAutoSubscribe) {
                    conn->subscriberCreateHandle(conn, id, name);
                }
            }
            else if (conn->mAsListener && conn->mFeedId != 0 && conn->mAsPublisher) {
                if (id == conn->mFeedId) {
                    conn->subscriberCreateHandle(conn, id, name);
                }
            }
        }
    }
}

static void onSessionEvent(struct FRTCConnection* conn, char* data, size_t len) {

    JSONStatus_t result = JSONSuccess;
    char* sessionEventBuf = data;
    size_t seLength = len;
    char recvDataQuery[] = "recvdata.data";
    size_t rdqLength = strlen(recvDataQuery);
    char* recvDataValue = NULL;
    size_t rdvLength = 0;
    result = JSON_Validate(sessionEventBuf, seLength);
    if (result != JSONSuccess) {
        uart_printf("onSessionEvent sessionEventBuf invalidate\n");
        return;
    }
    result = JSON_Search(sessionEventBuf, seLength, recvDataQuery, rdqLength, &recvDataValue, &rdvLength);

    char internalQuery[] = "recvdata.internal";
    size_t inqLength = strlen(internalQuery);
    char* internalValue = NULL;
    size_t invLength = 0;
    JSONStatus_t intResult = JSON_Search(sessionEventBuf, seLength, internalQuery, inqLength, 
       &internalValue, &invLength);

    char fromQuery[] = "recvdata.from";
    size_t frqLength = strlen(fromQuery);
    char* fromValue = NULL;
    size_t frmvLength = 0;
    JSONStatus_t frmResult = JSON_Search(sessionEventBuf, seLength, fromQuery, frqLength, &fromValue, &frmvLength);

    if (result == JSONSuccess) {
        char data[MIN_SIZE] = "";
        strncpy(data, recvDataValue, rdvLength);
        char saveFromValue[MIN_SIZE] = "";
        strncpy(saveFromValue, fromValue, frmvLength);
        if (intResult == JSONSuccess && frmResult == JSONSuccess && !strncmp("true", internalValue, invLength)) {
            conn->mObserver->onRTCUserAttribute(conn->mObserver->handle, atoll(saveFromValue), data);
        }
        else if (intResult == JSONSuccess && frmResult == JSONSuccess && !strncmp("false", internalValue, invLength)) {
            conn->mObserver->onRTCUserMessage(conn->mObserver->handle, atoll(saveFromValue), data);
        }
    }

    char joinQuery[] = "userevent.joined";
    size_t jnqLength = strlen(joinQuery);
    char* joinValue = NULL;
    size_t jnvLength = 0;
    result = JSON_Search(sessionEventBuf, seLength, "userevent", 9, &joinValue, &jnvLength);
    JSONStatus_t joinResult = JSON_Search(sessionEventBuf, seLength, joinQuery, jnqLength, &joinValue, &jnvLength);

    char uedisQuery[] = "userevent.display";
    size_t uedisqLength = strlen(uedisQuery);
    char* uedisValue = NULL;
    size_t uedisvLength = 0;
    JSONStatus_t uedisResult = JSON_Search(sessionEventBuf, seLength, uedisQuery, 
        uedisqLength, &uedisValue, &uedisvLength);

    char leaveQuery[] = "userevent.leaving";
    size_t leqLength = strlen(leaveQuery);
    char* leaveValue = NULL;
    size_t levLength = 0;
    JSONStatus_t leResult = JSON_Search(sessionEventBuf, seLength, leaveQuery, leqLength, &leaveValue, &levLength);

    char userIdQuery[] = "userevent.users.[0].id";
    size_t uidqLength = strlen(userIdQuery);
    char* userIdValue = NULL;
    size_t uidvLength = 0;
    JSONStatus_t uidResult = JSON_Search(sessionEventBuf, seLength, userIdQuery, uidqLength, &userIdValue, &uidvLength);

    char udisQuery[] = "userevent.users.[0].display";
    size_t udisqLength = strlen(udisQuery);
    char* udisValue = NULL;
    size_t udisvLength = 0;
    JSONStatus_t udisResult = JSON_Search(sessionEventBuf, seLength, udisQuery, udisqLength, &udisValue, &udisvLength);

    char attrQuery[] = "userevent.users.[0].attribute";
    size_t attrqLength = strlen(attrQuery);
    char* attrValue = NULL;
    size_t attrvLength = 0;
    JSONStatus_t attrResult = JSON_Search(sessionEventBuf, seLength, attrQuery, attrqLength, &attrValue, &attrvLength);

    if (result == JSONSuccess) {
        if (joinResult == JSONSuccess && strncmp("", joinValue, jnvLength)) {
            char saveJoinValue[MIN_SIZE] = "";
            strncpy(saveJoinValue, joinValue, jnvLength);
            char saveUedisValue[MIN_SIZE] = "";
            strncpy(saveUedisValue, uedisValue, uedisvLength);
            conn->mObserver->onRTCUserJoinedRoom(conn->mObserver->handle, atoll(saveJoinValue), saveUedisValue);
        }
        else if (leResult == JSONSuccess && strncmp("", leaveValue, levLength)) {
            char saveLeaveValue[MIN_SIZE] = "";
            strncpy(saveLeaveValue, leaveValue, levLength);
            conn->mObserver->onRTCUserLeavingRoom(conn->mObserver->handle, atoll(saveLeaveValue));
        }
        else if (uidResult == JSONSuccess && strncmp("", userIdValue, uidvLength)) {
            char userId[MIN_SIZE] = "";
            strncpy(userId, userIdValue, uidvLength);
            unsigned long long id = atoll(userId);
            char name[MIN_SIZE] = "noname";
            if (udisResult == JSONSuccess && strncmp("", udisValue, udisvLength)) {
                strncpy(name, udisValue, udisvLength);
            }
            conn->mObserver->onRTCUserJoinedRoom(conn->mObserver->handle, id,name);
            if (attrResult == JSONSuccess && strncmp("", attrValue, attrvLength)) {
                char saveAttrValue[MIN_SIZE] = "";
                strncpy(saveAttrValue, attrValue, attrvLength);
                conn->mObserver->onRTCUserAttribute(conn->mObserver->handle, id, attrValue);
            }
        }
    }

}

static void onRTCClosed(struct FRTCConnection* conn) {
    conn->mbQuitWatchDog = true;
    conn->mObserver->onRTCConnectError(conn->mObserver->handle);
    conn->mObserver->onCirrusDisconnected(conn->mObserver->handle);
}

static void onRTCFailure(struct FRTCConnection* conn) {
    conn->mbQuitWatchDog = true;
    conn->mObserver->onRTCLoginError(conn->mObserver->handle);
}

static void onRTCOpen(struct FRTCConnection* conn) {
    conn->mRTCConnected = true;
    conn->mObserver->onRTCLoginOK(conn->mObserver->handle);
    conn->createSession(conn);
}

static void onRTCMessage(struct FRTCConnection* conn, const char* data, size_t len) {
    if (!conn->mRTCConnected) {
        return;
    }
    // ESP_LOGI("onRTCMessage", "msg: %.*s.\n", len, data);
    int i = 0, j = 0;
    JSONStatus_t result = JSONSuccess;
    char* buf = data;
    size_t bufLength = len;
    char janusQuery[] = "janus";
    size_t jqLength = strlen(janusQuery);
    char* janusValue = NULL;
    size_t jvLength = 0;
    result = JSON_Validate(buf, bufLength);
    if (result != JSONSuccess) {
        uart_printf("onRTCMessage input buffer invalidate %.*s\n", len, data);
        return;
    }
    result = JSON_Search(buf, bufLength, janusQuery, jqLength, &janusValue, &jvLength);
    if (result != JSONSuccess) {
        uart_printf("onRTCMessage search no janusValue\n");
        return;
    }

    char transQuery[] = "transaction";
    size_t tqLength = strlen(transQuery);
    char* transValue = NULL;
    size_t tvLength = 0;
    result = JSON_Search(buf, bufLength, transQuery, tqLength, &transValue, &tvLength);

    if (!strncmp("success", janusValue, jvLength)) {
        for (i = 0; i < RTC_JANUS_TRANSACTION_MAP_LEN; i++) {
            if (!strncmp(conn->mJanusTransactionMap[i].jtId, transValue, tvLength)) {
                RtcJanusTransaction* jt = conn->mJanusTransactionMap[i].jt;
                jt->onSuccess(conn, data);
                strcpy(conn->mJanusTransactionMap[i].jtId, "");
                free(jt);
                jt = NULL;
                break;
            }
        }
    }else if (!strncmp("error", janusValue, jvLength)) {
        for (j = 0; j < RTC_JANUS_TRANSACTION_MAP_LEN; j++) {
            if (!strncmp(conn->mJanusTransactionMap[j].jtId, transValue, tvLength)) {
                RtcJanusTransaction* jt = conn->mJanusTransactionMap[j].jt;
                if (jt) {
                    jt->onError(conn, data);
                    strcpy(conn->mJanusTransactionMap[j].jtId, "");
                    free(jt);
                    jt = NULL;
                    break;
                }
            }
        }
    }else if (!strncmp("event", janusValue, jvLength)) {
        conn->onEvent(conn, data, len);
        uart_printf("%s: onEvent\n", __func__);
    }else if (!strncmp("ack", janusValue, jvLength)) {
        conn->mbAliving = true;
    }else if (!strncmp("sessionevent", janusValue, jvLength)) {
        conn->onSessionEvent(conn, data, len);
        uart_printf("%s: onSessionEvent\n", __func__);
    }else if (!strncmp("webrtcup", janusValue, jvLength)) {
        unsigned long long handleId = 0;
        result = JSON_Search(buf, bufLength, "sender", 6, &janusValue, &jvLength);
        if (result == JSONSuccess) {
            char saveHandleId[MIN_SIZE] = "";
            strncpy(saveHandleId, janusValue, jvLength);
            handleId = atoll(saveHandleId);
        }
        conn->mObserver->onRTCWebrtcUp(conn->mObserver->handle, handleId);
    }else if (!strncmp("media", janusValue, jvLength)) {
        bool sending = false;
        char receivingQuery[] = "receiving";
        size_t recqLength = strlen(receivingQuery);
        char* receivingValue = NULL;
        size_t recvLength = 0;
        result = JSON_Search(buf, bufLength, receivingQuery, recqLength, &receivingValue, &recvLength);
        if (result == JSONSuccess) {
            if (!strncmp("true", janusValue, jvLength)) {
                sending = true;
            }
        }
        int mediaType = 0;
        char mediaTypeQuery[] = "type";
        size_t mediaqLength = strlen(mediaTypeQuery);
        char* mediaTypeValue = NULL;
        size_t mediavLength = 0;
        result = JSON_Search(buf, bufLength, mediaTypeQuery, mediaqLength, &mediaTypeValue, &mediavLength);
        if (result == JSONSuccess) {
            if (!strncmp("video", mediaTypeValue, mediavLength)) {
                mediaType = 1;
            }else {
                mediaType = 0;
            }
        }
        conn->mObserver->onRTCMediaStreamingEvent(conn->mObserver->handle, 0, mediaType, sending);
    }else if (!strncmp("hangup", janusValue, jvLength)) {
        unsigned long long handleId = 0;
        result = JSON_Search(buf, bufLength, "sender", 6, &janusValue, &jvLength);
        if (result == JSONSuccess) {
            char saveHandleId[20] = "";
            strncpy(saveHandleId, janusValue, jvLength);
            handleId = atoll(saveHandleId);
        }
        conn->mObserver->onRTCHangUp(conn->mObserver->handle, handleId);
    }else if (!strncmp("slowlink", janusValue, jvLength)) {
        char nacksQuery[] = "nacks";
        size_t nkqLength = strlen(nacksQuery);
        char* nacksValue = NULL;
        size_t nkvLength = 0;
        char saveNacksValue[20] = "";
        result = JSON_Search(buf, bufLength, nacksQuery, nkqLength, &nacksValue, &nkvLength);
        if (result == JSONSuccess) {
            strncpy(saveNacksValue, nacksValue, nkvLength);
            int64_t nacks = atoll(saveNacksValue);
            conn->mObserver->onNACKs(conn->mObserver->handle, nacks);
        }
    }
}

char g_rtc_create_session[] = "{\"janus\":\"create\",\"transaction\":\"%s\",\"sdktag\":\"%s\",\
\"janusIp\":\"%s\",\"uri\":\"%s\",\"userevent\":true,\"sessionevent\":true}";
static void createSession(struct FRTCConnection* conn) {

    RtcJanusTransaction* jt = malloc(sizeof(struct RtcJanusTransaction));
    generateString(jt->transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    char createSession[DEFAULT_SIZE];
    sprintf(createSession, g_rtc_create_session, jt->transactionId, conn->mSDKTag,
        conn->mMediaServerIP, conn->mOriginURI);

    jt->onSuccess = createSession_callback;
    jt->onError = RtcJanusTransaction_onError;

    strcpy(conn->mJanusTransactionMap[0].jtId, jt->transactionId);
    conn->mJanusTransactionMap[0].jt = jt;

    conn->sendMessage(conn, createSession);
    uart_printf("%s: %s\n", __func__, createSession);
}

char g_rtc_create_room[] = "{\"janus\":\"message\",\"body\":{\"request\":\"create\",\"id\":%s,\"app_id\":\"%s\",\
\"%s\":\"%s\",\"description\":\"%s\",\"publishers\":10000,\"is_private\":false,\
\"videocodec\":\"%s\",\"audiocodec\":\"%s\"},\
\"transaction\":\"%s\",\
\"session_id\": %lld,\"handle_id\":%lld}";
static void createRoom(struct FRTCConnection* conn, unsigned long long handleId) {

    RtcJanusTransaction* jt = malloc(sizeof(struct RtcJanusTransaction));
    generateString(jt->transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    char createRoom[DEFAULT_SIZE] = "";
    char description[MIN_SIZE] = "";
    strcat(description, "RTOS");
    if (conn->mMediaServerStandalone_needRoomID) {
        char roomId[] = "room";
        sprintf(createRoom, g_rtc_create_room, conn->mUserId, conn->mAppId, roomId, conn->mRoomId,
            description, conn->mVideoCodec, conn->mAudioCodec, jt->transactionId, conn->mSessionId, handleId);
    }
    else {
        char roomName[] = "room_name";
        sprintf(createRoom, g_rtc_create_room, conn->mUserId, conn->mAppId, roomName, conn->mRoomName,
            description, conn->mVideoCodec, conn->mAudioCodec, jt->transactionId, conn->mSessionId, handleId);
    }

    jt->onSuccess = createRoom_callback;
    jt->onError = RtcJanusTransaction_onError;

    strcpy(conn->mJanusTransactionMap[2].jtId, jt->transactionId);
    conn->mJanusTransactionMap[2].jt = jt;
    conn->sendMessage(conn, createRoom);
    uart_printf("%s: %s\n", __func__, createRoom);
}

char g_rtc_connect_url[] = "%s?appid=%s&roomname=%s&uid=%s&token=%s&compulsive=true&connectingCount=%lld";
static void connecting(struct FRTCConnection* conn, const char* url) {

    conn->mOriginURI = url;
    sprintf(conn->mFullURL, g_rtc_connect_url, url, conn->mAppId, conn->mRoomName,
        conn->mUserId, conn->mTokenStr, count);
    conn->mUd.ctx = (void*)conn;
    conn->mUd.fp = RTCConnection_baidurtc_client;
    conn->mWSCtx = brtc_websocket_init_context(conn->mCerPath);
    conn->mWSConnetion = brtc_websocket_connect(conn->mWSCtx, conn->mFullURL, &conn->mUd);
    if (!conn->mWSConnetion) {
        conn->mObserver->onRTCConnectError(conn->mObserver->handle);
    }
    count++;
    uart_printf("conn:%p, conn->mFullURL:%s\n", conn, conn->mFullURL);
}

char g_rtc_keep_alive[] = "{\"janus\":\"keepalive\",\"session_id\": %lld,\"transaction\":\"%s\"}";
static void keepAlive(struct FRTCConnection* conn) {
    RtcJanusTransaction jt;
    if (conn->mSessionId == 0) {
        return;
    }
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    char keepAlive[DEFAULT_SIZE];
    sprintf(keepAlive, g_rtc_keep_alive, conn->mSessionId, jt.transactionId);
    conn->sendMessage(conn, keepAlive);
    uart_printf("keepalive sucess %s\n", keepAlive);
}

char g_rtc_attach_cmd[] = "{\"janus\":\"attach\",\"plugin\":\"janus.plugin.videoroom\",\
\"transaction\":\"%s\",\"session_id\": %lld}";
static void publisherCreateHandle(struct FRTCConnection* conn) {
    RtcJanusTransaction* jt = malloc(sizeof(struct RtcJanusTransaction));
    generateString(jt->transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    char attach[DEFAULT_SIZE] = "";
    sprintf(attach, g_rtc_attach_cmd, jt->transactionId, conn->mSessionId);
    jt->onSuccess = publisherCreateHandle_callback;
    jt->onError = RtcJanusTransaction_onError;
    strcpy(conn->mJanusTransactionMap[1].jtId, jt->transactionId);
    conn->mJanusTransactionMap[1].jt = jt;

    conn->sendMessage(conn, attach);
    uart_printf("%s: %s\n", __func__, attach);
}

char g_rtc_join_room[] = "{\"janus\":\"message\",\"body\":{\"request\":\"join\",\"room\":%lld,\"ptype\":\"publisher\",\
\"display\":\"%s\",\"id\":%lld,\"app_id\":\"%s\",\"room_name\":\"%s\",\
\"role\":\"publisher\",\"token\":\"%s\"},\
\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void publisherJoinRoom(struct FRTCConnection* conn, unsigned long long handleId) {
    char joinRoom[DEFAULT_SIZE] = "";
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(joinRoom, g_rtc_join_room, conn->mRoomId, conn->mDisplayName, atoll(conn->mUserId), conn->mAppId,
        conn->mRoomName, conn->mTokenStr, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, joinRoom);
    uart_printf("%s: %s\n", __func__, joinRoom);
}
char offerSdp[DEFAULT_SIZE * 8] = "";
char g_rtc_offer_sdp[] = "{\"janus\":\"message\",\"body\":{\"request\":\"configure\",\
\"audio\":%s,\"video\":%s,\"data\":%s},\"jsep\":{\"type\":\"OFFER\",\"sdp\":\"%s\"},\
\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void publisherCreateOffer(struct FRTCConnection* conn, unsigned long long handleId, void* Desc) {

    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(offerSdp,g_rtc_offer_sdp, conn->mHasAudio ? "true" : "false", conn->mHasVideo ? "true" : "false",
        conn->mHasData ? "true" : "false", (char*)Desc, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, offerSdp);
    // uart_printf("%s: %s\n", __func__, offerSdp);
}

char g_rtc_trickle_cmd[] = "{\"janus\":\"trickle\",\"candidate\":{\"candidate\":\"candidate:\"%s\",\
\"sdpMid\":\"0\",\"sdpMLineIndex\":0},\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void trickleCandicate(struct FRTCConnection* conn, unsigned long long handleId, void* Candidate) {
    if (!strcmp(conn->mBRTCServerVersion, "brtc_v0")) {
        return;// It has no need in BRTC after 2020.4.14
    }
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    char trickle[DEFAULT_SIZE];
    sprintf(trickle, g_rtc_trickle_cmd, (char*)Candidate, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, trickle);
    uart_printf("%s: %s\n", __func__, trickle);
}

char g_rtc_trickle_complete_cmd[] = "{\"janus\":\"trickle\",\"candidate\":{\"completed\":true},\
\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void trickleCandicateComplete(struct FRTCConnection* conn, unsigned long long handleId) {
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    char trickleComplete[DEFAULT_SIZE];
    sprintf(trickleComplete, g_rtc_trickle_complete_cmd, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, trickleComplete);
    uart_printf("%s: %s\n", __func__, trickleComplete);
}

static void subscriberCreateHandle(struct FRTCConnection* conn, unsigned long long feedId, const char * display) {

    RtcJanusTransaction* jt = malloc(sizeof(struct RtcJanusTransaction));
    generateString(jt->transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    char subCreateHandle[DEFAULT_SIZE] = "";
    sprintf(subCreateHandle, g_rtc_attach_cmd, jt->transactionId, conn->mSessionId);
    conn->mFeedId = feedId;
    jt->onSuccess = subscriberCreateHandle_callback;
    jt->onError = RtcJanusTransaction_onError;
    strcpy(conn->mJanusTransactionMap[3].jtId, jt->transactionId);
    conn->mJanusTransactionMap[3].jt = jt;
    conn->sendMessage(conn, subCreateHandle);
    uart_printf("%s: %s\n", __func__, subCreateHandle);
}

char g_rtc_subscriber_join_room[] = "{\"janus\":\"message\",\"body\":{\"request\":\"join\",\
\"room\":%lld,\"ptype\":\"listener\",\"id\":%lld,\"app_id\":\"%s\",\
\"room_name\":\"%s\",\"role\":\"%s\",\"token\":\"%s\",\"feed\":%lld},\
\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void subscriberJoinRoom(struct FRTCConnection* conn, unsigned long long handleId, unsigned long long feedId) {
    char subscriberJoinRoom[DEFAULT_SIZE] = "";
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(subscriberJoinRoom, g_rtc_subscriber_join_room, conn->mRoomId, atoll(conn->mUserId), conn->mAppId,
        conn->mRoomName, conn->mAsPublisher ? "publisher" : "listener", conn->mTokenStr,
        feedId, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, subscriberJoinRoom);
    uart_printf("%s: %s\n", __func__, subscriberJoinRoom);

}

char g_rtc_answer_sdp[] = "{\"janus\":\"message\",\"body\":{\"request\":\"start\",\"room\":%lld},\
\"jsep\":{\"type\":\"ANSWER\",\"sdp\":\"%s\"},\
\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void subscriberCreateAnswer(struct FRTCConnection* conn, unsigned long long handleId, void* Desc) {
    char answerSdp[DEFAULT_SIZE * 4] = "";
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(answerSdp, g_rtc_answer_sdp, conn->mRoomId, (char*)Desc, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, answerSdp);
    uart_printf("%s: %s\n", __func__, answerSdp);
}

char g_rtc_config_cmd[] = "{\"janus\":\"message\",\"body\":{\"request\":\"configure\", \"audio\":%s,\
\"video\":%s,\"data\":%s},\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void subscriberConfig(struct FRTCConnection* conn, unsigned long long handleId,
                                    bool audio, bool video, bool data) {
    char configCmd[DEFAULT_SIZE] = "";
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(configCmd, g_rtc_config_cmd, conn->mHasAudio ? "true" : "false", conn->mHasVideo ? "true" : "false",
        conn->mHasData ? "true" : "false", jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, configCmd);
    uart_printf("%s: %s\n", __func__, configCmd);
}

static void setUserId(struct FRTCConnection* conn, const char* userId) {
    conn->mUserId = userId;
}

static void setRoomName(struct FRTCConnection* conn, const char* roomName) {
    conn->mRoomName = roomName;
}

static void setAppId(struct FRTCConnection* conn, const char* appId) {
    conn->mAppId = appId;
}

static void setTokenStr(struct FRTCConnection* conn, const char* token) {
    conn->mTokenStr = token;
}

static void setDisplayName(struct FRTCConnection* conn, const char* displayRame) {
    conn->mDisplayName = displayRame;
}

static void setCerPath(struct FRTCConnection* conn, const char* cerPath) {
    conn->mCerPath = cerPath;
}

static void setAsPublisher(struct FRTCConnection* conn, bool asPublisher) {
    conn->mAsPublisher = asPublisher;
}

static void setAsListener(struct FRTCConnection* conn, bool asListener) {
    conn->mAsListener = asListener;
}

static void setFeedId(struct FRTCConnection* conn, const char* feedIdStr) {
    conn->mFeedId = atoll(feedIdStr);
}

static void setHasAudio(struct FRTCConnection* conn, bool hasAudio) {
    conn->mHasAudio = hasAudio;
}

static void setHasVideo(struct FRTCConnection* conn, bool hasVideo) {
    conn->mHasVideo = hasVideo;
}

static void setHasData(struct FRTCConnection* conn, bool hasData) {
    conn->mHasData = hasData;
}

static void setMediaServerIP(struct FRTCConnection* conn, const char* mediaserverIP) {
    conn->mMediaServerIP = mediaserverIP;
}

static void setSDKTag(struct FRTCConnection* conn, const char* sdkTag) {
    conn->mSDKTag = sdkTag;
}

static void setAutoSubscribe(struct FRTCConnection* conn, bool autoSubscribe) {
    conn->mAutoSubscribe = autoSubscribe;
}

static void setVideoCodec(struct FRTCConnection* conn, const char* videoCodec) {
    conn->mVideoCodec = videoCodec;
}

static void setObserver(struct FRTCConnection* conn, struct IRTCConnectionObserver* observer) {
    conn->mObserver = observer;
}

static void sendMessage(struct FRTCConnection* conn, char* buf) {
    if (!conn->mRTCConnected) {
        return;
    }
    brtc_websocket_send_data(conn->mWSConnetion, buf, strlen(buf));
    printf("tx: %s\n", buf);
}

char g_rtc_sendmsg_cmd[] = "{\"janus\":\"message\",\"transaction\":\"%s\",\"body\":{\"request\":\"senddata\",\
\"room\":%lld,\"id\":%lld,\"to\":%lld,\"data\":\"%s\",\"internal\":\"%s\"},\"session_id\":%lld}";
static void sendMessageToUser(struct FRTCConnection* conn, const char* msg, unsigned long long id, bool internal) {
    char sendMsgCmd[DEFAULT_SIZE] = "";
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(sendMsgCmd, g_rtc_sendmsg_cmd, jt.transactionId, conn->mRoomId, atoll(conn->mUserId), id,
        msg, internal ? "true" : "false", conn->mSessionId);
    conn->sendMessage(conn, sendMsgCmd);
}

char g_rtc_unpublish_cmd[] = "{\"janus\":\"message\",\"body\":{\"request\":\"unpublish\"},\
\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void unpublish(struct FRTCConnection* conn, unsigned long long handleId) {
    char unpublishCmd[DEFAULT_SIZE] = "";
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(unpublishCmd, g_rtc_unpublish_cmd, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, unpublishCmd);
}

char g_rtc_detach_cmd[] = "{\"janus\":\"detach\",\"transaction\":\"%s\",\"session_id\":%lld,\"handle_id\":%lld}";
static void detach(struct FRTCConnection* conn, unsigned long long handleId) {
    char detachCmd[DEFAULT_SIZE] = "";
    RtcJanusTransaction jt;
    generateString(jt.transactionId, RTC_JANUS_TRANSACTION_ID_LEN);
    sprintf(detachCmd, g_rtc_detach_cmd, jt.transactionId, conn->mSessionId, handleId);
    conn->sendMessage(conn, detachCmd);
}

static void disconnect(struct FRTCConnection* conn) {
    if (conn->mWSCtx != NULL) {
        brtc_websocket_uninit_context(conn->mWSCtx);
        conn->mWSCtx = NULL;
    }
    if (conn->mWSConnetion) {
        conn->mWSConnetion = NULL;
    }
    if (conn->mRTCConnected) {
        conn->mRTCConnected = false;
    }
    uart_printf("%s: success\n", __func__);
}

void FRTCConnection_construct(struct FRTCConnection* fRTCConn) {

    fRTCConn->onEvent = onEvent;
    fRTCConn->onRTCClosed = onRTCClosed;
    fRTCConn->onRTCFailure = onRTCFailure;
    fRTCConn->onRTCMessage = onRTCMessage;
    fRTCConn->onSessionEvent = onSessionEvent;
    fRTCConn->onRTCOpen = onRTCOpen;
    fRTCConn->connecting = connecting;
    fRTCConn->createSession = createSession;
    fRTCConn->publisherCreateHandle = publisherCreateHandle;
    fRTCConn->createRoom = createRoom;
    fRTCConn->publisherJoinRoom = publisherJoinRoom;
    fRTCConn->publisherCreateOffer = publisherCreateOffer;
    fRTCConn->keepAlive = keepAlive;
    fRTCConn->sendMessage = sendMessage;
    fRTCConn->sendMessageToUser = sendMessageToUser;

    fRTCConn->setAppId = setAppId;
    fRTCConn->setRoomName = setRoomName;
    fRTCConn->setUserId = setUserId;
    fRTCConn->setSDKTag = setSDKTag;
    fRTCConn->setTokenStr = setTokenStr;
    fRTCConn->setAsListener = setAsListener;
    fRTCConn->setAsPublisher = setAsPublisher;
    fRTCConn->setAutoSubscribe = setAutoSubscribe;
    fRTCConn->setCerPath = setCerPath;
    fRTCConn->setDisplayName = setDisplayName;
    fRTCConn->setFeedId = setFeedId;
    fRTCConn->setHasAudio = setHasAudio;
    fRTCConn->setHasVideo = setHasVideo;
    fRTCConn->setHasData = setHasData;
    fRTCConn->setMediaServerIP = setMediaServerIP;
    fRTCConn->setVideoCodec = setVideoCodec;
    fRTCConn->setObserver = setObserver;

    fRTCConn->subscriberConfig = subscriberConfig;
    fRTCConn->subscriberCreateAnswer = subscriberCreateAnswer;
    fRTCConn->subscriberCreateHandle = subscriberCreateHandle;
    fRTCConn->subscriberJoinRoom = subscriberJoinRoom;
    fRTCConn->trickleCandicate = trickleCandicate;
    fRTCConn->trickleCandicateComplete = trickleCandicateComplete;
    fRTCConn->unpublish = unpublish;
    fRTCConn->detach = detach;
    fRTCConn->disconnect = disconnect;

    fRTCConn->mUserId = "10001";
    fRTCConn->mRoomName = "2131";
    fRTCConn->mAppId = "";
    fRTCConn->mTokenStr = "";
    fRTCConn->mDisplayName = "BRTC RTOS demo";
    fRTCConn->mCerPath = "./a.cer";

    fRTCConn->mSessionId = 0;
    fRTCConn->mRoomId = 999;
    fRTCConn->mFeedId = 0;

    fRTCConn->mAsPublisher = true;
    fRTCConn->mAsListener = true;
    fRTCConn->mMediaServerStandalone_needRoomID = false;
    fRTCConn->mBRTCServerVersion = "brtc_v0";// brtc_v0 default as old janus, brtc_v1 for signalserver ARCH, brtc_v2 for multistream.
    fRTCConn->mFeedIdIsOnline = false;
    fRTCConn->mHasAudio = true;
    fRTCConn->mHasVideo = true;
    fRTCConn->mHasData = false;
    fRTCConn->mAutoSubscribe = true;
    fRTCConn->mVideoCodec = "h263";// "vp8", "vp9", "h263", "av1", "h265"
    fRTCConn->mAudioCodec = "pcmu";// "opus", "pcma", "g722", "isac16", "isac32"

    fRTCConn->mMediaServerIP = "";
    fRTCConn->mSDKTag = "";
    fRTCConn->mLiveStreamingURL = "";

    fRTCConn->mWSConnetion = NULL;
    fRTCConn->mWSCtx = NULL;
    fRTCConn->mRTCConnected = false;
    fRTCConn->mbAliving = false;
    fRTCConn->mbQuitWatchDog = false;
    fRTCConn->mFullURL[DEFAULT_SIZE - 1] = '\0';
    fRTCConn->mOriginURI = "";
}


