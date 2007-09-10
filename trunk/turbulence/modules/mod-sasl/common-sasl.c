/*
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
 *         info@aspl.es - http://www.turbulence.ws
 */
#include <common-sasl.h>

/** 
 * @internal Type to represent the set of backends that we support to
 * store users databases. 
 */
typedef enum {
	SASL_BACKEND_XML = 1
} SaslBackEndType;

/** 
 * @internal Type to represent the kind of encoding used to store
 * password in the database.
 */
typedef enum {
	SASL_STORAGE_FORMAT_PLAIN,
	SASL_STORAGE_FORMAT_MD5,
	SASL_STORAGE_FORMAT_SHA1
} SaslStorageFormat;

/** 
 * @internal Type used to represent one connected users
 * database. mod-sasl allows to configure several users databases that
 * are configured for each serverName allowed by the application.
 */
struct _SaslAuthDb {
	/** 
	 * @internal Type of the backend.
	 */
	SaslBackEndType     type;
	/** 
	 * @internal Kind of format used to store data.
	 */
	SaslStorageFormat   format;
	/** 
	 * @internal If the remote administration protocol can be
	 * used.
	 */
	bool                remote_admin;
	/** 
	 * @internal Allowed users to use the remote protocol. 
	 */
	TurbulenceDbList  * allowed_admins;
	/** 
	 * @internal serverName under which the database can be
	 * used. This value can be empty, in such case, it is used as
	 * default.
	 */
	char              * serverName;
	/** 
	 * @internal Reference to the db path, that is, the location
	 * in the file disk.
	 */
	char              * db_path;
	/** 
	 * @internal Reference to the database loaded.
	 */
	axlPointer          db;

	/** 
	 * @brief Time record the last modification for the document.
	 */
	long int            db_time;
	
};

/** 
 * @internal Structure used to store all information about databases
 * used to authenticate users. The structure contains all databases
 * indexed by serverName. 
 *
 * This serverName can be used to separate users databases for
 * different applications. 
 * 
 * For example, you can use the serverName "users.turbulence.ws" to
 * identify the set of profiles to be provided for general users and a
 * particular SASL database to authenticate them. 
 * 
 * At the same time you can use "admin.turbulence.ws" to provide a
 * different SASL database with remote administration support to allow
 * a small set of administrators to manage users.
 *
 */
struct _SaslAuthBackend {
	/** 
	 * @brief Reference to the sasl xml conf. The document itself
	 * loaded.
	 */
	axlDoc           * sasl_xml_conf;

	/** 
	 * @brief Reference to the default database to be used if no
	 * serverName configuration is found. It is only allowed to
	 * have only one default database. 
	 */
	SaslAuthDb       * default_db;
	
	/** 
	 * @brief serverName indexes hash with all databases found.
	 */
	axlHash          * dbs;

	/** 
	 * @brief Reference to the list of root managers, that is, the
	 * set of users that are allowed to manage every domain.
	 */
	TurbulenceDbList * rootManagers;

	/** 
	 * @brief Path to the sasl conf file opened.
	 */
	char             * sasl_conf_path;
};

/** 
 * @internal Function used to deallocate a sasl auth db, that is, a
 * particular connected database.
 * 
 * @param db A reference to the database to dealloc.
 */
void common_sasl_db_free (SaslAuthDb * db)
{
	if (db == NULL)
		return;

	/* close allowed domains */
	turbulence_db_list_close (db->allowed_admins);
	axl_free (db->serverName);
	axl_free (db->db_path);

	/* free users backend */
	if (db->type == SASL_BACKEND_XML)
		axl_doc_free ((axlDoc*) db->db);
	
	/* free the node itself */
	axl_free (db);

	return;
}


void common_sasl_free (SaslAuthBackend * backend)
{
	if (backend == NULL)
		return;

	/* release the path */
	axl_free            (backend->sasl_conf_path);
	axl_doc_free        (backend->sasl_xml_conf);
	axl_hash_free       (backend->dbs);
	common_sasl_db_free (backend->default_db);
	axl_free            (backend);

	return;
}

