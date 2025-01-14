/*  Turbulence BEEP application server
 *  Copyright (C) 2025 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; version 2.1 of the
 *  License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to develop
 *  proprietary applications using this library without any royalty or
 *  fee but returning back any change, improvement or addition in the
 *  form of source code, project image, documentation patches, etc.
 *
 *  For commercial support on build BEEP enabled solutions, supporting
 *  turbulence based solutions, etc, contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         C/ Antonio Suarez Nº10, Edificio Alius A, Despacho 102
 *         Alcala de Henares, 28802 (MADRID)
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/turbulence
 */
#include <turbulence-child.h>
#include <syslog.h>

/* local private include */
#include <turbulence-ctx-private.h>

/** 
 * @internal Creates a object that will represents a child object and
 * initialize internal data structures for its function.
 */
TurbulenceChild * turbulence_child_new (TurbulenceCtx * ctx, TurbulencePPathDef * def)
{
	TurbulenceChild * result;
	char            * temp_dir;
	struct timeval    now;

	result        = axl_new (TurbulenceChild, 1);
	/* check allocation */
	if (result == NULL)
		return NULL;

	/* get current time */
	gettimeofday (&now, NULL);

	/* create socket path: socket used to transfer file descriptors from parent to child */
	result->socket_control_path = axl_strdup_printf ("%s%s%s%s%p%d%d.tbc",
							 turbulence_runtime_datadir (ctx),
							 VORTEX_FILE_SEPARATOR,
							 "turbulence",
							 VORTEX_FILE_SEPARATOR,
							 result, now.tv_sec, now.tv_usec);
	/* check result */
	if (result->socket_control_path == NULL) {
		axl_free (result);
		return NULL;
	} /* end if */

	/* now check check base dir for socket control path exists */
	temp_dir = turbulence_base_dir (result->socket_control_path);
	if (temp_dir && ! vortex_support_file_test (temp_dir, FILE_EXISTS)) {
		/* base directory having child socket control do not exists */
		wrn ("run time directory %s do not exists, creating..", temp_dir);
		if (! turbulence_create_dir (temp_dir)) {
			/* call to finish references */
			turbulence_child_unref (result);

			error ("Unable to create directory to hold child socket connection, unable to create child process..");
			axl_free (temp_dir);
			return NULL;
		} /* end if */
	} /* end if */
	/* free temporal directory */
	axl_free (temp_dir);
	
	/* set profile path and context */
	result->ppath = def;
	result->ctx   = ctx;

	/* create listener connection used for child management */
	result->conn_mgr = vortex_listener_new_full (ctx->vortex_ctx, "0.0.0.0", "0", NULL, NULL);
	if (! vortex_connection_is_ok (result->conn_mgr, axl_false)) {
		error ("Failed to connection child connection management, unable to create child process");

		/* shutdown connection to be child by the child */
		vortex_connection_close (result->conn_mgr);
		axl_free (result);

		return NULL;
	}

	msg ("PARENT: created master<->child BEEP link running at %s:%s (socket-session=%d)",
	     vortex_connection_get_host_ip (result->conn_mgr), vortex_connection_get_port (result->conn_mgr),
	     vortex_connection_get_socket (result->conn_mgr));

	/* flag this listener as master<->child link */
	vortex_connection_set_data (result->conn_mgr, "tbc:mc-link", result);

	/* unregister conn mgr */
	turbulence_conn_mgr_unregister (ctx, result->conn_mgr);
	
	/* set default reference counting */
	result->ref_count = 1;
	vortex_mutex_create (&result->mutex);

	return result;
}

/** 
 * @brief Allows to a acquire a reference to the provided object.
 *
 * @param child The child object to get a reference.
 *
 * @return axl_true if the reference was acquired otherwise, axl_false
 * is returned.
 */
axl_bool              turbulence_child_ref (TurbulenceChild * child)
{
	if (child == NULL || child->ref_count == 0)
		return axl_false;

	/* get mutex */
	vortex_mutex_lock (&child->mutex);

	child->ref_count++;

	/* release */
	vortex_mutex_unlock (&child->mutex);

	return axl_true;
}


/** 
 * @brief Allows to decrease reference counting and deallocating
 * resources when reference counting reaches 0.
 *
 * @param child A reference to the child to decrease reference counting.
 */
