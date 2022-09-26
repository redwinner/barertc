/**
 * @file barertc.c
 *
 *
 */

#include <string.h>
#include <re.h>
#include <baresip.h>
#include "barertc.h"

struct session {
	struct le le;
	struct peer_connection *pc;
	char *id;
};

static struct http_sock *httpsock;
static struct http_sock *httpssock;
static struct http_conn *conn_pending;
const struct mnat *mnat;
const struct menc *menc;
const struct menc *menc_srtp;
static uint32_t session_counter;


static struct configuration pc_config;
static struct list sessl;

#include "brtc/baidu_rtc_interface.h"
static void* gBrtcClient = NULL;

static char appid[50];

const char *s_url = "ws://rtc.exp.bcelive.com:8186/janus"; // "wss://rtc.exp.bcelive.com/janus"
                                                        // or "ws://183.194.218.96:8188/janus";
														// "ws://rtc-old-sandbox.acgrtc.com:8188/janus";

const char *ca_info = "-----BEGIN CERTIFICATE-----\r\n\
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\
-----END CERTIFICATE-----";

static void OnRtcMessage(RtcMessage* msg)
{
    printf("OnRtcMessage %d\r\n", msg->msgType);
}

static int brtc_cmd_handler(char obj, const char *d)
{
    bool ret = false;

    if(obj == 'c') {
        //初始化
        if (gBrtcClient == NULL) {
            printf("create client success\n");
            gBrtcClient = brtc_create_client();
        }

        if (gBrtcClient) {
            brtc_register_message_listener(gBrtcClient, OnRtcMessage);

            ret = brtc_init_client(gBrtcClient);
            brtc_set_cer(gBrtcClient, ca_info);
            brtc_set_server_url(gBrtcClient, s_url);
            brtc_set_appid(gBrtcClient, appid);

            brtc_set_auto_publish(gBrtcClient, 1);
            if (ret) {
                printf("brtc_init_sdk success\n");
            } else {
                printf("brtc_init_sdk failed\n");
            }
        }

    } else if(obj == 'i') {
        if (gBrtcClient) {
            ret = brtc_login_room(gBrtcClient, "2131", "10001", "BRTC RTOS V3.0 demo", "no_token");
            if (ret) {
                printf("brtc_login_room success\n");
            } else {
                printf("brtc_login_room failed\n");
            }
        }
    } else if(obj == 'o') {
        if (gBrtcClient) {
            ret = brtc_logout_room(gBrtcClient);
            if (ret) {
                printf("logout room success\r\n");
            } else {
                printf("logout room failed\r\n");
            }
        }
    } else if(obj == 'd') {
        if (gBrtcClient) {
            brtc_deinit_client(gBrtcClient);
            brtc_destroy_client(gBrtcClient);
            gBrtcClient = NULL;
            printf("destroy success\r\n");
        }
    }  else if(obj == 'm') {
        if (gBrtcClient) {
            brtc_send_message_to_user(gBrtcClient, d, "0");
        }
    } else if(obj == 's') {
        if (gBrtcClient) {
            brtc_start_publish(gBrtcClient);
        }
    }
    return 0;
}

static void destructor(void *data)
{
	struct session *sess = data;

	list_unlink(&sess->le);
	mem_deref(sess->pc);
	mem_deref(sess->id);
}


static int session_new(struct session **sessp)
{
	struct session *sess;

	sess = mem_zalloc(sizeof(*sess), destructor);

	/* generate a unique session id */

	re_sdprintf(&sess->id, "%u", ++session_counter);

	list_append(&sessl, &sess->le, sess);

	*sessp = sess;

	return 0;
}


static struct session *session_lookup(const struct pl *sessid)
{
	struct le *le;

	for (le = sessl.head; le; le = le->next) {

		struct session *sess = le->data;

		if (0 == pl_strcasecmp(sessid, sess->id))
			return sess;
	}

	return NULL;
}


static void reply_fmt(struct http_conn *conn, const char *ctype,
		      const char *fmt, ...)
{
	char *buf = NULL;
	va_list ap;
	int err;

	va_start(ap, fmt);
	err = re_vsdprintf(&buf, fmt, ap);
	va_end(ap);

	if (err)
		return;

	info("barertc: reply: %s\n", ctype);

	http_reply(conn, 200, "OK",
		   "Content-Type: %s\r\n"
		   "Content-Length: %zu\r\n"
		   "Access-Control-Allow-Origin: *\r\n"
		   "\r\n"
		   "%s",
		   ctype, str_len(buf), buf);

	mem_deref(buf);
}


