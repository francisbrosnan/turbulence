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
#ifndef __TURBULENCE_CTX_PRIVATE_H__
#define __TURBULENCE_CTX_PRIVATE_H__


struct _TurbulenceCtx {
	/* Reference to the turbulence vortex context associated.
	 */
	VortexCtx          * vortex_ctx;

	/* default signal handlers */
	TurbulenceSignalHandler signal_handler;

	/* Controls if messages must be send to the console log.
	 */
	int                  console_enabled;
	int                  console_debug;
	int                  console_debug2;
	int                  console_debug3;
	int                  console_color_debug;

	/* Turbulence current pid (process identifier) */
	int                  pid;
	
	/* some variables used to terminate turbulence. */
	int                  is_existing;
	VortexMutex          exit_mutex;
	
	/* Mutex to protect the list of db list opened. */
	VortexMutex          db_list_mutex;
	
	/* List of already opened db list, used to implement automatic
	 * features such automatic closing on turbulence exit,
	 * automatic reloading.. */
	axlList            * db_list_opened;
	axlDtd             * db_list_dtd;

	/*** turbulence ppath module ***/
	TurbulencePPath    * paths;

	/*** turbulence log module ***/
	int                  general_log;
	int                  error_log;
	int                  vortex_log;
	int                  access_log;
	TurbulenceLoop     * log_manager;

	/*** turbulence config module ***/
	axlDoc             * config;

	/* turbulence loading modules module */
	axlList            * registered_modules;
	VortexMutex          registered_modules_mutex;

	/* turbulence connection manager module */
	VortexMutex          conn_mgr_mutex;
	axlHash            * conn_mgr_hash; 

	/* turbulence stored data */
	axlHash            * data;
	VortexMutex          data_mutex;

	/* turbulence run  module */
	int                  clean_start;
	/* DTd used by the turbulence-run module to validate module
	 * pointers */
	axlDtd             * module_dtd;

	/*** turbulence process module ***/
	axlList            * child_process;
	VortexMutex          child_process_mutex;

	/*** turbulence mediator module ***/
	axlHash            * mediator_hash;
	VortexMutex          mediator_hash_mutex;
};

#endif