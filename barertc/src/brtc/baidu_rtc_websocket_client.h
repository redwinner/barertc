#ifndef BRTC_RTC_WS_H_
#define BRTC_RTC_WS_H_
#include "mongoose.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*p_callback_baidurtc_client)(void *c, int ev, void *ev_data, void *fn_data);

typedef struct baidurtc_user_data
{
    void *ctx;
	p_callback_baidurtc_client fp;
} baidurtc_user_data;

typedef struct baidurtc_context
{
    void * ctx;
    volatile int exit_flag;
    const char *ca_path;
    struct mg_mgr mgr;        // Event manager
} baidurtc_context;

void* brtc_websocket_init_context(const char *ca_path);
void* brtc_websocket_connect(void *ctx, char *url, void *user_data);
void brtc_websocket_timer_poll(void *ctx, int time);
int brtc_websocket_send_data(void *c, const char* data_buf, int len);
int brtc_websocket_uninit_context(void *ctx);

#ifdef __cplusplus
}
#endif

#endif