void              turbulence_child_unref (TurbulenceChild * child)
{
	TurbulenceCtx * ctx;

	if (child == NULL || child->ref_count == 0)
		return;

	/* get mutex */
	vortex_mutex_lock (&child->mutex);

	child->ref_count--;

	if (child->ref_count != 0) {
		/* release */
		vortex_mutex_unlock (&child->mutex);
		return;
	}

	/* get reference to the context */
	ctx = child->ctx;

#if defined(AXL_OS_UNIX)
	/* unlink (child->socket_control_path);*/
	axl_free (child->socket_control_path);
	child->socket_control_path = NULL;
	vortex_close_socket (child->child_connection);
	axl_freev (child->init_string_items);
#endif

	/* finish child connection */
	msg ("PARENT: Finishing child connection manager id=%d (refs: %d, role %d)", 
	     vortex_connection_get_id (child->conn_mgr), 
	     vortex_connection_ref_count (child->conn_mgr), 
	     vortex_connection_get_role (child->conn_mgr));

	/* release reference if it is either initiator or listener */
	if (vortex_connection_get_role (child->conn_mgr) == VortexRoleListener) {
		vortex_connection_shutdown (child->conn_mgr);
		vortex_connection_unref (child->conn_mgr, "free data"); 
	} /* end if */

	/* finish child conn loop */
	turbulence_loop_close (child->child_conn_loop, axl_true);

	/* nullify */
	child->conn_mgr = NULL;

	/* release server name */
	axl_free (child->serverName);

	/* destroy mutex */
	vortex_mutex_destroy (&child->mutex);

	axl_free (child);

	return;
}

/** 
 * @brief Allows to get the serverName string included in the profile
 * path configuration that triggered this child.
 *
 * Every child process created by turbulence is because there is a
 * profile path configuration that instructs it. In this context, this
 * function allows to get the serverName value of the provided profile
 * path configuration that triggered the creation of this child. This
 * function is useful to get a reference to the serverName
 * configuration that triggered this child to allow additional
 * configurations that may be necessary during child initialization or
 * for its later processing.
 *
 * @param ctx A reference to the turbulence context that is running a
 * child.
 *
 * @return A reference to the serverName expression configured in the
 * profile path or NULL if it fails. 
 */
const char      * turbulence_child_get_serverName (TurbulenceCtx * ctx)
{
	if (ctx == NULL || ctx->child == NULL)
		return NULL;
	return ctx->child->serverName;
}

#define TURBULENCE_CHILD_CONF_LOG(logw,logr,level) do {			\
	log_descriptor = atoi (logw);                                   \
	if (log_descriptor > 0) {                                       \
		turbulence_log_configure (ctx, level, log_descriptor);  \
		vortex_close_socket (atoi (logr));                      \
	}                                                               \
} while(0)

axl_bool __turbulence_child_post_init_openlogs (TurbulenceCtx  * ctx, 
						char          ** items)
{
	int log_descriptor;

	if (ctx->use_syslog) {
		openlog ("turbulence", LOG_PID, LOG_DAEMON);
		return axl_true;
	} /* end if */

	/* configure general_log */
	TURBULENCE_CHILD_CONF_LOG (items[2], items[1], LOG_REPORT_GENERAL);

	/* configure error_log */
	TURBULENCE_CHILD_CONF_LOG (items[4], items[3], LOG_REPORT_ERROR);

	/* configure access_log */
	TURBULENCE_CHILD_CONF_LOG (items[6], items[5], LOG_REPORT_ACCESS);

	/* configure vortex_log */
	TURBULENCE_CHILD_CONF_LOG (items[8], items[7], LOG_REPORT_VORTEX);

	return axl_true;
}

/** 
 * @internal Function used to recover child status from the provided
 * init string.
 */
axl_bool          turbulence_child_build_from_init_string (TurbulenceCtx * ctx, 
							   const char    * socket_control_path)
{
	TurbulenceChild   * child;
	char                child_init_string[4096];
	char                child_init_length[30];
	int                 iterator;
	int                 size;
	axl_bool            found;
	char             ** items;

	/* create empty child object */
	child = axl_new (TurbulenceChild, 1);

	/* set socket control path */
	child->socket_control_path = axl_strdup (socket_control_path);

	/* connect to the socket control path */
	if (! __turbulence_process_create_parent_connection (child)) {
		error ("CHILD: unable to create parent control connection, finishing child");
		return axl_false;
	}

	/* get the amount of bytes to read from parent */
	iterator = 0;
	found    = axl_false;
	while (iterator < 30 && recv (child->child_connection, child_init_length + iterator, 1, 0) == 1) {
		if (child_init_length[iterator] == '\n') {
			found = axl_true;
			break;
		} /* end if */
		
		/* next position */
		iterator++;
	}

	if (! found) {
		error ("CHILD: expected to find init child string length indication termination (\\n) but not found, finishing child");
		return axl_false;
	}
	child_init_length[iterator + 1] = 0;
	size = atoi (child_init_length);
	msg ("CHILD: init child string length is: %d", size);

	if (size > 4095) {
		error ("CHILD: unable to process child init string, found length indicator bigger than 4095");
		return axl_false;
	}

	/* now receive child init string */
	size = recv (child->child_connection, child_init_string, size, 0);
	if (size > 0)
		child_init_string[size] = 0;
	else {
		error ("CHILD: received empty or wrong init child string from parent, finishing child");
		return axl_false;
	}

	msg ("CHILD: starting child with init string: %s", child_init_string);

	/* build items */
	child->init_string_items = axl_split (child_init_string, 1, ";_;");
	if (child->init_string_items == NULL)
		return axl_false;

	/* set child on context */
	ctx->child = child;
	child->ctx = ctx;

	/* get a reference to the serverName this child represents */
	items = axl_split (child->init_string_items[11], 1, ";-;");
	if (items == NULL)
		return axl_false;
	child->serverName = axl_strdup (items[5]);
	axl_freev (items);
	msg ("CHILD: profile path serverName for this child: %s", child->serverName);

	/* set default reference counting */
	child->ref_count = 1;
	vortex_mutex_create (&child->mutex);	

	/* open logs */
	if (! __turbulence_child_post_init_openlogs (ctx, child->init_string_items)) 
		return axl_false;

	/* return child structure properly recovered */
	return axl_true;
}

