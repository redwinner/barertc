#include "baidu_rtc_websocket_client.h"
#include "mongoose.h"

typedef struct baidurtc_context
{
    void * ctx;
    volatile int exit_flag;
    const char *ca_path;
    struct mg_mgr mgr;        // Event manager
} baidurtc_context;

void* brtc_websocket_init_context(const char *ca_path)
{
    baidurtc_context *pCtx = (baidurtc_context *)malloc(sizeof(baidurtc_context));
    pCtx->ctx = NULL;
    pCtx->exit_flag = 0;
    // mg_log_set("4");
    mg_mgr_init(&pCtx->mgr);        // Initialise event manager
    return pCtx;
}

void* brtc_websocket_connect(void *ctx, char *url, void *user_data)
{
    baidurtc_context *pCtx = (baidurtc_context *)ctx;
    baidurtc_user_data *pUd = (baidurtc_user_data *)user_data;
    struct mg_connection *c = mg_ws_connect(&pCtx->mgr, url, pUd->fp, pUd->ctx, NULL);   // Create client
    return c;
}

void brtc_websocket_timer_poll(void *ctx, int time)
{
    baidurtc_context *pCtx = (baidurtc_context *)ctx;
    mg_mgr_poll(&pCtx->mgr, time);
}

int brtc_websocket_send_data(void *c, const char* data_buf, int len)
{
    mg_ws_send((struct mg_connection *)c, data_buf, len, WEBSOCKET_OP_TEXT);
    return 0;
}

int brtc_websocket_uninit_context(void *ctx)
{
    if (ctx == NULL) {
        return -1;
    }
    baidurtc_context *pCtx = (baidurtc_context *)ctx;
    pCtx->exit_flag = 1;
    mg_mgr_free(&pCtx->mgr);
    free(pCtx);
    pCtx = NULL;
    return 0;
}
