/**
 * @file demo.c  Baresip WebRTC demo
 *
 * Copyright (C) 2010 Alfred E. Heggestad
 */

#include <string.h>
#include <re.h>
#include <baresip.h>
#include "demo.h"


enum {
	HTTP_PORT  = 9000,
	HTTPS_PORT = 9001
};


struct session {
	struct le le;
	struct peer_connection *pc;
	char *id;
};


static struct http_sock *httpsock;
static struct http_sock *httpssock;
static struct http_conn *conn_pending;
static const struct mnat *mnat;
static const struct menc *menc;
static uint32_t session_counter;


static struct configuration pc_config;
static struct list sessl;


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

	info("demo: reply: %s\n", ctype);

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

	info("demo: session gathered -- send %s\n", sdptype_name(type));

	if (type == SDP_OFFER)
		err = peerconnection_create_offer(sess->pc, &mb_sdp);
	else
		err = peerconnection_create_answer(sess->pc, &mb_sdp);
	if (err)
		return;

	err = reply_descr(type, mb_sdp);
	if (err) {
		warning("demo: reply error: %m\n", err);
		goto out;
	}

	if (type == SDP_ANSWER) {

		err = peerconnection_start_ice(sess->pc);
		if (err) {
			warning("demo: failed to start ice (%m)\n", err);
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

	info("demo: stream established: '%s'\n", media_kind_name(media->kind));

	switch (media->kind) {

	case MEDIA_KIND_AUDIO:
		err = mediatrack_start_audio(media, baresip_ausrcl(),
					     baresip_aufiltl());
		if (err) {
			warning("demo: could not start audio (%m)\n", err);
		}
		break;

	case MEDIA_KIND_VIDEO:
		err = mediatrack_start_video(media);
		if (err) {
			warning("demo: could not start video (%m)\n", err);
		}
		break;
	}
}


static void peerconnection_close_handler(int err, void *arg)
{
	struct session *sess = arg;

	warning("demo: session closed (%m)\n", err);

	/* todo: notify client that session was closed */
	sess->pc = mem_deref(sess->pc);
}


/* RemoteDescription */
static int create_pc(struct session *sess, enum sdp_type type)
{
	const struct config *config = conf_config();
	bool got_offer = (type == SDP_OFFER);
	int err;

	info("demo: create session (type=%s)\n", sdptype_name(type));

	/* create a new session object, send SDP to it */
	err = peerconnection_new(&sess->pc, &pc_config, got_offer, mnat, menc,
				 peerconnection_gather_handler,
				 peerconnection_estab_handler,
				 peerconnection_close_handler, sess);
	if (err) {
		warning("demo: session alloc failed (%m)\n", err);
		goto out;
	}

	err = peerconnection_add_audio(sess->pc, config, baresip_aucodecl());
	if (err) {
		warning("demo: add_audio failed (%m)\n", err);
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

	info("demo: handle POST sdp: content is '%r/%r'\n",
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
					warning("demo: set remote descr error"
						" (%m)\n", err);
					goto out;
				}

				err = peerconnection_start_ice(sess->pc);
				if (err) {
					warning("demo: failed to start ice"
						" (%m)\n", err);
					goto out;
				}
			}
			else {
				warning("demo: invalid session description"
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
				warning("demo: decode offer failed (%m)\n",
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
		info("demo: not found: %s\n", buf);
		http_ereply(conn, 404, "Not Found");
		goto out;
	}

	ext = file_extension(buf);
	mime = extension_to_mimetype(ext);

	info("demo: loaded file '%s', %zu bytes (%s)\n", buf, mb->end, mime);

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
	(void)arg;

	info("demo: request: met=%r, path=%r, prm=%r\n",
	     &msg->met, &msg->path, &msg->prm);

	if (msg->path.l > 1)
		path = msg->path;

	if (0 == pl_strcasecmp(&msg->met, "GET")) {

		handle_get(conn, &path);
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
			warning("demo: no Session-ID header\n");
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
			warning("demo: sdp: session not found (%r)\n",
				&hdr->val);
			http_ereply(conn, 404, "Session Not Found");
			return;
		}
	}
	else if (0 == pl_strcasecmp(&msg->met, "POST") &&
		 0 == pl_strcasecmp(&msg->path, "/disconnect")) {

		const struct http_hdr *hdr;

		info("demo: disconnect\n");

		hdr = http_msg_xhdr(msg, "Session-ID");
		if (!hdr) {
			warning("demo: no Session-ID header\n");
			err = EPROTO;
			goto out;
		}

		sess = session_lookup(&hdr->val);
		if (sess) {
			info("demo: closing session %s\n", sess->id);
			mem_deref(sess);

			http_reply(conn, 200, "OK",
				   "Content-Length: 0\r\n"
				   "Access-Control-Allow-Origin: *\r\n"
				   "\r\n");
		}
		else {
			warning("demo: session not found (%r)\n", &hdr->val);
			http_ereply(conn, 404, "Session Not Found");
			return;
		}
	}
	else {
		warning("demo: not found: %r %r\n", &msg->met, &msg->path);
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
			warning("demo: invalid iceserver '%r' (%m)\n",
				&srv, err);
			return err;
		}
	}

	pc_config.stun_user = stun_user;
	pc_config.credential = credential;

	mnat = mnat_find(baresip_mnatl(), "ice");
	if (!mnat) {
		warning("demo: medianat 'ice' not found\n");
		return ENOENT;
	}

	menc = menc_find(baresip_mencl(), "dtls_srtp");
	if (!menc) {
		warning("demo: mediaenc 'dtls_srtp' not found\n");
		return ENOENT;
	}

	sa_set_str(&laddr, "0.0.0.0", http_port);
	sa_set_str(&laddrs, "0.0.0.0", HTTPS_PORT);

	err = http_listen(&httpsock, &laddr, http_req_handler, NULL);
	if (err)
		return err;

	//err = https_listen(&httpssock, &laddrs, "./share/cert.pem",
	//		   http_req_handler, NULL);
	if (err)
		return err;

	info("demo: listening on:\n");
	info("    http://localhost:%u/\n", sa_port(&laddr));
	info("    https://localhost:%u/\n", sa_port(&laddrs));

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