/*
 * format:
 *
 * {
 *   "type" : "answer",
 *   "sdp" : "v=0\r\ns=-\r\n..."
 * }
 *
 * specification:
 *
 * https://developer.mozilla.org/en-US/docs/Web/API/RTCSessionDescription
 *
 * NOTE: currentLocalDescription
 */
static int reply_descr(enum sdp_type type, struct mbuf *mb_sdp)
{
	struct odict *od = NULL;
	int err;

	err = session_description_encode(&od, type, mb_sdp);
	if (err)
		goto out;

	reply_fmt(conn_pending, "application/json",
		  "%H", json_encode_odict, od);

 out:
	mem_deref(od);

	return err;
}


static void peerconnection_gather_handler(void *arg)
{
	struct session *sess = arg;
	struct mbuf *mb_sdp = NULL;
	enum signaling_st ss;
	enum sdp_type type;
	int err;

	ss = peerconnection_signaling(sess->pc);
	type = (ss != SS_HAVE_REMOTE_OFFER) ? SDP_OFFER : SDP_ANSWER;

	info("barertc: session gathered -- send %s\n", sdptype_name(type));

	if (type == SDP_OFFER)
		err = peerconnection_create_offer(sess->pc, &mb_sdp);
	else
		err = peerconnection_create_answer(sess->pc, &mb_sdp);
	if (err)
		return;

	err = reply_descr(type, mb_sdp);
	if (err) {
		warning("barertc: reply error: %m\n", err);
		goto out;
	}

	if (type == SDP_ANSWER) {

		err = peerconnection_start_ice(sess->pc);
		if (err) {
			warning("barertc: failed to start ice (%m)\n", err);
			goto out;
		}
	}

 out:
	mem_deref(mb_sdp);
}


static void peerconnection_estab_handler(struct media_track *media, void *arg)
{
	int err;

	(void)arg;

	info("barertc: stream established: '%s'\n", media_kind_name(media->kind));

	switch (media->kind) {

	case MEDIA_KIND_AUDIO:
		err = mediatrack_start_audio(media, baresip_ausrcl(),
					     baresip_aufiltl());
		if (err) {
			warning("barertc: could not start audio (%m)\n", err);
		}
		break;

	case MEDIA_KIND_VIDEO:
		err = mediatrack_start_video(media);
		if (err) {
			warning("barertc: could not start video (%m)\n", err);
		}
		break;
	}
}


static void peerconnection_close_handler(int err, void *arg)
{
	struct session *sess = arg;

	warning("barertc: session closed (%m)\n", err);

	/* todo: notify client that session was closed */
	sess->pc = mem_deref(sess->pc);
}


/* RemoteDescription */
static int create_pc(struct session *sess, enum sdp_type type)
{
	const struct config *config = conf_config();
	bool got_offer = (type == SDP_OFFER);
	int err;

	info("barertc: create session (type=%s)\n", sdptype_name(type));

	/* create a new session object, send SDP to it */
	err = peerconnection_new(&sess->pc, &pc_config, got_offer, mnat, menc,
				 peerconnection_gather_handler,
				 peerconnection_estab_handler,
				 peerconnection_close_handler, sess);
	if (err) {
		warning("barertc: session alloc failed (%m)\n", err);
		goto out;
	}

	err = peerconnection_add_audio(sess->pc, config, baresip_aucodecl());
	if (err) {
		warning("barertc: add_audio failed (%m)\n", err);
		goto out;
	}

	//err = peerconnection_add_video(sess->pc, config, baresip_vidcodecl());
	//if (err) {
	//	warning("demo: add_video failed (%m)\n", err);
	//	goto out;
	//}

 out:
	return err;
}