/** 
 * @brief Public mod-sasl APi that allows to load sasl backend from
 * the default file or the one located using alt_location. The
 * function return on success a reference to the sasl backend loaded,
 * with all databases associated. 
 *
 * @param sasl_backend Caller reference to the sasl_backend to be loaded.
 * 
 * @param alt_location An optional value that allows to configure
 * which is the location of the sasl backend.
 *
 * @param mutex Optional mutex variable used to lock the
 * implementation to avoid race conditions between threads.
 */
bool common_sasl_load_config (SaslAuthBackend ** sasl_backend,
			      const char       * alt_location,
			      VortexMutex      * mutex)
{
	axlNode         * node;
	axlError        * err;
	SaslAuthBackend * result;


	/* nullify before doing anyting */
	if (sasl_backend)
		*sasl_backend = NULL;

	/* create the sasl back end node */
	result      = axl_new (SaslAuthBackend, 1);
	result->dbs = axl_hash_new (axl_hash_string, axl_hash_equal_string);

	/* check alternative location */
	if (alt_location == NULL) {
		/* configure lookup domain for mod sasl settings */
		vortex_support_add_domain_search_path_ref (axl_strdup ("sasl"), 
							   vortex_support_build_filename (SYSCONFDIR, "turbulence", "sasl", NULL));
		
		/* find and load the file */
		result->sasl_conf_path  = vortex_support_domain_find_data_file ("sasl", "sasl.conf");
	} else {
		/* us the alternative location to load the document */
		result->sasl_conf_path  = axl_strdup (alt_location);
		
	} /* end if */

	/* load the document */
	result->sasl_xml_conf   = axl_doc_parse_from_file (result->sasl_conf_path, &err);
	
	/* check result */
	if (result->sasl_xml_conf == NULL) {
		/* release sasl_backend */
		common_sasl_free (result);

		error ("failed to init the SASL profile, unable to find configuration file, error: %s",
		       axl_error_get (err));
		axl_error_free (err);
		return false;
	} /* end if */

	/* now load all users dbs */
	node                = axl_doc_get (result->sasl_xml_conf, "/mod-sasl/auth-db");
	while (node != NULL) {
		if (HAS_ATTR_VALUE (node, "type", "xml") &&
		    HAS_ATTR (node, "location")) {

			/* call to load the database in xml format */
			if (! common_sasl_load_auth_db_xml (result, node, mutex)) {
				/* failed to load some database */
				common_sasl_free (result);
				return false;
			} /* end if */
			
		} else {
			/* add here other formats ... */
			
		} /* end if */

		/* get the next database */
		node = axl_node_get_next_called (node, "auth-db");

	} /* end while */

	/* set the backend loaded to the caller */
	if (sasl_backend)
		*sasl_backend = result;
	else {
		/* weird case where the programmer didn't provide a
		 * reference to the resulting object */
		common_sasl_free (result);
		return false;
	} /* end if */

	return true;
}

/** 
 * @internal Function that performs the loading of the auth-db in xml
 * format.
 * 
 * @param sasl_backend The global sasl backend reference.
 *
 * @param node The reference to the <auth-db> node found in the
 * sasl.conf file.
 * 
 * @return true if the auth-db was properly loaded, otherwise false is
 * returned.
 */
