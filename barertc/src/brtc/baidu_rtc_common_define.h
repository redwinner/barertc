#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define bool _Bool

// typedef unsigned char        bool;

#define BAIDU_RTC_MAX_COMMON_ARRAY_LEN 512
#define BAIDU_RTC_OPTION_ONLY_PULL_VIDEO_STREAMING 0

struct video_capturer_ctx;
//video capturer
typedef void(*video_frame_push)(struct videdo_frame *frame, long long int timestamp, void *arg);
typedef int(*video_capturer_ctx_alloc)(struct video_capturer_ctx **ctx,
                                                   video_frame_push vfp,
                                                   struct vidsrc_prm *prm, const struct video_sz *size, void *arg);
enum video_fmt {
    VIDEO_FMT_YUV420P = 0, /* planar YUV  4:2:0   12bpp                 */
    VIDEO_FMT_YUYV422,      /* packed YUV  4:2:2   16bpp                 */
    VIDEO_FMT_UYVY422,      /* packed YUV  4:2:2   16bpp                 */
    VIDEO_FMT_RGB32,        /* packed RGBA 8:8:8:8 32bpp (native endian) */
    VIDEO_FMT_ARGB,         /* packed RGBA 8:8:8:8 32bpp (big endian)    */
    VIDEO_FMT_RGB565,       /* packed RGB  5:6:5   16bpp (native endian) */
    VIDEO_FMT_RGB555,       /* packed RGB  5:5:5   16bpp (native endian) */
    VIDEO_FMT_NV12,         /* planar YUV  4:2:0   12bpp UV interleaved  */
    VIDEO_FMT_NV21,         /* planar YUV  4:2:0   12bpp VU interleaved  */
    VIDEO_FMT_YUV444P,      /* planar YUV  4:4:4   24bpp                 */
    /* marker */
    VIDEO_FMT_N
};

struct videosrc_prm {
    double fps;       /**< Wanted framerate                            */
    int fmt;          /**< Wanted pixel format (enum vidfmt)           */
};

struct video_sz {
    unsigned w;  /**< Width  */
    unsigned h;  /**< Height */
};

struct video_frame {
    unsigned char *data[4];      /**< Video planes        */
    unsigned short linesize[4];  /**< Array of line-sizes */
    struct video_sz size;     /**< Frame resolution    */
    enum video_fmt fmt;       /**< Video pixel format  */
};


/*
 *
 * 消息类型定义
 *
 **/
typedef enum  RtcMessageType {
//    RTC_MESSAGE_SIGNAL_CHENNAL_DISCONNECTED,
//  RTC_MESSAGE_LOGIN_ROOM_SUCCESS,
    RTC_MESSAGE_ROOM_EVENT_LOGIN_OK                    = 100,
    RTC_MESSAGE_ROOM_EVENT_LOGIN_TIMEOUT               = 101,
    RTC_MESSAGE_ROOM_EVENT_LOGIN_ERROR                 = 102,
    RTC_MESSAGE_ROOM_EVENT_CONNECTION_LOST             = 103,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_COMING               = 104,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_LEAVING              = 105,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_RENDERING            = 106,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_GONE                 = 107,
    RTC_MESSAGE_ROOM_EVENT_SERVER_ERROR                = 108,

    RTC_ROOM_EVENT_AVAILABLE_SEND_BITRATE                 = 200,
    RTC_ROOM_EVENT_FORCE_KEY_FRAME                     = 201,

    RTC_ROOM_EVENT_ON_USER_JOINED_ROOM                 = 300,
    RTC_ROOM_EVENT_ON_USER_LEAVING_ROOM                = 301,
    RTC_ROOM_EVENT_ON_USER_MESSAGE                     = 302,
    RTC_ROOM_EVENT_ON_USER_ATTRIBUTE                   = 303,

    RTC_MESSAGE_STATE_STREAM_UP                        = 2000,
    RTC_MESSAGE_STATE_SENDING_MEDIA_OK                 = 2001,
    RTC_MESSAGE_STATE_SENDING_MEDIA_FAILED             = 2002,
    RTC_MESSAGE_STATE_STREAM_DOWN                      = 2003,
    RTC_STATE_STREAM_SLOW_LINK_NACKS                   = 2100,
}RtcMessageType;

typedef struct  RtcMessage {
    RtcMessageType msgType;
    union {
        int64_t feedId;
        int64_t streamId;
        int64_t errorCode;
    }data;
    const char* extra_info;
    int frameObserverIndex;
    void* obj;
}RtcMessage;


#ifdef __cplusplus
}
#endif