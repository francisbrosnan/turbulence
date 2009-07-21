/*  Turbulence:  BEEP application server
 *  Copyright (C) 2008 Advanced Software Production Line, S.L.
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
 *  at COPYING file. This is LGPL software: you are wellcome to
 *  develop propietary applications using this library withtout any
 *  royalty or fee but returning back any change, improvement or
 *  addition in the form of source code, project image, documentation
 *  patches, etc.
 *
 *  For comercial support on build BEEP enabled solutions, supporting
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
#if defined(ENABLE_TERMIOS)
# include <termios.h>
# include <sys/stat.h>
# include <unistd.h>
# define TBC_TERMINAL "/dev/tty"
#endif

#include <turbulence.h>

/* local include */
#include <turbulence-ctx-private.h>

#if defined(__COMPILING_TURBULENCE__) && defined(__GNUC__)
/* make happy gcc compiler */
int fsync (int fd);
#endif

/** 
 * \defgroup turbulence Turbulence: general facilities, initialization, etc
 */

/** 
 * \addtogroup turbulence
 * @{
 */

/** 
 * @brief Starts turbulence execution, initializing all libraries
 * required by the server application.
 *
 * A call to \ref turbulence_exit is required before exit.
 */
int  turbulence_init (TurbulenceCtx * ctx, 
		      VortexCtx     * vortex_ctx,
		      const char    * config)
{
	/* no initialization done if null reference received */
	if (ctx == NULL) {
	        abort_error ("Received a null turbulence context, failed to init the turbulence");
		return axl_false;
	} /* end if */
	if (config == NULL) {
		abort_error ("Received a null reference to the turbulence configuration, failed to init the turbulence");
		return axl_false;
	} /* end if */

	/* get current process id */
	ctx->pid = getpid ();

	/* init turbulence internals */
	vortex_mutex_create (&ctx->exit_mutex);

	/* if a null value is received for the vortex context, create
	 * a new empty one */
	if (vortex_ctx == NULL) {
		msg2 ("creating a new vortex context because a null value was received..");
		vortex_ctx = vortex_ctx_new ();
	} /* end if */

	/* configure the vortex context created */
	turbulence_ctx_set_vortex_ctx (ctx, vortex_ctx);

	/* configure lookup domain for turbulence data */
	vortex_support_add_domain_search_path_ref (vortex_ctx, axl_strdup ("turbulence-data"), 
						   vortex_support_build_filename (TBC_DATADIR, "turbulence", NULL));
#if defined(AXL_OS_WIN32)
	/* make turbulence to add the path ../data to the search list
	 * under windows as it is organized this way */
	vortex_support_add_domain_search_path     (vortex_ctx, "turbulence-data", TBC_DATADIR);
#endif
	vortex_support_add_domain_search_path     (vortex_ctx, "turbulence-data", ".");

	/*** init the vortex library ***/
	if (! vortex_init_ctx (vortex_ctx)) {
		abort_error ("unable to start vortex library, terminating turbulence execution..");
		return axl_false;
	} /* end if */

	/*** not required to initialize axl library, already done by vortex ***/
	msg ("turbulence internal init ctx: %p, vortex ctx: %p", ctx, vortex_ctx);

	/* db list */
	if (! turbulence_db_list_init (ctx)) {
		abort_error ("failed to init the turbulence db-list module");
		return axl_false;
	} /* end if */

	/* init turbulence-module.c */
	turbulence_module_init (ctx);

	/* load current turbulence configuration */
	if (! turbulence_config_load (ctx, config)) {
		/* unable to load configuration */
		return axl_false;
	}

	/* init profile path module */
	if (! turbulence_ppath_init (ctx)) {
		return axl_false;
	} /* end if */

	/* init connection manager */
	turbulence_conn_mgr_init (ctx);

	/* init ok */
	return axl_true;
} /* end if */

/** 
 * @internal Function that performs a reload operation for the current
 * turbulence instance.
 * 
 * @param value The signal number caught.
 */
void     turbulence_reload_config       (TurbulenceCtx * ctx, int value)
{
	/* get turbulence context */
	int             already_notified = axl_false;
	
	msg ("caught HUP signal, reloading configuration");
	/* reconfigure signal received, notify turbulence modules the
	 * signal */
	vortex_mutex_lock (&ctx->exit_mutex);
	if (already_notified) {
		vortex_mutex_unlock (&ctx->exit_mutex);
		return;
	}
	already_notified = axl_true;

	/* reload turbulence here, before modules
	 * reloading */
	turbulence_db_list_reload_module ();
	
	/* reload modules */
	turbulence_module_notify_reload_conf (ctx);
	vortex_mutex_unlock (&ctx->exit_mutex);

	return;
} 