bool common_sasl_load_auth_db_xml (SaslAuthBackend * sasl_backend,
				   axlNode         * node,
				   VortexMutex     * mutex)
{
	SaslAuthDb * db;
	axlDoc     * doc  = NULL;
	axlError   * err  = NULL;

	/* create one db */
	db           = axl_new (SaslAuthDb, 1);
	db->type     = SASL_BACKEND_XML;
	
	/* find the file */
	db->db_path  = vortex_support_domain_find_data_file ("sasl", ATTR_VALUE (node, "location"));
	
	/* load db in xml format */
	if (! common_sasl_load_users_db (db, mutex)) {
		/* failed to load database */
		wrn ("Unable to load database from: %s, this database won't be usable", 
		     db->db_path);
		
		/* free memory allocated */
		axl_free (db->db_path);
		axl_free (db);
	} else {
		msg ("sasl auth db: %s", db->db_path);
		db->db = doc;

		/* configure the rest of parameters */
		db->remote_admin = HAS_ATTR_VALUE (node, "remote", "yes");
		if (HAS_ATTR_VALUE (node, "format", "sha-1"))
			db->format = SASL_STORAGE_FORMAT_SHA1;
		else if (HAS_ATTR_VALUE (node, "format", "md5"))
			db->format = SASL_STORAGE_FORMAT_MD5;
		else if (HAS_ATTR_VALUE (node, "format", "plain"))
			db->format = SASL_STORAGE_FORMAT_PLAIN;
		else {
			wrn ("using as default storage format: md5");
			db->format = SASL_STORAGE_FORMAT_MD5;
		} /* end if */
		
		/* check remote admins */
		if (HAS_ATTR (node, "remote-admins") && strlen (ATTR_VALUE (node, "remote-admins")) > 0) {
			/* flag as activated */
			db->remote_admin = true;
			
			/* load the turbulence db list */
			msg ("found remote admins, loading dblist: '%s'", ATTR_VALUE (node, "remote-admins"));
			if (turbulence_file_is_fullpath (ATTR_VALUE (node, "remote-admins"))) {
				/* full path, try to load */
				db->allowed_admins = turbulence_db_list_open (&err, ATTR_VALUE (node, "remote-admins"), NULL);
			} else {
				/* relative, try to load */
				db->allowed_admins = turbulence_db_list_open (&err, 
									      SYSCONFDIR,
									      "turbulence",
									      "sasl",
									      ATTR_VALUE (node, "remote-admins"), NULL);
			} /* end if */
			
			if (db->allowed_admins == NULL) {
				error ("unable to load the list of allowed admins, this will disable the remote administration. Error found: %s",
				       axl_error_get (err));
				axl_error_free (err);
				
				/* flag as activated */
				db->remote_admin = false;
			} /* end if */
		}
		
		/* get the serverName value */
		if (HAS_ATTR (node, "serverName") && strlen (ATTR_VALUE (node, "serverName")) > 0) {
			/* check if there are other database
			 * added for the same serverName */
			if (axl_hash_exists (sasl_backend->dbs, (axlPointer) ATTR_VALUE (node, "serverName"))) {
				error ("found a serverName database associated to the current database (serverName configured twice!)");
				common_sasl_db_free (db);
				return false;
			}
			
			/* fine, add to the database loaded hash */
			axl_hash_insert_full (sasl_backend->dbs, 
					      /* the key to store and its destroy function */
					      axl_strdup (ATTR_VALUE (node, "serverName")), axl_free,
					      /* the value to store and its destroy function */
					      db, (axlDestroyFunc) common_sasl_db_free);
			
		} else {
			/* no serverName found, it seems this
			 * is the default database to be used,
			 * check if there is already configure
			 * a default database */
			if (sasl_backend->default_db != NULL) {
				error ("it was found several default users databases (serverName empty or not found)");
				common_sasl_db_free (db);
				return false;
			} /* end if */
			
			/* configure as the default one */
			sasl_backend->default_db = db;
		} /* end if */
	} /* end if (type == xml) */

	return true;
}

/** 
 * @internal Function that supports the authentication of SASL users
 * provided a SaslAuthDb using xml format.
 * 
 * @param db The reference to the xml database.
 * @param auth_id The user to authenticate.
 * @param authorization_id The authorization to use.
 * @param password The password to be used.
 * 
 * @return true if the authentication was successful, otherwise false
 * is returned.
 */
bool common_sasl_auth_db_xml (SaslAuthDb * db, 
			      const char * auth_id, 
			      const char * authorization_id, 
			      const char * password)
{
	axlNode     * node;
	axlDoc      * doc;
	const char  * user_id;
	const char  * db_password;

	/* look up for the user and its password */
	doc  = (axlDoc *) db->db;
	node = axl_doc_get (doc, "/sasl-auth-db/auth");
	while (node != NULL) {
		
		/* get user id to check */
		user_id = ATTR_VALUE (node, "user_id");
		
		/* check the user id */
		if (axl_cmp (auth_id, user_id)) {

			/* user found, check if the account is
			 * disabled */
			if (HAS_ATTR_VALUE (node, "disabled", "yes")) {
				error ("trying to auth an account disabled: %s", auth_id);
				return false;
			}
			
			/* user id found, check password */
			db_password = ATTR_VALUE (node, "password");
			
			/* return if both passwords
			 * are equal */
			if (axl_cmp (password, db_password)) {
				return true;
			} /* end if */
			
		} /* end if */
			
		/* get next node */
		node = axl_node_get_next (node);
	} /* end if */

	return false;
}

