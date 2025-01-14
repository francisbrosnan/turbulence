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
#ifndef __TURBULENCE_CONFIG_H__
#define __TURBULENCE_CONFIG_H__

#include <turbulence.h>

axl_bool        turbulence_config_load     (TurbulenceCtx * ctx, 
					    const char    * config);

void            turbulence_config_cleanup  (TurbulenceCtx * ctx);

axlDoc        * turbulence_config_get      (TurbulenceCtx * ctx);

axl_bool        turbulence_config_set      (TurbulenceCtx * ctx,
					    const char    * path,
					    const char    * attr_name,
					    const char    * attr_value);

/** 
 * @brief Allows to get the node associated on a particular path on
 * inside turbulence configuration.
 *
 * @param path The path that will be used to search the node inside
 * the turbulence configuration file (turbulence.conf).
 *
 * @return A reference to the node pointing to the path or NULL if
 * nothing is found.
 *
 * <b>Examples:</b>
 *
 * To know if a particular variable is enabled at the turbulence
 * configuration file use something like:
 * \code
 * value = turbulence_config_is_attr_positive (ctx, TBC_CONFIG_PATH ("/turbulence/global-settings/close-conn-on-start-failure"), "value");
 * \endcode
 */
#define TBC_CONFIG_PATH(path) axl_doc_get (turbulence_config_get (ctx), path)

axl_bool        turbulence_config_is_attr_positive (TurbulenceCtx * ctx,
						    axlNode       * node,
						    const char    * attr_name);

axl_bool        turbulence_config_is_attr_negative (TurbulenceCtx * ctx,
						    axlNode       * node,
						    const char    * attr_name);

int             turbulence_config_get_number (TurbulenceCtx * ctx, 
					      const char    * path,
					      const char    * attr_name);

#endif