/** 
 * @brief Performs all operations required to cleanup turbulence
 * runtime execution (calling to all module cleanups).
 */
void turbulence_exit (TurbulenceCtx * ctx, 
		      int             free_ctx,
		      int             free_vortex_ctx)
{
	VortexCtx * vortex_ctx;

	/* do not perform any change if a null context is received */
	v_return_if_fail (ctx);

	msg ("cleaning up..");

	/* terminate all modules */
	turbulence_config_cleanup (ctx);

	/* unref all connections (before calling to terminate vortex) */
	turbulence_conn_mgr_cleanup (ctx);

	/* get the vortex context assocaited */
	vortex_ctx = turbulence_ctx_get_vortex_ctx (ctx);

	/* terminate profile path */
	turbulence_ppath_cleanup (ctx);

	/* close modules before terminating vortex so they still can
	 * use the Vortex API to terminate its function. */
	turbulence_module_notify_close (ctx);

	/* terminate turbulence db list module at this point to avoid
	 * modules referring to db-list to lost references */
	turbulence_db_list_cleanup (ctx);

	/* do not release the context (this is done by the caller) */
	turbulence_log_cleanup (ctx);

	/* terminate vortex */
	msg ("now, terminate vortex library after turbulence cleanup..");
	vortex_exit_ctx (vortex_ctx, free_vortex_ctx);

	/* now modules and vortex library is stopped, terminate
	 * modules unloading them */
	turbulence_module_cleanup (ctx);

	/* terminate run module */
	turbulence_run_cleanup (ctx);

	/* free mutex */
	vortex_mutex_destroy (&ctx->exit_mutex);

	/* free ctx */
	if (free_ctx)
		turbulence_ctx_free (ctx);

	return;
}

/** 
 * @internal Simple macro to check if the console output is activated
 * or not.
 */
#define CONSOLE if (ctx->console_enabled || ignore_debug) fprintf

/** 
 * @internal Simple macro to check if the console output is activated
 * or not.
 */
#define CONSOLEV if (ctx->console_enabled || ignore_debug) vfprintf



/** 
 * @internal function that actually handles the console error.
 *
 * @param ignore_debug Allows to configure if the debug configuration
 * must be ignored (bypassed) and drop the log. This can be used to
 * perform logging for important messages.
 */
void turbulence_error (TurbulenceCtx * ctx, axl_bool ignore_debug, const char * file, int line, const char * format, ...)
{
	/* get turbulence context */
	va_list            args;
	
	/* check extended console log */
	if (ctx->console_debug3 || ignore_debug) {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stderr, "(proc:%d) [\e[1;31merr\e[0m] (%s:%d) ", ctx->pid, file, line);
		} else
#endif
			CONSOLE (stderr, "(proc:%d) [err] (%s:%d) ", ctx->pid, file, line);
	} else {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stderr, "\e[1;31mE: \e[0m");
		} else
#endif
			CONSOLE (stderr, "E: ");
	} /* end if */
	
	va_start (args, format);

	/* report to the console */
	CONSOLEV (stderr, format, args);

	va_end (args);
	va_start (args, format);

	/* report to log */
	turbulence_log_report (ctx, LOG_REPORT_ERROR, format, args, file, line);

	va_end (args);
	va_start (args, format);

	turbulence_log_report (ctx, LOG_REPORT_GENERAL, format, args, file, line);
	
	va_end (args);

	CONSOLE (stderr, "\n");
	
	fflush (stderr);
	
	return;
}

/** 
 * @brief Allows to check if the debug is activated (\ref msg type).
 * 
 * @return axl_true if activated, otherwise axl_false is returned.
 */
int  turbulence_log_enabled (TurbulenceCtx * ctx)
{
	/* get turbulence context */
	v_return_val_if_fail (ctx, axl_false);

	return ctx->console_debug;
}

/** 
 * @brief Allows to activate the turbulence console log (by default
 * disabled).
 * 
 * @param ctx The turbulence context to configure.  @param value The
 * value to configure to enable/disable console log.
 */
void turbulence_log_enable       (TurbulenceCtx * ctx, 
				  int  value)
{
	v_return_if_fail (ctx);

	/* configure the value */
	ctx->console_debug = value;

	/* update the global console activation */
	ctx->console_enabled     = ctx->console_debug || ctx->console_debug2 || ctx->console_debug3;

	return;
}