static int handle_post_sdp(struct session *sess, const struct http_msg *msg)
{
	struct session_description sd = {-1, NULL};
	bool got_offer = false;
	int err = 0;

	info("barertc: handle POST sdp: content is '%r/%r'\n",
	     &msg->ctyp.type, &msg->ctyp.subtype);

	if (msg->clen) {

		if (msg_ctype_cmp(&msg->ctyp, "application", "json")) {

			err = session_description_decode(&sd, msg->mb);
			if (err)
				goto out;

			if (sd.type == SDP_OFFER) {

				got_offer = true;
			}
			else if (sd.type == SDP_ANSWER) {

				err = peerconnection_set_remote_descr(sess->pc,
								      &sd);
				if (err) {
					warning("barertc: set remote descr error"
						" (%m)\n", err);
					goto out;
				}

				err = peerconnection_start_ice(sess->pc);
				if (err) {
					warning("barertc: failed to start ice"
						" (%m)\n", err);
					goto out;
				}
			}
			else {
				warning("barertc: invalid session description"
					" type:"
					" %s\n",
					sdptype_name(sd.type));
				err = EPROTO;
				goto out;
			}
		}
		else {
			warning("unknown content-type: %r/%r\n",
				&msg->ctyp.type, &msg->ctyp.subtype);
			err = EPROTO;
			goto out;
		}

		if (!sess->pc) {
			err = create_pc(sess, sd.type);
			if (err)
				goto out;
		}

		if (got_offer) {

			err = peerconnection_set_remote_descr(sess->pc, &sd);
			if (err) {
				warning("barertc: decode offer failed (%m)\n",
					err);
				goto out;
			}
		}
	}

out:
	session_description_reset(&sd);

	return err;
}


static void handle_get(struct http_conn *conn, const struct pl *path)
{
	const char *ext, *mime;
	struct mbuf *mb;
	char *buf = NULL;
	int err;

	mb = mbuf_alloc(8192);
	if (!mb)
		return;

	err = re_sdprintf(&buf, "./www%r", path);
	if (err)
		goto out;

	err = load_file(mb, buf);
	if (err) {
		info("barertc: not found: %s\n", buf);
		http_ereply(conn, 404, "Not Found");
		goto out;
	}

	ext = file_extension(buf);
	mime = extension_to_mimetype(ext);

	info("barertc: loaded file '%s', %zu bytes (%s)\n", buf, mb->end, mime);

	http_reply(conn, 200, "OK",
		   "Content-Type: %s;charset=UTF-8\r\n"
		   "Content-Length: %zu\r\n"
		   "Access-Control-Allow-Origin: *\r\n"
		   "\r\n"
		   "%b",
		   mime,
		   mb->end,
		   mb->buf, mb->end);

 out:
	mem_deref(mb);
	mem_deref(buf);
}


