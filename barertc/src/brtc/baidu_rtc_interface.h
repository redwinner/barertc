#ifndef BRTC_RTC_INT_H_
#define BRTC_RTC_INT_H_

#include "baidu_rtc_common_define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*LogCallback)(char *log, unsigned int len);
typedef void(*VideoArrivalCallback)(unsigned char *img, int width, int height, void* ctx);
typedef void(*IRtcMessageListener)(RtcMessage* msg);

void * brtc_create_client();
void brtc_destroy_client(void* rtc_client);
bool brtc_init_client(void* rtc_client);
void brtc_deinit_client(void* rtc_client);
bool brtc_login_room(void* rtc_client, const char* room_name, const char* user_id, const char* display_name, const char* token);
bool brtc_logout_room(void* rtc_client);
void brtc_set_server_url(void* rtc_client, const char* url);
void brtc_set_appid(void* rtc_client, const char* app_id);
void brtc_set_cer(void* rtc_client, const char* cer_path);
void brtc_register_message_listener(void* rtc_client, IRtcMessageListener msgListener);
void brtc_send_message_to_user(void* rtc_client, const char* msg, const char* id);
void brtc_start_publish(void* rtc_client);
void brtc_set_auto_publish(void* rtc_client, int auto_publish);
void brtc_set_auto_subscribe(void* rtc_client, int auto_subscribe);

void brtc_keepalive(void* rtc_client);
void brtc_timer_poll(void* rtc_client);
void brtc_keepalive_check_result(void* rtc_client);

#ifdef __cplusplus
}
#endif

#endif