/** 
 * @brief Allows to check if the second level debug is activated (\ref
 * msg2 type).
 * 
 * @return axl_true if activated, otherwise axl_false is returned.
 */
int  turbulence_log2_enabled (TurbulenceCtx * ctx)
{
	/* get turbulence context */
	v_return_val_if_fail (ctx, axl_false);
	
	return ctx->console_debug2;
}

/** 
 * @brief Allows to activate the second level console log. This level
 * of debug automatically activates the previous one. Once activated
 * it provides more information to the console.
 * 
 * @param ctx The turbulence context to configure.
 * @param value The value to configure.
 */
void turbulence_log2_enable      (TurbulenceCtx * ctx,
				  int  value)
{
	v_return_if_fail (ctx);

	/* set the value */
	ctx->console_debug2 = value;

	/* update the global console activation */
	ctx->console_enabled     = ctx->console_debug || ctx->console_debug2 || ctx->console_debug3;

	/* makes implicit activations */
	if (ctx->console_debug2)
		ctx->console_debug = axl_true;

	return;
}

/** 
 * @brief Allows to check if the third level debug is activated (\ref
 * msg2 with additional information).
 * 
 * @return axl_true if activated, otherwise axl_false is returned.
 */
int  turbulence_log3_enabled (TurbulenceCtx * ctx)
{
	/* get turbulence context */
	v_return_val_if_fail (ctx, axl_false);

	return ctx->console_debug3;
}

/** 
 * @brief Allows to activate the third level console log. This level
 * of debug automatically activates the previous one. Once activated
 * it provides more information to the console.
 * 
 * @param ctx The turbulence context to configure.
 * @param value The value to configure.
 */
void turbulence_log3_enable      (TurbulenceCtx * ctx,
				  int  value)
{
	v_return_if_fail (ctx);

	/* set the value */
	ctx->console_debug3 = value;

	/* update the global console activation */
	ctx->console_enabled     = ctx->console_debug || ctx->console_debug2 || ctx->console_debug3;

	/* makes implicit activations */
	if (ctx->console_debug3)
		ctx->console_debug2 = axl_true;

	return;
}

/** 
 * @brief Allows to configure if the console log produced is colorfied
 * according to the status reported (red: (error,criticals), yellow:
 * (warning), green: (info, debug).
 * 
 * @param ctx The turbulence context to configure.
 *
 * @param value The value to configure. This function could take no
 * effect on system where ansi values are not available.
 */
void turbulence_color_log_enable (TurbulenceCtx * ctx,
				  int             value)
{
	v_return_if_fail (ctx);

	/* configure the value */
	ctx->console_color_debug = value;

	return;
}

/** 
 * @internal function that actually handles the console msg.
 */
void turbulence_msg (TurbulenceCtx * ctx, const char * file, int line, const char * format, ...)
{
	/* get turbulence context */
	va_list            args;
	axl_bool           ignore_debug = axl_false;

	/* check extended console log */
	if (ctx->console_debug3) {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "(proc:%d) [\e[1;32mmsg\e[0m] (%s:%d) ", ctx->pid, file, line);
		} else
#endif
			CONSOLE (stdout, "(proc:%d) [msg] (%s:%d) ", ctx->pid, file, line);
	} else {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "\e[1;32mI: \e[0m");
		} else
#endif
			CONSOLE (stdout, "I: ");
	} /* end if */
	
	va_start (args, format);
	
	/* report to console */
	CONSOLEV (stdout, format, args);

	va_end (args);
	va_start (args, format);

	/* report to log */
	turbulence_log_report (ctx, LOG_REPORT_GENERAL, format, args, file, line);

	va_end (args);

	CONSOLE (stdout, "\n");
	
	fflush (stdout);
	
	return;
}

/** 
 * @internal function that actually handles the console access
 */
void  turbulence_access   (TurbulenceCtx * ctx, const char * file, int line, const char * format, ...)
{
	/* get turbulence context */
	va_list            args;
	axl_bool           ignore_debug = axl_false;

	/* check extended console log */
	if (ctx->console_debug3) {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "(proc:%d) [\e[1;32mmsg\e[0m] (%s:%d) ", ctx->pid, file, line);
		} else
#endif
			CONSOLE (stdout, "(proc:%d) [msg] (%s:%d) ", ctx->pid, file, line);
	} else {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "\e[1;32mI: \e[0m");
		} else