static void http_req_handler(struct http_conn *conn,
			     const struct http_msg *msg, void *arg)
{
	struct pl path = PL("/index.html");
	struct session *sess;
	int err = 0;
    char b[50];
	(void)arg;

	info("barertc: request: met=%r, path=%r, prm=%r\n",
	     &msg->met, &msg->path, &msg->prm);

	if (msg->path.l > 1)
		path = msg->path;

	if (0 == pl_strcasecmp(&msg->met, "GET")) {

        if (0 == pl_strcasecmp(&path, "/login")) {
            struct pl data = PL("?appid=xxx");

            if (msg->prm.l > 1)
                data = msg->prm;

            strncpy(appid, &data.p[7], data.l - 7);
            brtc_cmd_handler('c', "");
            brtc_cmd_handler('i', "");

            http_reply(conn, 200, "OK",
                    "Content-Length: 2\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "\r\nok");
        } else if (0 == pl_strcasecmp(&path, "/start")) {
            brtc_cmd_handler('s', "");
        } else if (0 == pl_strcasecmp(&path, "/stop")) {
            brtc_cmd_handler('o', "");
            brtc_cmd_handler('d', "");
        } else if (0 == pl_strcasecmp(&path, "/sendmsg")) {

            struct pl data = PL("?d=sendmsg from barertc");

            if (msg->prm.l > 1)
                data = msg->prm;

            strncpy(b, data.p, data.l);

            brtc_cmd_handler('m', &b[3]);
            http_reply(conn, 200, "OK",
                    "Content-Length: 2\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "\r\nok");
        } else {
            handle_get(conn, &path);
        }
	}
	else if (0 == pl_strcasecmp(&msg->met, "POST") &&
		 0 == pl_strcasecmp(&msg->path, "/connect")) {

		err = session_new(&sess);
		if (err)
			goto out;

		/* sync reply */
		http_reply(conn, 200, "OK",
			   "Content-Length: 0\r\n"
			   "Access-Control-Allow-Origin: *\r\n"
			   "Session-ID: %s\r\n"
			   "\r\n", sess->id);
	}
	else if (0 == pl_strcasecmp(&msg->met, "POST") &&
		 0 == pl_strcasecmp(&msg->path, "/sdp")) {

		const struct http_hdr *hdr;

		hdr = http_msg_xhdr(msg, "Session-ID");
		if (!hdr) {
			warning("barertc: no Session-ID header\n");
			err = EPROTO;
			goto out;
		}

		info(".... sdp: session-id '%r'\n", &hdr->val);

		sess = session_lookup(&hdr->val);
		if (sess) {
			err = handle_post_sdp(sess, msg);
			if (err)
				goto out;

			/* async reply */
			mem_deref(conn_pending);
			conn_pending = mem_ref(conn);
		}
		else {
			warning("barertc: sdp: session not found (%r)\n",
				&hdr->val);
			http_ereply(conn, 404, "Session Not Found");
			return;
		}
	}
	else if (0 == pl_strcasecmp(&msg->met, "POST") &&
		 0 == pl_strcasecmp(&msg->path, "/disconnect")) {

		const struct http_hdr *hdr;

		info("barertc: disconnect\n");

		hdr = http_msg_xhdr(msg, "Session-ID");
		if (!hdr) {
			warning("barertc: no Session-ID header\n");
			err = EPROTO;
			goto out;
		}

		sess = session_lookup(&hdr->val);
		if (sess) {
			info("barertc: closing session %s\n", sess->id);
			mem_deref(sess);

			http_reply(conn, 200, "OK",
				   "Content-Length: 0\r\n"
				   "Access-Control-Allow-Origin: *\r\n"
				   "\r\n");
		}
		else {
			warning("barertc: session not found (%r)\n", &hdr->val);
			http_ereply(conn, 404, "Session Not Found");
			return;
		}
	}
	else {
		warning("barertc: not found: %r %r\n", &msg->met, &msg->path);
		http_ereply(conn, 404, "Not Found");
	}

 out:
	if (err)
		http_ereply(conn, 500, "Server Error");
}


int demo_init(const char *ice_server,
	      const char *stun_user, const char *credential,
	      int http_port)
{
	struct pl srv;
	struct sa laddr, laddrs;
	int err;

	if (ice_server) {
		pl_set_str(&srv, ice_server);

		err = stunuri_decode(&pc_config.ice_server, &srv);
		if (err) {
			warning("barertc: invalid iceserver '%r' (%m)\n",
				&srv, err);
			return err;
		}
	}

	pc_config.stun_user = stun_user;
	pc_config.credential = credential;

	mnat = mnat_find(baresip_mnatl(), "ice");
	if (!mnat) {
		warning("barertc: medianat 'ice' not found\n");
		return ENOENT;
	}

	menc = menc_find(baresip_mencl(), "dtls_srtp");
	if (!menc) {
		warning("barertc: mediaenc 'dtls_srtp' not found\n");
		return ENOENT;
	}

	menc_srtp = menc_find(baresip_mencl(), "srtp-rtp");
	if (!menc) {
		warning("barertc: mediaenc 'srtp-rtp' not found\n");
		return ENOENT;
	}

	sa_set_str(&laddr, "0.0.0.0", http_port);

	err = http_listen(&httpsock, &laddr, http_req_handler, NULL);
	if (err)
		return err;

	//err = https_listen(&httpssock, &laddrs, "./share/cert.pem",
	//		   http_req_handler, NULL);
	if (err)
		return err;

	info("barertc: listening on:\n");
	info("    http://localhost:%u/\n", sa_port(&laddr));

	return 0;
}


void demo_close(void)
{
	list_flush(&sessl);

	conn_pending = mem_deref(conn_pending);
	httpssock = mem_deref(httpssock);
	httpsock = mem_deref(httpsock);
	pc_config.ice_server = mem_deref(pc_config.ice_server);
}
