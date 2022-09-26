#include "baidu_rtc_common_define.h"
#include "baidu_rtc_interface.h"
#include "baidu_rtc_signal_client.h"

#ifdef __cplusplus
extern "C" {
#endif

void * brtc_create_client() {
    return (void*)createClient();
}

void brtc_destroy_client(void* rtc_client) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->destoryClient(client);
    }
}

bool  brtc_init_client(void* rtc_client) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        bool res = client->init(client);
        return res;
    } else {
        return false;
    }
}

void brtc_deinit_client(void* rtc_client) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->deInit(client);
    }
}

bool brtc_login_room(void* rtc_client, const char* room_name, 
                        const char* user_id, const char* display_name, const char* token) {
    BaiduRtcClient * client = (BaiduRtcClient *)rtc_client;
    if (client) {
        return client->loginRoom(client, room_name, user_id, display_name, token);
    } else {
        return false;
    }
}

bool brtc_logout_room(void* rtc_client) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        return client->logoutRoom(client);
    } else {
        return false;
    }
}

void brtc_set_server_url(void* rtc_client, const char* url) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->setMediaServerURL(client, url);
    }
}

void brtc_set_appid(void* rtc_client, const char* app_id) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->setAppID(client, app_id);
    }
}

void brtc_set_cer(void* rtc_client, const char* cer_path) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->setCER(client, cer_path);
    }
}

void brtc_register_message_listener(void* rtc_client, IRtcMessageListener msgListener) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->registerRtcMessageListener(client, msgListener);
    }
}

void brtc_send_message_to_user(void* rtc_client, const char* msg, const char* id) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->sendMessageToUser(client, msg, id);
    }
}

void brtc_start_publish(void* rtc_client) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->startPublish(client);
    }
}

void brtc_set_auto_publish(void* rtc_client, int auto_publish) {
    BaiduRtcClient* client = (BaiduRtcClient*)rtc_client;
    if (client) {
        client->mAutoPublish = auto_publish;
    }
}

#ifdef __cplusplus
}
#endif
