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
#ifndef __TURBULENCE_HANDLERS_H__

/* include header */
#include <turbulence.h>

/** 
 * \defgroup turbulence_handlers Turbulence Handlers : Function handler definitions used by the API
 */

/** 
 * \addtogroup turbulence_handlers
 * @{
 */

/** 
 * @brief Handler definition for the set of functions that are able to
 * filter connections to be not broadcasted by the turbulence conn mgr
 * module.
 *
 * The function must return true to filter the connection and avoid
 * sending the message broadcasted.
 * 
 * @param conn The connection which is asked to be filtered or not.
 *
 * @param user_data User defined data associated to the filter
 * configuration.
 * 
 * @return true to filter the connection, otherwise return false.
 */
typedef int  (*TurbulenceConnMgrFilter) (VortexConnection * conn, axlPointer user_data);

/** 
 * @brief A function which is called to know if an item must be
 * removed from the turbulence-db list. This handler is used by \ref
 * turbulence_db_list_remove_by_func.
 * 
 * @param item_stored The item which is being requested to be removed or not.
 *
 * @param user_data User defined pointer passed to the function and
 * configured at \ref turbulence_db_list_remove_by_func.
 * 
 * @return true if the item must be removed, otherwise false must be
 * returned.
 */
typedef int  (*TurbulenceDbListRemoveFunc) (const char * item_stored, axlPointer user_data);

/** 
 * @brief Handler definition used by process calling to \ref
 * turbulence_signal_install which has visibility to access to the
 * right \ref TurbulenceCtx object. This handler must call to \ref
 * turbulence_signal_received in the case and call to
 * turbulence_signal_reconf in the case of SIGHUP received.
 *
 * @param signal The signal that was received.
 */
typedef void (*TurbulenceSignalHandler) (int signal);

/** 
 * @brief Handler definition used by turbulence_loop_set_read_handler
 * to notify that the descriptor is ready to be read (either because
 * it has data or because it was closed).
 *
 * @param loop The loop wher the notification was found.
 * @param ctx The Turbulence context where the loop is running.
 * @param descriptor The descriptor that is ready to be read.
 * @param ptr User defined pointer defined at \ref turbulence_loop_set_read_handler and passed to this handler.
 * @param ptr2 User defined pointer defined at \ref turbulence_loop_set_read_handler and passed to this handler.
 *
 * @return The function return axl_true in the case the read operation
 * was completed without problem. Otherwise axl_false is returned
 * indicating that the turbulence loop engine should close the
 * descriptor.
 */
typedef axl_bool (*TurbulenceLoopOnRead) (TurbulenceLoop * loop, 
					  TurbulenceCtx  * ctx,
					  int              descriptor, 
					  axlPointer       ptr, 
					  axlPointer       ptr2);

#endif

/**
 * @}
 */
