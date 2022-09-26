/**
 * @file main.c Main application code
 *
 *
 */

#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <re.h>
#include <rem.h>
#include <baresip.h>
#include "barertc.h"

#define DEBUG_MODULE "barertc"
#define DEBUG_LEVEL 6
#include <re_dbg.h>

enum {
	HTTP_PORT  = 9000
};

static const char *modpath = "/usr/local/lib/baresip/modules";


static const char *modv[] = {
	"ice",
	"dtls_srtp",
	"srtp",
	//"opus",
	"g711",
	"aufile",
	//"avcodec",
	//"fakevideo",
	//"avformat",
	"ausine"
};

static const char *ice_server = "stun:192.168.1.221:3478";  // "stun:stun.l.google.com:19302";

static const char *modconfig =
	"opus_bitrate       96000\n"
	"opus_stereo        yes\n"
	"opus_sprop_stereo  yes\n"
	;


static void signal_handler(int signum)
{
	(void)signum;

	re_fprintf(stderr, "terminated on signal %d\n", signum);

	re_cancel();
}


static void usage(void)
{
	re_fprintf(stderr,
		   "Usage: barertc [options]\n"
		   "\n"
		   "options:\n"
                   "\t-h               Help\n"
		   "\t-v               Verbose debug\n"
		   "\t-q               http port\n"
		   "\n"
		   "ice:\n"
		   "\t-i <server>      ICE server (%s)\n"
		   "\t-u <username>    ICE username\n"
		   "\t-p <password>    ICE password\n"
		   "\n",
		   ice_server);
}


int main(int argc, char *argv[])
{
	struct config *config;
	const char *stun_user = NULL, *stun_pass = NULL;
	size_t i;
	int err = 0;
    int http_port = HTTP_PORT;

	for (;;) {

		const int c = getopt(argc, argv, "hl:i:u:tvu:p:q:");
		if (0 > c)
			break;

		switch (c) {

		case '?':
		default:
			err = EINVAL;
			/*@fallthrough@*/

		case 'h':
			usage();
			return err;

		case 'i':
			if (0 == str_casecmp(optarg, "null"))
				ice_server = NULL;
			else
				ice_server = optarg;
			break;

		case 'u':
			stun_user = optarg;
			break;

		case 'p':
			stun_pass = optarg;
			break;

		case 'q':
			http_port = atoi(optarg);
			break;

		case 'v':
			log_enable_debug(true);
			break;
		}
	}

	if (argc < 1 || (argc != (optind + 0))) {
		usage();
		return -2;
	}

	err = libre_init();
	if (err) {
		(void)re_fprintf(stderr, "libre_init: %m\n", err);
		goto out;
	}

	(void)sys_coredump_set(true);

	err = conf_configure_buf((uint8_t *)modconfig, str_len(modconfig));
	if (err) {
		warning("main: configure failed: %m\n", err);
		goto out;
	}

	/*
	 * Initialise the top-level baresip struct, must be
	 * done AFTER configuration is complete.
	 */
	err = baresip_init(conf_config());
	if (err) {
		warning("main: baresip init failed (%m)\n", err);
		goto out;
	}

	for (i=0; i<ARRAY_SIZE(modv); i++) {

		err = module_load(modpath, modv[i]);
		if (err) {
			re_fprintf(stderr,
				   "could not pre-load module"
				   " '%s' (%m)\n", modv[i], err);
		}
	}

	config = conf_config();

	str_ncpy(config->audio.src_mod, "aufile",
		 sizeof(config->audio.src_mod));
	str_ncpy(config->audio.src_dev, "mypcm.wav",
		 sizeof(config->audio.src_dev));

	//str_ncpy(config->video.src_mod, "avformat",
	//	 sizeof(config->video.src_mod));
	//str_ncpy(config->video.src_dev, "lavfi,testsrc2",
	//	 sizeof(config->video.src_dev));

	config->video.bitrate = 2000000;
	config->video.fps = 30.0;

	/* override default config */
	config->avt.rtcp_mux = true;

	err = demo_init(ice_server, stun_user, stun_pass, http_port);
	if (err) {
		re_fprintf(stderr, "failed to init demo: %m\n", err);
		goto out;
	}

	(void)re_main(signal_handler);

	re_printf("Bye for now\n");

 out:
	demo_close();

	/* note: must be done before mod_close() */
	module_app_unload();

	conf_close();

	baresip_close();

	/* NOTE: modules must be unloaded after all application
	 *       activity has stopped.
	 */
	debug("main: unloading modules..\n");
	mod_close();

	libre_close();

	/* Check for memory leaks */
	tmr_debug();
	mem_debug();

	return err;
}