/** 
 * @brief Public function used by mod-sasl and its tools to perform
 * user validation against the current backend loaded.
 *
 * The function will try to find a database matching the provided
 * serverName or use the default one either because no database
 * associated to the serverName was found or because no serverName
 * value was provided.
 *
 * The rest of values are required for the authentication itself, that
 * is, auth id and passwords are the usual values to perform the
 * login/password authetication. For those SASL protocols that support
 * authorization id (acting on behalf of) it is also required the
 * authorization_id.
 * 
 * @param sasl_backend The sasl backend where the auth operation will
 * be performed.
 *
 * @param auth_id The auth id that is being required to authenticate
 * (the user login).
 *
 * @param authorization_id For protocols that support auth id proxy.
 *
 * @param password The password.
 *
 * @param serverName The server name under which the SASL
 * authentication must take place.
 *
 * @param mutex An optional mutex used by the library to lock the
 * database while operating.
 * 
 * @return true if the user was authenticated, otherwise false is
 * returned.
 */
bool common_sasl_auth_user        (SaslAuthBackend  * sasl_backend,
				   const char       * auth_id,
				   const char       * authorization_id,
				   const char       * password,
				   const char       * serverName,
				   VortexMutex      * mutex)
{
	SaslAuthDb * db      = NULL;
	bool         release = false;
	bool         result  = false;

	/* no backend, no authentication */
	if (sasl_backend == NULL) {
		error ("no sasl backend was provided, unable to perform SASL authentication.");
		return false;
	}

	/* check serverName authentication */
	if (serverName != NULL) {
		/* try to find the associated database */
		db = axl_hash_get (sasl_backend->dbs, (axlPointer) serverName);
	} /* end if */

	/* if no database found, use the default one */
	if (db == NULL)
		db = sasl_backend->default_db;

	/* check database */
	if (db == NULL) {
		error ("no sasl <auth-db> was found for the provided serverName or no default <auth-db> was found, unable to perform SASL authentication.");
		return false;
	} /* end if */

	/* now we have the database, check the user and password */
	/* prepare key and password to be looked up */
	switch (db->format) {
	case SASL_STORAGE_FORMAT_MD5:
		/* redifine values */
		password = vortex_tls_get_digest (VORTEX_MD5, password);
		release  = true;
		break;
	case SASL_STORAGE_FORMAT_SHA1:
		/* redifine values */
		password = vortex_tls_get_digest (VORTEX_SHA1, password);
		release  = true;
		break;
	case SASL_STORAGE_FORMAT_PLAIN:
		/* plain do not require additional format */
		release  = false;
		break;
	default:
		/* error, unable to find the proper keying material
		 * encoding configuration */
		error ("unable to find the proper format for keying material (inside sasl.conf)");
		return false;
	} /* end switch */

	/* now, according to the database backend, call to the proper
	 * function */
	switch (db->type) {
	case SASL_BACKEND_XML:
		/* get result */
		result = common_sasl_auth_db_xml (db, auth_id, authorization_id, password);
		break;
	default:
		/* no support db format found */
		break;
	} /* end switch */

	/* check to release memory allocated */
	if (release)
		axl_free ((char*) password);

	/* return auth operation */
	return result;
}

/** 
 * @brief Allows to check if the provided sasl method is allowed. Only
 * method that is currently supported is "plain".
 * 
 * @param sasl_backend The sasl backend where to check for sasl
 * methods.
 *
 * @param sasl_method The sasl method to be checked to be
 * activated. Currently only "plain" is accepted.
 * 
 * @return true if the SASL method requested is supported.
 */
bool common_sasl_method_allowed   (SaslAuthBackend  * sasl_backend,
				   const char       * sasl_method)
{
	axlNode * node;

	/* check receiving data */
	if (sasl_backend == NULL || sasl_method == NULL)
		return false;

	node = axl_doc_get (sasl_backend->sasl_xml_conf, "/mod-sasl/method-allowed/method");
	while (node != NULL) {

		/* check for plain profile */
		if (HAS_ATTR_VALUE (node, "value", "plain")) {
			/* accept plain profile */
			return true;
		} /* end if */
		
		/* get the next node */
		node = axl_node_get_next (node);

	} /* end if */

	return false;
}

