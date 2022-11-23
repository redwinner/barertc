#include "baidu_rtc_client_session.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct mnat *mnat;
extern const struct menc *menc_srtp;
static uint32_t session_counter;

static struct configuration pc_config;
static struct list sessl;

void client_session_destructor(void *data)
{
	struct client_session *sess = data;

	list_unlink(&sess->le);
	mem_deref(sess->pc);
}

int client_session_new(struct client_session **sessp, unsigned long long handle_id, unsigned char is_sender)
{
	struct client_session *sess;

	sess = mem_zalloc(sizeof(*sess), client_session_destructor);
    
    sess->handle_id = handle_id;

	list_append(&sessl, &sess->le, sess);

	*sessp = sess;

	return 0;
}


struct client_session *client_session_lookup(unsigned long long handle_id)
{
	struct le *le;

	for (le = sessl.head; le; le = le->next) {

		struct client_session *sess = le->data;

		if (handle_id == sess->handle_id)
			return sess;
	}

	return NULL;
}


static void reply_fmt(struct client_session *sess, const char *ctype,
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

	sess->brtc_client->mFRTCconn->publisherCreateOffer(sess->brtc_client->mFRTCconn, sess->handle_id,
	       (void *)buf);

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
static int reply_descr(void* arg, enum sdp_type type, struct mbuf *mb_sdp)
{
	struct client_session *sess = arg;
    reply_fmt(sess, "application/json",
        "%H", utf8_encode, mb_sdp->buf);
    // info("reply_descr -- exit.\n");

	return 0;
}

int client_session_create_offer(struct client_session *sess, enum sdp_type type)
{
	struct mbuf *mb_sdp = NULL;
	enum signaling_st ss;
	int err;

	ss = peerconnection_signaling(sess->pc);

	info("client_session_create_offer -- send %s\n", sdptype_name(type));

	err = peerconnection_create_offer(sess->pc, &mb_sdp);

    mb_sdp->buf[mb_sdp->end] = '\0';
	err = reply_descr(sess, type, mb_sdp);
	if (err) {
		warning("clientsession: reply error: %m\n", err);
		goto out;
	}

 out:
	mem_deref(mb_sdp);
}

static void sendanswer_fmt(struct client_session *sess, const char *ctype,
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

	sess->brtc_client->mFRTCconn->subscriberCreateAnswer(sess->brtc_client->mFRTCconn, sess->handle_id,
	       (void *)buf);

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
static int sendanswer_descr(void* arg, enum sdp_type type, struct mbuf *mb_sdp)
{
	struct client_session *sess = arg;

	mb_sdp->buf[mb_sdp->end] = '\0';

	info("client_session_create_answer -- send %s\n", sdptype_name(type));

    sendanswer_fmt(sess, "application/json",
        "%H", utf8_encode, mb_sdp->buf);

	return 0;
}


int client_session_create_answer(struct client_session *sess, enum sdp_type type)
{
	struct mbuf *mb_sdp = NULL;
	enum signaling_st ss;
	int err;

	ss = peerconnection_signaling(sess->pc);

	info("client_session_create_answer -- send %s\n", sdptype_name(type));

	err = peerconnection_create_answer(sess->pc, &mb_sdp);

	err = sendanswer_descr(sess, type, mb_sdp);
	if (err) {
		goto out;
	}

 out:
	mem_deref(mb_sdp);
}

static void peerconnection_gather_handler(void *arg)
{
	struct client_session *sess = arg;
	struct mbuf *mb_sdp = NULL;
	enum signaling_st ss;
	enum sdp_type type;
	int err;

	ss = peerconnection_signaling(sess->pc);
	type = (ss != SS_HAVE_REMOTE_OFFER) ? SDP_OFFER : SDP_ANSWER;

	info("clientsession: session gathered -- send %s\n", sdptype_name(type));
/*
	if (type == SDP_OFFER)
		err = peerconnection_create_offer(sess->pc, &mb_sdp);
	else
		err = peerconnection_create_answer(sess->pc, &mb_sdp);
	if (err)
		return;

	err = reply_descr(arg, type, mb_sdp);
	if (err) {
		warning("clientsession: reply error: %m\n", err);
		goto out;
	}*/

	// if (type == SDP_ANSWER) {

		err = peerconnection_start_ice(sess->pc);
		if (err) {
			warning("clientsession: failed to start ice (%m)\n", err);
			//goto out;
		}
	// }

 // out:
	//mem_deref(mb_sdp);
}


static void peerconnection_estab_handler(struct media_track *media, void *arg)
{
	int err;

	(void)arg;

	info("clientsession: stream established: '%s'\n", media_kind_name(media->kind));

	switch (media->kind) {

	case MEDIA_KIND_AUDIO:
		err = mediatrack_start_audio(media, baresip_ausrcl(),
					     baresip_aufiltl());
		if (err) {
			warning("clientsession: could not start audio (%m)\n", err);
		}
		break;

	case MEDIA_KIND_VIDEO:
		err = mediatrack_start_video(media);
		if (err) {
			warning("clientsession: could not start video (%m)\n", err);
		}
		break;
	}
}


static void peerconnection_close_handler(int err, void *arg)
{
	struct client_session *sess = arg;

	warning("clientsession: session closed (%m)\n", err);

	/* todo: notify client that session was closed */
	sess->pc = mem_deref(sess->pc);
}


/* RemoteDescription */
int client_session_create_pc(struct client_session *sess, enum sdp_type type)
{
	const struct config *config = conf_config();
	bool is_offer = (type == SDP_OFFER);
	int err;

	info("client_session: create pc (type=%s)\n", sdptype_name(type));

	/* create a new session object, send SDP to it */
	err = peerconnection_new(&sess->pc, &pc_config, !is_offer, mnat, menc_srtp,
				 peerconnection_gather_handler,
				 peerconnection_estab_handler,
				 peerconnection_close_handler, sess);
	if (err) {
		warning("clientsession: session alloc failed (%m)\n", err);
		goto out;
	}

	err = peerconnection_add_audio(sess->pc, config, baresip_aucodecl());
	if (err) {
		warning("clientsession: add_audio failed (%m)\n", err);
		goto out;
	}

	//err = peerconnection_add_video(sess->pc, config, baresip_vidcodecl());
	//if (err) {
	//	warning("clientsession: add_video failed (%m)\n", err);
	//	goto out;
	//}

 out:
	return err;
}


int client_session_set_remote_description(struct client_session *sess, struct mbuf *jsep)
{
	struct session_description sd = {-1, NULL};
	int err = 0;

    err = session_description_decode(&sd, jsep);
    if (err)
        goto out;

    err = peerconnection_set_remote_descr(sess->pc,
                            &sd);
    if (err) {
        warning("clientsession: set remote descr error"
            " (%m)\n", err);
        goto out;
    }

    // if (sd.type == SDP_ANSWER) {
    //     err = peerconnection_start_ice(sess->pc);
    //     if (err) {
    //         warning("clientsession: failed to start ice"
    //             " (%m)\n", err);
    //         goto out;
    //     }
    // }
out:
	session_description_reset(&sd);

	return err;
}


#ifdef __cplusplus
}
#endif