#endif
			CONSOLE (stdout, "I: ");
	} /* end if */
	
	va_start (args, format);
	
	/* report to console */
	CONSOLEV (stdout, format, args);

	va_end (args);
	va_start (args, format);

	/* report to log */
	turbulence_log_report (ctx, LOG_REPORT_ACCESS, format, args, file, line);

	va_end (args);

	CONSOLE (stdout, "\n");
	
	fflush (stdout);
	
	return;
}

/** 
 * @internal function that actually handles the console msg (second level debug)
 */
void turbulence_msg2 (TurbulenceCtx * ctx, const char * file, int line, const char * format, ...)
{
	/* get turbulence context */
	va_list            args;
	axl_bool           ignore_debug = axl_false;

	/* check second level debug */
	if (! ctx->console_debug2)
		return;

	/* check extended console log */
	if (ctx->console_debug3) {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "(proc:%d) [\e[1;32mmsg\e[0m] (%s:%d) ", ctx->pid, file, line);
		} else
#endif
			CONSOLE (stdout, "(proc:%d) [msg] (%s:%d) ", ctx->pid, file, line);
	} else {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "\e[1;32mI: \e[0m");
		} else
#endif
			CONSOLE (stdout, "I: ");
	} /* end if */
	va_start (args, format);
	
	/* report to console */
	CONSOLEV (stdout, format, args);

	va_end (args);
	va_start (args, format);

	/* report to log */
	turbulence_log_report (ctx, LOG_REPORT_GENERAL, format, args, file, line);

	va_end (args);

	CONSOLE (stdout, "\n");
	
	fflush (stdout);
	
	return;
}

/** 
 * @internal function that actually handles the console wrn.
 */
void turbulence_wrn (TurbulenceCtx * ctx, const char * file, int line, const char * format, ...)
{
	/* get turbulence context */
	va_list            args;
	axl_bool           ignore_debug = axl_false;
	
	/* check extended console log */
	if (ctx->console_debug3) {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "(proc:%d) [\e[1;33m!!!\e[0m] (%s:%d) ", ctx->pid, file, line);
		} else
#endif
			CONSOLE (stdout, "(proc:%d) [!!!] (%s:%d) ", ctx->pid, file, line);
	} else {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "\e[1;33m!: \e[0m");
		} else
#endif
			CONSOLE (stdout, "!: ");
	} /* end if */
	
	va_start (args, format);

	CONSOLEV (stdout, format, args);

	va_end (args);
	va_start (args, format);

	/* report to log */
	turbulence_log_report (ctx, LOG_REPORT_GENERAL, format, args, file, line);

	va_end (args);
	va_start (args, format);

	turbulence_log_report (ctx, LOG_REPORT_ERROR, format, args, file, line);

	va_end (args);

	CONSOLE (stdout, "\n");
	
	fflush (stdout);
	
	return;
}

/** 
 * @internal function that actually handles the console wrn_sl.
 */
void turbulence_wrn_sl (TurbulenceCtx * ctx, const char * file, int line, const char * format, ...)
{
	/* get turbulence context */
	va_list            args;
	axl_bool           ignore_debug = axl_false;
	
	/* check extended console log */
	if (ctx->console_debug3) {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "(proc:%d) [\e[1;33m!!!\e[0m] (%s:%d) ", ctx->pid, file, line);
		} else
#endif
			CONSOLE (stdout, "(proc:%d) [!!!] (%s:%d) ", ctx->pid, file, line);
	} else {
#if defined(AXL_OS_UNIX)	
		if (ctx->console_color_debug) {
			CONSOLE (stdout, "\e[1;33m!: \e[0m");
		} else
#endif
			CONSOLE (stdout, "!: ");
	} /* end if */
	
	va_start (args, format);

	CONSOLEV (stdout, format, args);

	va_end (args);
	va_start (args, format);

	/* report to log */
	turbulence_log_report (ctx, LOG_REPORT_ERROR | LOG_REPORT_GENERAL, format, args, file, line);

	va_end (args);

	fflush (stdout);
	
	return;
}

/** 
 * @brief Provides the same functionality like vortex_support_file_test,
 * but allowing to provide the file path as a printf like argument.
 * 
 * @param format The path to be checked.
 * @param test The test to be performed. 
 * 
 * @return axl_true if all test returns axl_true. Otherwise axl_false is returned.
 */
int  turbulence_file_test_v (const char * format, VortexFileTest test, ...)
{
	va_list   args;
	char    * path;
	int       result;

	/* open arguments */
	va_start (args, test);

	/* get the path */
	path = axl_strdup_printfv (format, args);

	/* close args */
	va_end (args);

	/* do the test */
	result = vortex_support_file_test (path, test);
	axl_free (path);

	/* return the test */
	return result;
}