axl_bool __turbulence_child_post_init_register_conn (TurbulenceCtx * ctx, const char * conn_socket, char * conn_status)
{
	VortexConnection * conn;

	msg ("CHILD: restoring connection to be handled at child, socket: %s, connection status: %s", conn_socket, conn_status);
	if (! (conn = __turbulence_process_handle_connection_received (ctx, ctx->child->ppath, atoi (conn_socket), conn_status + 1))) 
		return axl_false;

	/* drop a log in case of success */
	if (conn) {
		msg ("CHILD: child starting conn-id=%d (socket: %d, ref: %p) registered..", 
		     vortex_connection_get_id (conn), vortex_connection_get_socket (conn), conn);
	} /* end if */

	return axl_true;
} 

/** 
 * @internal Function used to complete child startup.
 */
axl_bool          turbulence_child_post_init (TurbulenceCtx * ctx)
{
	TurbulencePPathDef  * def;
	TurbulenceChild     * child;
	int                   len;

	/*** NOTE: indexes used for child->init_string_items[X] are defined inside
	 * turbulence_process_create_child.c, around line 1392 ***/
	msg ("CHILD: doing post init (recovering first connection)");

	/* get child reference */
	child = ctx->child;

	/* define profile path */
	msg ("Setting profile path id for child %s", child->init_string_items[10] ? child->init_string_items[10] : "<not defined>");
	def = turbulence_ppath_find_by_id (ctx, atoi (child->init_string_items[10]));
	if (def == NULL) {
		error ("Unable to find profile path associated to id %d, unable to complete post init", atoi (child->init_string_items[10]));
		return axl_false;
	}
	msg ("  Set profile path: '%s'", turbulence_ppath_get_name (def));
	ctx->child->ppath = def;

	/* check here to change root path, in the case it is defined
	 * now we still have priviledges */
	turbulence_ppath_change_root (ctx, def);

	/* check here for setuid support */
	turbulence_ppath_change_user_id (ctx, def);

	/* create loop to watch child->child_connection */
	child->child_conn_loop = turbulence_loop_create (ctx);
	turbulence_loop_watch_descriptor (child->child_conn_loop, child->child_connection, 
					  turbulence_process_parent_notify, child, NULL);
	msg ("CHILD: started socket watch on (child_connection socket: %d)", child->child_connection);

	/* open connection management (child->conn_mgr) */
	msg ("CHILD: starting child<->master BEEP link on %s:%s (timeout 10 seconds)", child->init_string_items[12], child->init_string_items[13]);
	vortex_connection_connect_timeout (ctx->vortex_ctx, 10000000);
	child->conn_mgr = vortex_connection_new (ctx->vortex_ctx, 
						 /* host */
						 child->init_string_items[12], 
						 /* port */
						 child->init_string_items[13],
						 NULL, NULL);

	if (! vortex_connection_is_ok (child->conn_mgr, axl_false)) {
		error ("CHILD: failed to create master<->child BEEP link, attempted to connect to %s:%s, error reported %s (code: %d)..",
		       child->init_string_items[12], child->init_string_items[13],
		       vortex_connection_get_message (child->conn_mgr),
		       vortex_connection_get_status (child->conn_mgr));
		return axl_false;
	} else {
		/* connection ok, now unregister */
		turbulence_conn_mgr_unregister (ctx, child->conn_mgr);
	}
	msg ("CHILD: child<->master BEEP link started..OK");

	/* check if we have to restore the connection or skip this
	 * step */
	len = strlen (child->init_string_items[11]);
	msg ("CHILD: checking to skip connection store, conn_status=%s, last='%c'", 
	     child->init_string_items[11], child->init_string_items[11][len - 1]);
	if (child->init_string_items[11][len - 1] == '1') {
		msg ("CHILD: skiping connection restoring as indicated in conn_status (last position == '1'): %s", child->init_string_items[11]);
	} else {
		/* register connection handled now by child  */
		msg ("CHILD: restoring connection conn_socket=%s, conn_status=%s", child->init_string_items[0], child->init_string_items[11]);
		if (! __turbulence_child_post_init_register_conn (ctx, /* conn_socket */ child->init_string_items[0],
								  /* conn_status */ child->init_string_items[11])) {
			error ("CHILD: failed to register starting connection at child process, finishing..");
			return axl_false;
		} /* end if */
	} /* end if */
	msg ("CHILD: post init phase done, child running (vortex.ctx refs: %d)", vortex_ctx_ref_count (child->ctx->vortex_ctx));
	return axl_true;
}