/** 
 * @brief Allows to check if the provided user already exists in the
 * database associated to the provided serverName.
 * 
 * @param sasl_backend The SASL backend where the search operation
 * will be performed.
 *
 * @param auth_id The user id to check.
 *
 * @param serverName The server name to configure the user database
 * where to lookup. If the value provided is null, the default
 * database will be used.
 *
 * @param mutex An optional mutex used by the library to lock the
 * database while operating.
 * 
 * @return true if the user already exists, otherwise false is
 * returned.
 */
bool common_sasl_user_exists      (SaslAuthBackend  * sasl_backend,
				   const char       * auth_id,
				   const char       * serverName,
				   VortexMutex      * mutex)
{
	axlNode    * node;
	const char * user_id;
	SaslAuthDb * db;

	/* return if minimum parameters aren't found. */
	if (sasl_backend == NULL ||
	    auth_id      == NULL)
		return false;

	/* get the appropiate database */
	if (serverName == NULL)
		db = sasl_backend->default_db;
	else {
		db = axl_hash_get (sasl_backend->dbs, (axlPointer) serverName);
	}
	
	/* check if the database was found */
	if (db == NULL)
		return false;

	/* according to the database backend, do */
	if (db->type == SASL_BACKEND_XML) {
		node     = axl_doc_get ((axlDoc*) db->db, "/sasl-auth-db/auth");
		while (node != NULL) {
			
			/* get the user */
			user_id = ATTR_VALUE (node, "user_id");
			
			/* check the user id */
			if (axl_cmp (auth_id, user_id)) {
				return true;
			} /* end if */
			
			/* get next node */
			node     = axl_node_get_next (node);

		} /* end while */
	} /* end if */

	return false;
}

/** 
 * @brief Allows to add the provided user under the database
 * associated to the provided serverName. 
 *
 * If the provided serverName is null, the user will be added in the
 * default user database.
 * 
 * @param sasl_backend The sasl database backend to be used to store
 * the user.
 *
 * @param auth_id The SASL user id to be used.
 *
 * @param password The password associated.
 *
 * @param serverName The server name used to identify the proper user
 * database.
 *
 * @param mutex An optional mutex used by the library to lock the
 * database while operating.
 * 
 * @return true if the user was added, otherwise false is returned.
 */
bool common_sasl_user_add         (SaslAuthBackend  * sasl_backend, 
				   const char       * auth_id, 
				   const char       * password, 
				   const char       * serverName, 
				   VortexMutex      * mutex)
{
	axlNode    * node;
	axlNode    * newNode;
	char       * enc_password;
	bool         release;
	SaslAuthDb * db;

	/* return if minimum parameters aren't found. */
	if (sasl_backend == NULL ||
	    auth_id      == NULL ||
	    password     == NULL)
		return false;

	/* get the appropiate database */
	if (serverName == NULL)
		db = sasl_backend->default_db;
	else {
		db = axl_hash_get (sasl_backend->dbs, (axlPointer) serverName);
	}
	
	/* check if the database was found */
	if (db == NULL)
		return false;

	/* encode the password received according to the encoding
	 * configured */
	switch (db->format) {
	case SASL_STORAGE_FORMAT_MD5:
		/* redifine values */
		enc_password = vortex_tls_get_digest (VORTEX_MD5, password);
		release      = true;
		break;
	case SASL_STORAGE_FORMAT_SHA1:
		/* redifine values */
		enc_password = vortex_tls_get_digest (VORTEX_SHA1, password);
		release      = true;
		break;
	case SASL_STORAGE_FORMAT_PLAIN:
		/* plain do not require additional format */
		enc_password = (char*) password;
		release      = false;
		break;
	default:
		/* error, unable to find the proper keying material
		 * encoding configuration */
		error ("unable to find the proper format for keying material (inside sasl.conf)");
		return false;
	} /* end switch */

	/* according to the database backend, do */
	if (db->type == SASL_BACKEND_XML) {
		
		/* get the top root database node */
		node     = axl_doc_get ((axlDoc*) db->db, "/sasl-auth-db");
		if (node != NULL) {
			newNode  = axl_node_create ("auth");
	
			/* set user */
			axl_node_set_attribute (newNode, "user_id", auth_id);
			
			/* set password */
			axl_node_set_attribute (newNode, "password", enc_password);
			
			/* account enabled */
			axl_node_set_attribute (newNode, "disabled", "no");
			
			/* set the node */
			axl_node_set_child (node, newNode);
			
			/* free the password */
			if (release) 
				axl_free ((char*) enc_password);
			
			/* dump the db */
			if (! axl_doc_dump_pretty_to_file ((axlDoc *) (db->db), db->db_path, 3))
				return false;
			return true;
		} /* end if */

	} /* end if */

	if (release) 
		axl_free ((char*) enc_password);

	return false;
}

