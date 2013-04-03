/* mod_websocket implementation */
#include <turbulence.h>

/* use this declarations to avoid c++ compilers to mangle exported
 * names. */
BEGIN_C_DECLS

#include <vortex_websocket.h>

/* global turbulence context reference */
TurbulenceCtx * ctx = NULL;

/* reference to the module configuration */
axlDoc * mod_websocket_conf = NULL;
noPollCtx  * nopoll_ctx = NULL;

/** 
 * @internal Load websocket.conf file.
 */
axl_bool mod_websocket_load_config (void) {
	char     * config;
	axlError * error = NULL;
	char     * path;

	if (mod_websocket_conf == NULL) {
		/* websocket.conf not loaded */
		path = vortex_support_build_filename (turbulence_sysconfdir (ctx), "turbulence", "websocket", NULL);
		msg ("Adding search path: %s", path);
		vortex_support_add_domain_search_path_ref (TBC_VORTEX_CTX(ctx), axl_strdup ("websocket"), path);

		/* load configuration file */
		config  = vortex_support_domain_find_data_file (TBC_VORTEX_CTX(ctx), "websocket", "websocket.conf");
		if (config == NULL) {
			error ("Unable to find websocket.conf file under expected locations, failed to activate websocket support (try checking %s/turbulence/websocket/websocket.conf)",
			       turbulence_sysconfdir (ctx));
			return axl_false;
		}
		msg ("loading mod-websocket config: %s", config);
		mod_websocket_conf  = axl_doc_parse_from_file (config, &error);
		axl_free (config);
		if (mod_websocket_conf == NULL) {
			error ("failed to load mod-websocket configuration file, error found was: %s", 
			       axl_error_get (error));
			axl_error_free (error);
			return axl_false;
		} /* end if */
	} else {
		msg ("mod-websocket config already loaded..");
	} /* end if */

	return axl_true;
}

/** 
 * @internal Post action function called to prepare each websocket
 * connection to support sending it to a child.
 */
int mod_websocket_post_configuration (VortexCtx               * _ctx, 
				      VortexConnection        * conn, 
				      VortexConnection       ** new_conn, 
				      VortexConnectionStage     stage, 
				      axlPointer                user_data)
{
	TurbulenceCtx * ctx = user_data;

	/* do not configure anything that isn't an accepted listener connection */
	if (vortex_connection_get_role (conn) != VortexRoleListener)
		return 1;

	/* do not change anything if it is not a websocket connection */
	if (! vortex_websocket_connection_is (conn))
		return 1;

	/* prepare this connection to be proxied on parent if sent to a child */
	msg ("WEBSOCKET: flagged connection-id=%d to be proxied on parent", vortex_connection_get_id (conn));
	vortex_connection_set_data (conn, "tbc:proxy:conn", INT_TO_PTR (axl_true));

	return 1;
}

