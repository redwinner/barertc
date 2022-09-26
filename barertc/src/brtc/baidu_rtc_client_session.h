#ifndef BRTC_RTC_CLIENT_SESSION_H_
#define BRTC_RTC_CLIENT_SESSION_H_

#include "baidu_rtc_common_define.h"
#include "baidu_rtc_signal_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <re.h>
#include <baresip.h>

#include "../barertc.h"

struct client_session {
	struct le le;
	struct peer_connection *pc;
	unsigned long long handle_id;
	BaiduRtcClient *brtc_client;
};

int client_session_new(struct client_session **sessp, unsigned long long handle_id, unsigned char is_sender);
struct client_session *client_session_lookup(unsigned long long handle_id);
int client_session_create_pc(struct client_session *sess, enum sdp_type type);
int client_session_set_remote_description(struct client_session *sess, struct mbuf *jsep);
int client_session_create_offer(struct client_session *sess, enum sdp_type type);
int client_session_create_answer(struct client_session *sess, enum sdp_type type);

#ifdef __cplusplus
}
#endif

#endif