/** 
 * @brief Creates the directory with the path provided.
 * 
 * @param path The directory to create.
 * 
 * @return axl_true if the directory was created, otherwise axl_false is
 * returned.
 */
int      turbulence_create_dir  (const char * path)
{
	/* check the reference */
	if (path == NULL)
		return axl_false;
	
	/* create the directory */
#if defined(AXL_OS_WIN32)
	return (_mkdir (path) == 0);
#else
	return (mkdir (path, 0770) == 0);
#endif
}

/**
 * @brief Allows to remove the selected file pointed by the path
 * provided.
 *
 * @param path The path to the file to be removed.
 *
 * @return axl_true if the file was removed, otherwise axl_false is
 * returned.
 */ 
axl_bool turbulence_unlink              (const char * path)
{
	if (path == NULL)
		return axl_false;

	/* remove the file */
#if defined(AXL_OS_WIN32)
	return (_unlink (path) == 0);
#else
	return (unlink (path) == 0);
#endif
}

/** 
 * @brief Allows to get last modification date for the file provided.
 * 
 * @param file The file to check for its modification time.
 * 
 * @return The last modification time for the file provided. The
 * function return -1 if it fails.
 */
long int turbulence_last_modification (const char * file)
{
	struct stat status;

	/* check file received */
	if (file == NULL)
		return -1;

	/* get stat */
	if (stat (file, &status) == 0)
		return (long int) status.st_mtime;
	
	/* failed to get stats */
	return -1;
}

/** 
 * @brief Allows to check if the provided file path is a full path
 * (not releative). The function is meant to be portable.
 * 
 * @param file The file to check if it is a full path file name.
 * 
 * @return axl_true if the file is a full path, otherwise axl_false is
 * returned.
 */
int      turbulence_file_is_fullpath (const char * file)
{
	/* check the value received */
	if (file == NULL)
		return axl_false;
#if defined(AXL_OS_UNIX)
	if (file != NULL && (strlen (file) >= 1) && file[0] == '/')
		return axl_true;
#elif defined(AXL_OS_WIN32)
	if (file != NULL && (strlen (file) >= 3) && file[1] == ':' && (file[2] == '/' || file[2] == '\\'))
		return axl_true;
#endif	
	/* the file is not a full path */
	return axl_false;
}


/** 
 * @brief Allows to get the base dir associated to the provided path.
 * 
 * @param path The path that is required to return its base part.
 * 
 * @return Returns the base dir associated to the function. You
 * must deallocate the returning value with axl_free.
 */
char   * turbulence_base_dir            (const char * path)
{
	int    iterator;
	axl_return_val_if_fail (path, NULL);

	/* start with string length */
	iterator = strlen (path) - 1;

	/* lookup for the back-slash */
	while ((iterator >= 0) && 
	       ((path [iterator] != '/') && path [iterator] != '\\'))
		iterator--;

	/* check if the file provided doesn't have any base dir */
	if (iterator == -1) {
		/* return base dir for default location */
		return axl_strdup (".");
	}

	/* check the special case where the base path is / */
	if (iterator == 0 && path [0] == '/')
		iterator = 1;

	/* copy the base dir found */
	return axl_stream_strdup_n (path, iterator);
}

/** 
 * @brief Allows to get the file name associated to the provided path.
 * 
 * @param path The path that is required to return its base value.
 * 
 * @return Returns the base dir associated to the function. You msut
 * deallocate the returning value with axl_free.
 */
char   * turbulence_file_name           (const char * path)
{
	int    iterator;
	axl_return_val_if_fail (path, NULL);

	/* start with string length */
	iterator = strlen (path) - 1;

	/* lookup for the back-slash */
	while ((iterator >= 0) && ((path [iterator] != '/') && (path [iterator] != '\\')))
		iterator--;

	/* check if the file provided doesn't have any file part */
	if (iterator == -1) {
		/* return the an empty file part */
		return axl_strdup (path);
	}

	/* copy the base dir found */
	return axl_strdup (path + iterator + 1);
}

/*
 * @brief Allows to get the next line read from the user. The function
 * return an string allocated.
 * 
 * @return An string allocated or NULL if nothing was received.
 */