/* mod_websocket init handler */
static int  mod_websocket_init (TurbulenceCtx * _ctx) {
	axlNode    * node;
	const char * port;
	axl_bool     enable_tls;

	/* cert and key path to be used on each particular host */
	const char * cert;
	const char * key;
	noPollConn * nopoll_listener;
	
	/* listener */
	VortexConnection * listener;

	/* configure the module */
	TBC_MOD_PREPARE (_ctx);

	/* add profile attr alias to allow detecting the TLS profile
	 * using the same configuration as mod-tls so configuration
	 * remains coherent */
	turbulence_ppath_add_profile_attr_alias (ctx, "http://iana.org/beep/TLS", "tls-fication:status");

	/* if this is a child process, we don't need to go through the
	 * initialization */
	if (turbulence_ctx_is_child (ctx)) 
		return axl_true;

	/* ok, get current configuration and start listener ports
	 * according to its configuration */
	if (! mod_websocket_load_config ()) 
		return axl_false;

	/* init context */
	nopoll_ctx = nopoll_ctx_new ();

	/* check for debug enable=yes */
	node = axl_doc_get (mod_websocket_conf, "/mod-websocket/general-settings/debug");
	if (HAS_ATTR_VALUE (node, "enable", "yes")) {
		nopoll_log_enable (nopoll_ctx, nopoll_true);
		nopoll_log_color_enable (nopoll_ctx, nopoll_true);
	} /* end if */

	/* now for each listener start it */
	node = axl_doc_get (mod_websocket_conf, "/mod-websocket/ports/port");
	while (node) {
		/* get port */
		port       = axl_node_get_content_trim (node, NULL);
		enable_tls = HAS_ATTR_VALUE (node, "enable-tls", "yes");
		if (! port) 
			continue;

		/* check if tls was requested */
		cert = NULL;
		key  = NULL;
		if (enable_tls) {
			if (! HAS_ATTR (node, "cert") || ! HAS_ATTR (node, "key")) {
				error ("Unable to activate secure web socket server listesner, failed to get valid certificates to set it up");
				return axl_false;
			} /* end if */

			/* get cert and key */
			cert = ATTR_VALUE (node, "cert");
			key  = ATTR_VALUE (node, "key");
		} /* end if */

		msg ("Websocket (noPoll): attempting to starting listener port on %s, enable-tls=%d%s%s%s%s", 
		     port, enable_tls,
		     cert ? ", cert=" : "",
		     cert ? cert : "",
		     key ? ", key=" : "",
		     key ? key : "");

		/* create the noPoll listener */
		if (enable_tls) {
			nopoll_listener = nopoll_listener_tls_new (nopoll_ctx, "0.0.0.0", port); 

			/* set listener certificate */
			nopoll_listener_set_certificate (nopoll_listener, cert, key, NULL);
		} else
			nopoll_listener = nopoll_listener_new (nopoll_ctx, "0.0.0.0", port);

		/* now associate that listener to BEEP */
		listener = vortex_websocket_listener_new (TBC_VORTEX_CTX (_ctx), nopoll_listener, NULL, NULL);
		if (! vortex_connection_is_ok (listener, axl_false)) {
			error ("ERROR: expected to find proper BEEP listener over Websocket creation but failure was found..\n");
			return axl_false;
		} /* end if */

		msg ("Websocket (noPoll) listener socket started at: %s:%s", vortex_connection_get_host (listener), vortex_connection_get_port (listener));

		/* get next port */
		node = axl_node_get_next_called (node, "port");
	} /* end while */

	/* install post action function */
	vortex_connection_set_connection_actions (TBC_VORTEX_CTX (_ctx), CONNECTION_STAGE_POST_CREATED, mod_websocket_post_configuration, _ctx);

	return axl_true;
} /* end mod_websocket_init */

/* mod_websocket close handler */
static void mod_websocket_close (TurbulenceCtx * _ctx) {
	msg ("WEBSOCKET: closing module..");

	/* release module allocated memory */
	axl_doc_free (mod_websocket_conf);
	mod_websocket_conf = NULL;

	nopoll_ctx_unref (nopoll_ctx);
	nopoll_ctx = NULL;

	/* cleanup library */
	nopoll_cleanup_library ();

	return;
} /* end mod_websocket_close */

/* mod_websocket reconf handler */
static void mod_websocket_reconf (TurbulenceCtx * _ctx) {
	/* reload */

	return;
} /* end mod_websocket_reconf */

/* mod_websocket unload handler */
static void mod_websocket_unload (TurbulenceCtx * _ctx) {
	/* nothing to be done yet */
	return;
} /* end mod_websocket_unload */

/* mod_websocket ppath-selected handler */
static axl_bool mod_websocket_ppath_selected (TurbulenceCtx * _ctx, TurbulencePPathDef * ppath_selected, VortexConnection * conn) {
	return axl_true;
	
} /* end mod_websocket_ppath_selected */

/* Entry point definition for all handlers included in this module */
TurbulenceModDef module_def = {
	"mod_websocket",
	"Support for BEEP over WebSocket (through libvortex-websocket-1.1)",
	mod_websocket_init,
	mod_websocket_close,
	mod_websocket_reconf,
	mod_websocket_unload,
	mod_websocket_ppath_selected
};

END_C_DECLS
