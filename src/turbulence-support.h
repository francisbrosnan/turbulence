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
#ifndef __TURBULENCE_SUPPORT_H__
#define __TURBULENCE_SUPPORT_H__

#include <turbulence.h>

char          * turbulence_support_get_backtrace (TurbulenceCtx * ctx, int pid);

axl_bool        turbulence_support_smtp_send (TurbulenceCtx * ctx, 
					      const char    * mail_from,
					      const char    * mail_to,
					      const char    * subject,
					      const char    * body,
					      const char    * body_file,
					      const char    * smtp_server,
					      const char    * smtp_port);

axl_bool        turbulence_support_simple_smtp_send (TurbulenceCtx * ctx,
						     const char    * smtp_conf_id,
						     const char    * subject,
						     const char    * body,
						     const char    * body_file);
					      

#endif