/** 
 * @brief Allows to disable the selected used in the database
 * associated to the provided serverName. If the server name value is
 * NULL the user will be disabled in the default datbase.
 * 
 * @param sasl_backend The SASL backend where the operation will be
 * performed.
 * 
 * @param auth_id The user id to disable.
 *
 * @param serverName The serverName to use to select the proper user database.
 *
 * @param mutex An optional mutex used by the library to lock the
 * database while operating.
 * 
 * @return true if the user was disable, otherwise false is returned.
 */
bool common_sasl_user_disable     (SaslAuthBackend  * sasl_backend, 
				   const char       * auth_id, 
				   const char       * serverName, 
				   VortexMutex      * mutex)
{
	axlNode    * node;
	SaslAuthDb * db;
	axlDoc     * auth_db;
	const char * user_id;

	/* return if minimum parameters aren't found. */
	if (sasl_backend == NULL ||
	    auth_id      == NULL)
		return false;

	/* get the appropiate database */
	if (serverName == NULL)
		db = sasl_backend->default_db;
	else {
		db = axl_hash_get (sasl_backend->dbs, (axlPointer) serverName);
	}
	
	/* check if the database was found */
	if (db == NULL)
		return false;

	/* according to the database backend, do */
	if (db->type == SASL_BACKEND_XML) {
		
		/* search the user */
		auth_db  = (axlDoc *) db->db;
		node     = axl_doc_get (auth_db, "/sasl-auth-db/auth");
		while (node != NULL) {
			
			/* get the user */
			user_id = ATTR_VALUE (node, "user_id");
			
			/* check the user id */
			if (axl_cmp (auth_id, user_id)) {
				
				/* user found, disable it */
				axl_node_remove_attribute (node, "disabled");
				
				/* install the new attribute */
				axl_node_set_attribute (node, "disabled", "yes");
				
				
				/* dump the db */
				if (! axl_doc_dump_pretty_to_file (auth_db, db->db_path, 3)) {
					return false;
				}
				return true;
			} /* end if */
		
			/* get next node */
			node     = axl_node_get_next (node);

		} /* end while */
	} /* end if */

	return false;
}

/** 
 * @internal Function used to dealloc resource hold by the sasl user
 * structure.
 * 
 * @param user The user to dealloc.
 */
void common_sasl_user_free (SaslUser * user)
{
	if (user == NULL)
		return;
	axl_free (user->auth_id);
	axl_free (user);
	return;
}

/** 
 * @brief Allows to get the list of users stored in the provided SASL
 * backend using the serverName as selector for the proper database.
 * 
 * @param sasl_backend The sasl backend to oper.
 *
 * @param serverName The serverName to select database.
 *
 * @param mutex An optional mutex used by the library to lock the
 * database while operating.
 * 
 * @return A reference to the list of users created. Use \ref
 * axl_list_free to terminate the list.
 */
