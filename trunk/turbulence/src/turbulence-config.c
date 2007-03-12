/*  Turbulence:  BEEP application server
 *  Copyright (C) 2007 Advanced Software Production Line, S.L.
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
 *         C/ Dr. Michavila N� 14
 *         Coslada 28820 Madrid
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://fact.aspl.es
 */
#include <turbulence.h>

axlDoc * __turbulence_config = NULL;

/** 
 * Loads the turbulence main file, which has all definitions to make
 * turbulence to start.
 * 
 * @param config 
 */
void turbulence_config_load (char * config)
{
	axlError * error;
	char     * dtd;

	/* check null value */
	if (config == NULL) {
		error ("config file not defined, terminating turbulence");
		turbulence_exit (-1);
		return;
	} /* end if */

	/* load the file */
	__turbulence_config = axl_doc_parse_from_file (config, &error);
	if (__turbulence_config == NULL) {
		error ("unable to load file (%s), it seems a xml error: %s", 
		       config, axl_error_get (error));

		/* free resources */
		axl_free (config);
		axl_error_free (error);

		/* call to finish turbulence */
		turbulence_exit (-1);
		return;

	} /* end if */
	
	/* drop a message */
	msg ("file %s loaded, ok", config);

	/* free resources */
	axl_free (config);

	/* now validates the turbulence file */
	/* configure lookup domain, and load DTD file */
	vortex_support_add_domain_search_path_ref (axl_strdup ("turbulence-data"), 
						   vortex_support_build_filename (DATADIR, "turbulence", NULL));
	vortex_support_add_domain_search_path     ("turbulence-data", ".");
	dtd = vortex_support_domain_find_data_file ("turbulence-data", "config.dtd");
	if (dtd == NULL) {
		/* free document */
		axl_doc_free (__turbulence_config);
		error ("unable to find turbulence config DTD definition (config.dtd), check your turbulence installation.");
		return;
	} /* end if */
 
	/* found dtd file */
	msg ("found dtd file at: %s", dtd);

	/** FOLLOW HERE: load the dtd file and validate the
	 * configuration file found. **/

	axl_free (dtd);
	

	return;
}