char * turbulence_io_get (char * prompt, TurbulenceIoFlags flags)
{
#if defined(ENABLE_TERMIOS)
	struct termios current_set;
	struct termios new_set;
	int input;
	int output;

	/* buffer declaration */
	char   buffer[1024];
	char * result = NULL;
	int    iterator;
	
	/* try to read directly from the tty */
	if ((input = output = open (TBC_TERMINAL, O_RDWR)) < 0) {
		/* if fails to open the terminal, use the standard
		 * input and standard error */
		input  = STDIN_FILENO;
		output = STDERR_FILENO;
	} /* end if */

	/* print the prompt if defined */
	if (prompt != NULL) {
		/* write the prompt */
		write (output, prompt, strlen (prompt));
		fsync (output);
	}

	/* check to disable echo */
	if (flags & DISABLE_STDIN_ECHO) {
		if (input != STDIN_FILENO && (tcgetattr (input, &current_set) == 0)) {
			/* copy to the new set */
			memcpy (&new_set, &current_set, sizeof (struct termios));
			
			/* configure new settings */
			new_set.c_lflag &= ~(ECHO | ECHONL);
			
			/* set this values to the current input */
			tcsetattr (input, TCSANOW, &new_set);
		} /* end if */
	} /* end if */

	iterator = 0;
	memset (buffer, 0, 1024);
	/* get the next character */
	while ((iterator < 1024) && (read (input, buffer + iterator, 1) == 1)) {
		
		if (buffer[iterator] == '\n') {
			/* remove trailing \n */
			buffer[iterator] = 0;
			result = axl_strdup (buffer);

			break;
		} /* end if */


		/* update the iterator */
		iterator++;
	} /* end while */

	/* return terminal settings if modified */
	if (flags & DISABLE_STDIN_ECHO) {
		if (input != STDIN_FILENO) {
			
			/* set this values to the current input */
			tcsetattr (input, TCSANOW, &current_set);

			/* close opened file descriptor */
			close (input);
		} /* end if */
	} /* end if */

	/* do not return anything from this point */
	return result;
#else
	return NULL;
#endif
}

/**
 * @brief Allows to get the SYSCONFDIR path provided at compilation
 * time. This is configured when the libturbulence.{dll,so} is built,
 * ensuring all pieces uses the same SYSCONFDIR value. See also \ref
 * turbulence_datadir.
 *
 * The SYSCONFDIR points to the base root directory where all
 * configuration is found. Under unix system it is usually:
 * <b>/etc</b>. On windows system it is usually configured to:
 * <b>../etc</b>. Starting from that directory is found the rest of
 * configurations:
 * 
 *  - etc/turbulence/turbulence.conf
 *  - etc/turbulence/sasl/sasl.conf
 */
const char    * turbulence_sysconfdir     (void)
{
	/* return current configuration */
	return SYSCONFDIR;
}

/**
 * @brief Allows to get the TBC_DATADIR path provided at compilation
 * time. This is configured when the libturbulence.{dll,so} is built,
 * ensuring all pieces uses the same SYSCONFDIR value. See also \ref
 * turbulence_sysconfdir
 *
 * The TBC_DATADIR points to the base root directory where data files
 * are located (mostly dtd files). Under unix system it is usually:
 * <b>/usr/share/turbulence</b>. On windows system it is usually configured to:
 * <b>../data</b>. Starting from that directory is found the rest of
 * configurations:
 */
const char    * turbulence_datadir        (void)
{
	/* return current configuration */
	return TBC_DATADIR;
}

/* @} */

/** 
 * \mainpage 
 *
 * \section intro Turbulence API Documentation
 *
 * The following is the API provided by Turbulence to all tools and
 * modules built on top of it. This API complements the <a class="el"
 * href="">Vortex API</a> adding features that are missing in Vortex
 * (due to its library nature). This documentation is only useful to
 * anyone that is interested in building a Turbulence module or to
 * extend Turbulence itself.
 *
 * In the case you are looking for Turbulence configuration manual check: http://www.aspl.es/turbulence/doc.html
 *
 * <h2>Vortex API </h2>
 *
 *  - <a class="el" href="http://fact.aspl.es/files/af-arch/vortex-1.1/html/index.html">Vortex Library 1.1 Documentation Center</a>
 *
 * <h2>Turbulence API</h2>
 *
 *  - \ref turbulence
 *  - \ref turbulence_moddef
 *  - \ref turbulence_config
 *  - \ref turbulence_db_list
 *  - \ref turbulence_conn_mgr
 *  - \ref turbulence_handlers
 *  - \ref turbulence_ctx
 *  - \ref turbulence_run
 *  - \ref turbulence_expr
 */