axlList * common_sasl_get_users      (SaslAuthBackend  * sasl_backend,
				      const char       * serverName,
				      VortexMutex      * mutex)
{
	axlNode    * node;
	SaslAuthDb * db;
	axlDoc     * auth_db;
	axlList    * list = NULL;
	SaslUser   * user;

	/* return if minimum parameters aren't found. */
	if (sasl_backend == NULL)
		return false;

	/* get the appropiate database */
	if (serverName == NULL)
		db = sasl_backend->default_db;
	else {
		db = axl_hash_get (sasl_backend->dbs, (axlPointer) serverName);
	}
	
	/* check if the database was found */
	if (db == NULL)
		return NULL;

	/* according to the database backend, do */
	if (db->type == SASL_BACKEND_XML) {
		
		/* search the user */
		auth_db  = (axlDoc *) db->db;
		node     = axl_doc_get (auth_db, "/sasl-auth-db/auth");
		list     = axl_list_new (axl_list_always_return_1, (axlDestroyFunc) common_sasl_user_free);
		while (node != NULL) {

			/* create the user node */
			user    = axl_new (SaslUser, 1);
			
			/* get the user */
			user->auth_id  = axl_strdup (ATTR_VALUE (node, "user_id"));
			user->disabled = HAS_ATTR_VALUE (node, "disabled", "yes");
			
			/* store in the resulting list */
			axl_list_add (list, user); 
		
			/* get next node */
			node     = axl_node_get_next (node);
		
		} /* end while */

	} /* end if */

	/* returning the list */
	return list;
}

/** 
 * @brief Allows to remove the provided user under the database
 * selected the given serverName.
 * 
 * @param sasl_backend The sasl backend where the operation will take
 * place.
 *
 * @param auth_id The user id to remove.
 *
 * @param serverName The server name value to use to select the proper
 * user database.
 *
 * @param mutex An optional mutex used by the library to lock the
 * database while operating.
 * 
 * @return true if the user is removed, otherwise false is returned.
 */
bool      common_sasl_user_remove    (SaslAuthBackend  * sasl_backend,
				      const char       * auth_id, 
				      const char       * serverName, 
				      VortexMutex      * mutex)
{
	axlNode    * node;
	SaslAuthDb * db;
	axlDoc     * auth_db;
	const char * user_id;

	/* return if minimum parameters aren't found. */
	if (sasl_backend == NULL ||
	    auth_id      == NULL)
		return false;

	/* get the appropiate database */
	if (serverName == NULL)
		db = sasl_backend->default_db;
	else {
		db = axl_hash_get (sasl_backend->dbs, (axlPointer) serverName);
	}
	
	/* check if the database was found */
	if (db == NULL)
		return false;

	/* according to the database backend, do */
	if (db->type == SASL_BACKEND_XML) {
		
		/* search the user */
		auth_db  = (axlDoc *) db->db;
		node     = axl_doc_get (auth_db, "/sasl-auth-db/auth");
		while (node != NULL) {
			
			/* get the user */
			user_id = ATTR_VALUE (node, "user_id");
			
			/* check the user id */
			if (axl_cmp (auth_id, user_id)) {

				/* remove the node */
				axl_node_remove (node, true);
				
				/* dump the db */
				if (! axl_doc_dump_pretty_to_file (auth_db, db->db_path, 3)) {
					return false;
				}
				return true;
			} /* end if */
		
			/* get next node */
			node     = axl_node_get_next (node);
		
		} /* end while */

	} /* end if */

	return false;
}

/** 
 * @internal Loads the xml users database into memory.
 * 
 * @return true if the db was properly loaded.
 */
bool common_sasl_load_users_db (SaslAuthDb * db, VortexMutex * mutex)
{
	axlError * error;
	
	/* check received parameter */
	if (db == NULL)
		return false;

	/* lock the mutex */
	vortex_mutex_lock (mutex);

	/* nullify the document */
	db->db = NULL;

	/* check file modification */
	if (turbulence_last_modification (db->db_path) == db->db_time) {
		
		/* lock the mutex */
		vortex_mutex_unlock (mutex);

		return true;
	} /* end if */

	msg ("loading sasl auth xml-db..");

	/* find the file to load */
	db->db       = axl_doc_parse_from_file (db->db_path, &error);
	
	/* check db opened */
	if (db->db == NULL) {
		/* unlock the mutex */
		vortex_mutex_unlock (mutex);

		error ("failed to init the SASL profile, unable to auth db, error: %s",
		       axl_error_get (error));
		axl_error_free (error);
		return false;
	} /* end if */
	
	/* get current db time */
	db->db_time = turbulence_last_modification (db->db_path);

	/* unlock the mutex */
	vortex_mutex_unlock (mutex);
	
	return true;
}


