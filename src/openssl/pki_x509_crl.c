/* PKI_X509 object management */

#include <libpki/pki.h>
#include <sys/utsname.h>

PKI_X509_CRL_REASON_CODE PKI_X509_CRL_REASON_DESCR[] = {
	{ PKI_CRL_REASON_UNSPECIFIED, 			"unspecified",		"No specified reason" },
	{ PKI_CRL_REASON_KEY_COMPROMISE, 		"keyCompromise",	"Subject's Key Compromised" },
	{ PKI_CRL_REASON_CA_COMPROMISE, 		"caCompromise",		"Certification Authority Compromised" },
	{ PKI_CRL_REASON_AFFILIATION_CHANGED, 	"affiliationChanged",	"Subject's Change in affiliation" },
	{ PKI_CRL_REASON_SUPERSEDED, 			"superseded",		"Superseded" },
	{ PKI_CRL_REASON_CESSATION_OF_OPERATION, "cessationOfOperation", "Cessation of Operation" },
	{ PKI_CRL_REASON_CERTIFICATE_HOLD, 		"certificateHold",	"Certificate is Suspended" },
	{ PKI_CRL_REASON_REMOVE_FROM_CRL, 		"removeFromCRL",	"Remove From CRL" },
	{ PKI_CRL_REASON_PRIVILEGE_WITHDRAWN, 	"privilegeWithdrawn",	"Privilege is Withdrawn" },
	{ PKI_CRL_REASON_AA_COMPROMISE, 		"aaCompromise",		"Attribute Authority Compromised" },
	{ PKI_CRL_REASON_HOLD_INSTRUCTION_REJECT, 	"onHoldReject",	"Certificate is Suspended, Reject" },
	{ PKI_CRL_REASON_HOLD_INSTRUCTION_CALLISSUER,	"onHoldCallIssuer", "Certificate is Suspended, Call Issuer" },
};


/*! 
 * \brief Returns an empty PKI_X509_CRL data structure
 */

PKI_X509_CRL *PKI_X509_CRL_new_null ( void ) {
	return PKI_X509_new ( PKI_DATATYPE_X509_CRL, NULL );
}

/*! 
 * \brief Frees memory associated with a PKI_CRL
 *
 * This function frees memory associated with a PKI_X509_CRL data structure.
 * Returns PKI_OK if successful or PKI_ERR in case of an error (or if
 * the passed pointer was NULL.
 */

int PKI_X509_CRL_free ( PKI_X509_CRL * x ) {

	if( !x ) return (PKI_ERR);

	if( x->value ) X509_CRL_free ((X509_CRL *) x->value );

	PKI_Free ( x );

	return( PKI_OK );
}

void PKI_X509_CRL_free_void( void *x ) {
	PKI_X509_free( (PKI_X509_CRL *) x );

	return;
}

/*! \brief Generate a new CRL from a stack of revoked entries
 *
 * Generates a new signed CRL from a stack of revoked entries. A profile is
 * used to set the right extensions in the CRL. To generate a new revoked
 * entry the PKI_X509_CRL_ENTRY_new() function has to be used.
 */

PKI_X509_CRL *PKI_X509_CRL_new ( PKI_X509_KEYPAIR *k, PKI_X509_CERT *cert, 
	char * crlNumber_s, unsigned long validity, PKI_X509_CRL_ENTRY_STACK *sk, 
			PKI_X509_PROFILE *profile, PKI_CONFIG *oids, HSM *hsm) {

	PKI_X509_CRL *ret = NULL;
	PKI_X509_CRL_VALUE *val = NULL;
	ASN1_INTEGER *crlNumber = NULL;
	ASN1_TIME *time = NULL;
	int rv = PKI_OK;
	int i = 0;

	char * tmp_s = NULL;

	PKI_X509_CRL_ENTRY *entry = NULL;

	PKI_DIGEST_ALG *dgst = NULL;

	long long lastUpdateVal  = 0;
	long long nextUpdateVal  = 0;

	/* Checks for the Key and its internal value */
	if( !k || !k->value ) return NULL;

	/* checks for the certificate and its internal value */
	if( !cert || !cert->value ) return ( NULL );

	if(( ret = PKI_X509_CRL_new_null()) == NULL ) {
		PKI_ERROR(PKI_ERR_OBJECT_CREATE, NULL);
		goto err;
	}

	/* Alloc memory structure for the Certificate */
	if((ret->value = ret->cb->create()) == NULL ) {
		PKI_ERROR(PKI_ERR_OBJECT_CREATE, NULL);
		goto err;
	}

	val = ret->value;

	if ( !crlNumber_s && profile ) {

		if(( tmp_s = PKI_CONFIG_get_value( profile, 
				"/profile/crlNumber")) != NULL ) {
			crlNumber = PKI_INTEGER_new_char ( tmp_s );
			PKI_Free ( tmp_s );
		};
	} else if ( crlNumber_s ) {
		crlNumber = PKI_INTEGER_new_char( crlNumber_s );

		// Let's add the CRLSerial extension
		X509_CRL_add1_ext_i2d(val, NID_crl_number, crlNumber, 0, 0);
	};

	/* Set the start date (notBefore) */
	if (profile)
	{
		int years = 0;
		int days  = 0;
		int hours = 0;
		int mins  = 0;
		int secs  = 0;

		if(( tmp_s = PKI_CONFIG_get_value( profile, 
				"/profile/notBefore/years")) != NULL ) {
			years = atoi( tmp_s );
			PKI_Free ( tmp_s );
		};

		if(( tmp_s = PKI_CONFIG_get_value( profile, 
				"/profile/notBefore/days")) != NULL ) {
			days = atoi( tmp_s );
			PKI_Free ( tmp_s );
		};

		if(( tmp_s = PKI_CONFIG_get_value( profile, 
				"/profile/notBefore/hours")) != NULL ) {
			hours = atoi( tmp_s );
			PKI_Free ( tmp_s );
		};

		if(( tmp_s = PKI_CONFIG_get_value( profile, 
				"/profile/notBefore/minutes")) != NULL ) {
			mins = atoi( tmp_s );
			PKI_Free ( tmp_s );
		};

		if(( tmp_s = PKI_CONFIG_get_value( profile, 
				"/profile/notBefore/minutes")) != NULL ) {
			secs = atoi( tmp_s );
			PKI_Free ( tmp_s );
		};

		lastUpdateVal = secs +
						( mins * 60 ) + 
						( hours * 3600 ) + 
						( days 	* 3600 * 24 ) + 
						( years * 3600 * 24 * 365 );
	} 
	else 
	{
		// Sets lastUpdate to current time
		lastUpdateVal = 0;
	};

	if ( profile && validity <= 0 ) {
		long long years = 0;
		long long days = 0;
		long long hours = 0;
		long long mins = 0;
		long long secs = 0;

		if((tmp_s = PKI_CONFIG_get_value ( profile,
					"/profile/validity/years")) != NULL ) {
			years = atoll( tmp_s );
			PKI_Free(tmp_s);
		}

		if((tmp_s = PKI_CONFIG_get_value ( profile,
					"/profile/validity/days")) != NULL ) {
			days = atoll( tmp_s );
			PKI_Free( tmp_s );
		}

		if((tmp_s = PKI_CONFIG_get_value ( profile,
					"/profile/validity/hours")) != NULL ) {
			hours = atoll( tmp_s );
			PKI_Free( tmp_s );
		}

		if((tmp_s = PKI_CONFIG_get_value ( profile,
					"/profile/validity/mins")) != NULL ) {
			mins = atoll( tmp_s );
			PKI_Free ( tmp_s );
		}

		if((tmp_s = PKI_CONFIG_get_value ( profile,
					"/profile/validity/secs")) != NULL ) {
			secs = atoll( tmp_s );
			PKI_Free ( tmp_s );
		}

		nextUpdateVal = secs + 
				60 * ( mins + 
					60 * (hours + 
						24 * ( days + 
							365 * years 
						     )
					     )
				    );
	} 
	else
	{
		nextUpdateVal = (long long) validity;
	};

	/* Generates a new time for lastUpdate field */
	if((time = PKI_TIME_new( lastUpdateVal )) == NULL ) {
		PKI_ERROR(PKI_ERR_MEMORY_ALLOC, NULL);
		goto err;
	};

	/* Set the Last Update field */
	if(X509_CRL_set_lastUpdate( val, time ) == 0 ) {
		PKI_log_err ( "ERROR, can not set lastUpdate field in CRL");
		goto err;
	}
	PKI_TIME_free ( time );
	time = NULL; // Memory

	/* Generates a new time for lastUpdate field */
	if((time = PKI_TIME_new( nextUpdateVal )) == NULL ) {
		PKI_ERROR(PKI_ERR_MEMORY_ALLOC, NULL);
		goto err;
	};

	/* Set the nextUpdate field */
	if(X509_CRL_set_nextUpdate( val, time ) == 0 ) {
		PKI_log_err ( "ERROR, can not set lastUpdate field in CRL");
		goto err;
	}
	PKI_TIME_free ( time );
	time = NULL; // Memory

	/* Now we need to add the CRL issuer name and details */
	if (X509_CRL_set_issuer_name( val, 
				X509_get_subject_name(cert->value)) == 0) {
		PKI_log_debug( "Can not set CRL issuer name");
		goto err;
	}

	if ( sk ) {
		/* Adds the list of revoked certificates */
		for(i=0; i < PKI_STACK_X509_CRL_ENTRY_elements(sk); i++ ) {
			PKI_log_debug("CRL::ADDING ENTRY %d\n", i );

			entry = PKI_STACK_X509_CRL_ENTRY_get_num(sk, i);
			if(!entry) break;

			X509_CRL_add0_revoked(val, entry);
		};
	}

	/* Sorts the CRL entries */
	X509_CRL_sort ( val );

	/*
	if((ret = PKI_X509_new_value( PKI_DATATYPE_X509_CRL, val, hsm)) == NULL ) {
		PKI_ERROR(PKI_ERR_MEMORY_ALLOC, NULL);
		X509_CRL_free ( val );
		return ( NULL );
	}
	*/

	/* Get the extensions from the profile */
	if( profile ) {
		PKI_TOKEN * tk;

		if((tk = PKI_TOKEN_new_null()) == NULL ) {
			PKI_log_err ( "Memory allocation failure");
			PKI_X509_CRL_free ( ret );
			return NULL;
		}

		PKI_TOKEN_set_cert ( tk, cert );
		PKI_TOKEN_set_keypair ( tk, k );

		if(PKI_X509_EXTENSIONS_crl_add_profile( profile, oids, ret, tk) == 0 ) {
					PKI_log_debug( "ERROR, can not set extensions!");
			PKI_X509_CRL_free ( ret );

			tk->cert = NULL;
			tk->keypair = NULL;
			PKI_TOKEN_free ( tk );

			return ( NULL );
		}
		tk->cert = NULL;
		tk->keypair = NULL;
		PKI_TOKEN_free ( tk );
	}

	/* Get the Digest Algorithm */
	if( (dgst = PKI_DIGEST_ALG_get_by_key( k )) == NULL ) {
		PKI_log_err("Can not get digest algor from keypair!");
		goto err;
	}
	
	rv = PKI_X509_sign ( ret, dgst, k );

	if ( rv == PKI_ERR ) {
		PKI_log_debug ("ERROR, can not sign CRL!");
		goto err;
	}

	return( ret );

err:

	if ( time ) PKI_TIME_free ( time );
	if ( ret ) PKI_X509_CRL_free ( ret );
	return NULL;
}

/*!
 * \brief Returns reason code from input text
 */

int PKI_X509_CRL_REASON_CODE_get ( const char * st ) {

	int ret = -1;
	int i = 0;

	if ( !st ) return ret;

	for ( i = 0; i < PKI_X509_CRL_REASON_CODE_num(); i++ ) {
		if( strcmp_nocase( (char *) st, (char *) PKI_X509_CRL_REASON_DESCR[i].name ) == 0 ) {
			ret = PKI_X509_CRL_REASON_DESCR[i].code;
			break;
		}
	}

	return ret;
};

/*!
 * \brief Returns a string representation of the passed reason code
 */

const char *PKI_X509_CRL_REASON_CODE_get_parsed ( int reason ) {
	if (reason < 0 || reason > PKI_X509_CRL_REASON_CODE_num() ) {
		return NULL;
	};

	return PKI_X509_CRL_REASON_DESCR[reason].name;
};

/*!
 * \brief Returns a description of the passed reason code
 */

const char *PKI_X509_CRL_REASON_CODE_get_descr ( int reason ) {
	if (reason < 0 || reason > PKI_X509_CRL_REASON_CODE_num() ) {
		return NULL;
	};

	return PKI_X509_CRL_REASON_DESCR[reason].descr;
};

/*!
 * \brief Returns the number of allowed revocation codes [0..NUM]
 */

int PKI_X509_CRL_REASON_CODE_num ( void ) {
	return sizeof(PKI_X509_CRL_REASON_DESCR)/sizeof(PKI_X509_CRL_REASON_CODE);
};

/*! \brief Generates a new PKI_X509_CRL_ENTRY from a certificate
 *
 * This function generates a new PKI_X509_CRL_ENTRY starting from a certificate.
 * The new structure can be added to a PKI_X509_CRL_ENTRY_STACK structure in
 * order to generate a new CRL.
 */

PKI_X509_CRL_ENTRY * PKI_X509_CRL_ENTRY_new ( PKI_X509_CERT *cert, 
		PKI_X509_CRL_REASON reason, PKI_TIME * revDate,
						PKI_X509_PROFILE *profile ) {

	PKI_X509_CRL_ENTRY *entry = NULL;
	char *ser_s = NULL;

	if((ser_s = PKI_X509_CERT_get_parsed(cert, PKI_X509_DATA_SERIAL)) 
								== NULL ) {
		return ( NULL );
	}

	entry = PKI_X509_CRL_ENTRY_new_serial ( ser_s, reason, 
							revDate, profile );

	if( ser_s ) PKI_Free ( ser_s );

	return ( entry );
}

/*! \brief Generates a new PKI_X509_CRL_ENTRY from a serial number (string)
 *
 * This function generates a new PKI_X509_CRL_ENTRY starting from a string
 * representing the serial number of the entry. The optional profile
 * is used to add extensions to the entry (when needed)
 */

PKI_X509_CRL_ENTRY * PKI_X509_CRL_ENTRY_new_serial ( char *serial, 
		PKI_X509_CRL_REASON reason, PKI_TIME *revDate,
					PKI_X509_PROFILE *profile ) {

	PKI_X509_CRL_ENTRY *entry = NULL;

	if( !serial ) return (NULL);

	if((entry = (PKI_X509_CRL_ENTRY *) X509_REVOKED_new()) == NULL ) {
		/* Memory alloc error */
		PKI_ERROR(PKI_ERR_MEMORY_ALLOC, NULL);
		return (NULL);
	}

	if((entry->serialNumber = PKI_INTEGER_new_char(serial)) == NULL ) {
		PKI_ERROR(PKI_ERR_MEMORY_ALLOC, "Can not convert serial %s to Integer");
		goto err;
	}

	if (!revDate) revDate = PKI_TIME_new( 0 );

	if (!X509_REVOKED_set_revocationDate((X509_REVOKED *) entry, (ASN1_TIME *) revDate))
		goto err;

	if (reason != PKI_CRL_REASON_UNSPECIFIED)
	{
		int supported_reason = -1;
		ASN1_ENUMERATED *rtmp = ASN1_ENUMERATED_new();

		switch (reason )
		{
			case PKI_CRL_REASON_CERTIFICATE_HOLD:
			case PKI_CRL_REASON_HOLD_INSTRUCTION_REJECT:
				if (!X509_REVOKED_add1_ext_i2d(entry, NID_hold_instruction_code, PKI_OID_get("holdInstructionReject"), 0, 0))
					goto err;

				if (revDate && !X509_REVOKED_add1_ext_i2d(entry, NID_invalidity_date, revDate, 0, 0))
					goto err;

				supported_reason = PKI_CRL_REASON_CERTIFICATE_HOLD;
				break;

			/* --- Deprecated in RFC 5280 ---
			case PKI_CRL_REASON_HOLD_INSTRUCTION_NONE:
				if (!X509_REVOKED_add1_ext_i2d(entry, NID_hold_instruction_code, 
						PKI_OID_get( "holdInstructionReject"), 0, 0)) {
					goto err;
				};
				if( revDate && !X509_REVOKED_add1_ext_i2d ( entry, 
						NID_invalidity_date, revDate, 0, 0)) {
					goto err;
				};
				reason = PKI_CRL_REASON_CERTIFICATE_HOLD;
				break;
			*/

			case PKI_CRL_REASON_HOLD_INSTRUCTION_CALLISSUER:
				if (!X509_REVOKED_add1_ext_i2d(entry, NID_hold_instruction_code, 
						PKI_OID_get( "holdInstructionCallIssuer"), 0, 0)) goto err;

				if( revDate && !X509_REVOKED_add1_ext_i2d ( entry, 
						NID_invalidity_date, revDate, 0, 0)) goto err;

				supported_reason = PKI_CRL_REASON_CERTIFICATE_HOLD;
				break;

    	case PKI_CRL_REASON_KEY_COMPROMISE:
    	case PKI_CRL_REASON_CA_COMPROMISE:
    	case PKI_CRL_REASON_AFFILIATION_CHANGED:
    	case PKI_CRL_REASON_SUPERSEDED:
    	case PKI_CRL_REASON_CESSATION_OF_OPERATION:
    	case PKI_CRL_REASON_REMOVE_FROM_CRL:
    	case PKI_CRL_REASON_PRIVILEGE_WITHDRAWN:
    	case PKI_CRL_REASON_AA_COMPROMISE:

			default:
				PKI_ERROR(PKI_ERR_GENERAL, "CRL Reason Unknown %d", reason);
				supported_reason = -1;
				break;
		};

		if (supported_reason >= 0)
		{
			if (!ASN1_ENUMERATED_set(rtmp, supported_reason)) goto err;
			if (!X509_REVOKED_add1_ext_i2d( entry, NID_crl_reason, rtmp, 0, 0)) goto err;
		}

		/*
		if( reason == CRL_REASON_HOLD_INSTRUCTION ) {
			// if (!X509_REVOKED_add1_ext_i2d ( entry, 
			// 		NID_invalidity_date, revDate, 0, 0)) {
			// 	goto err;
			// };
			// if (!X509_REVOKED_add1_ext_i2d(entry, NID_hold_instruction_code, 
				// 	PKI_OID_get( "holdInstructionReject"), 0, 0)) {
			// goto err;
			// };
		};
		*/

	}

/*
	if (rev && !X509_REVOKED_set_revocationDate(rev, revDate))
                goto err;

        if (rev && (reason_code != OCSP_REVOKED_STATUS_NOSTATUS))
                {
                rtmp = ASN1_ENUMERATED_new();
                if (!rtmp || !ASN1_ENUMERATED_set(rtmp, reason_code))
                        goto err;
                if (!X509_REVOKED_add1_ext_i2d(rev, NID_crl_reason, rtmp, 0, 0))
                        goto err;
                }

        if (rev && comp_time)
                {
                if (!X509_REVOKED_add1_ext_i2d(rev, NID_invalidity_date, comp_time, 0, 0))
                        goto err;
                }
	if (rev && hold)
                {
                if (!X509_REVOKED_add1_ext_i2d(rev, NID_hold_instruction_code, hold, 0, 0))
                        goto err;
                }

*/

	return( entry );

err:

	if( entry ) X509_REVOKED_free ( (X509_REVOKED *) entry );
	return (NULL);
}

void PKI_X509_CRL_ENTRY_free_void ( void *entry ) {
	PKI_X509_CRL_ENTRY_free ( (PKI_X509_CRL_ENTRY *) entry );
}

/*! \brief Frees a PKI_X509_CRL_ENTRY
 *
 * Frees memory associated to a PKI_X509_CRL_ENTRY
 */

int PKI_X509_CRL_ENTRY_free ( PKI_X509_CRL_ENTRY *entry ) {

	if( !entry ) return (PKI_ERR);

	if( entry ) X509_REVOKED_free ( (X509_REVOKED *) entry );

	return (PKI_OK);

}

/*! \brief Find an entry within a CRL by using the PKI_INTEGER serial
 *         number of the certificate
 *
 * This function look inside a CRL and returns the PKI_X509_CRL_ENTRY
 * if the passed serial is found.  The returned pointer refers to the
 * internal CRL structure, when the CRL is freed the pointer will no
 * more point to a valid memory area.
 */

PKI_X509_CRL_ENTRY * PKI_X509_CRL_lookup( PKI_X509_CRL *x, 
						PKI_INTEGER *s ) {

	PKI_X509_CRL_ENTRY *r = NULL;
	X509_CRL *crl = NULL;

        long long curr = 0;
        long long end, cmp_val;

	if( !x || !s ) return (NULL);

	/* Set the end point to the last one */
	end = (long long) sk_X509_REVOKED_num(crl->crl->revoked) - 1;
	if( end < 0 ) return (r);

	crl = (X509_CRL *) x;

        /* Look for serial number of certificate in CRL */
        // rtmp.serialNumber = (ASN1_INTEGER *) serial;
        // ok = sk_X509_REVOKED_find(crl->crl->revoked, &rtmp);

	for( curr = 0 ; curr <= end ; curr++ ) {
		r = sk_X509_REVOKED_value( crl->crl->revoked, (int) curr );
                if(( cmp_val = ASN1_INTEGER_cmp(r->serialNumber, s)) == 0 ) {
			/* Found */
			break;
		}
	}

	return( r );
}

/*! \brief Find an entry within a CRL
 *
 * This function look inside a CRL and returns the PKI_X509_CRL_ENTRY
 * if the passed serial is found.  The returned pointer refers to the
 * internal CRL structure, when the CRL is freed the pointer will no
 * more point to a valid memory area.
 */

PKI_X509_CRL_ENTRY * PKI_X509_CRL_lookup_serial( PKI_X509_CRL *x, 
							char *serial ) {

	ASN1_INTEGER *s = NULL;
	PKI_X509_CRL_ENTRY *r = NULL;

	if( !x || !serial ) return (NULL);

	if((s = PKI_INTEGER_new_char ( serial )) == NULL ) {
		return ( NULL );
	}

	r = PKI_X509_CRL_lookup ( x, s );

	if ( s ) PKI_INTEGER_free ( s );

	return( r );
}

/*! \brief Find an entry in a CRL by using a certificate
 *
 * Use the information within the certificate to look for a corresponding
 * entry in the CRL. If found, the PKI_X509_CRL_ENTRY structure is copied and
 * returned.  The returned pointer refers to the internal CRL structure, 
 * when the CRL is freed the pointer will no more point to a valid memory
 * area.
 */

PKI_X509_CRL_ENTRY * PKI_X509_CRL_lookup_cert ( PKI_X509_CRL *x, 
							PKI_X509_CERT *cert ) {

	// ASN1_INTEGER *serial = NULL;
	PKI_INTEGER *serial = NULL;

	if( !x || !cert ) return (NULL);

	if((serial = PKI_X509_CERT_get_data ( cert, 
			PKI_X509_DATA_SERIAL )) == NULL ) {
		return ( NULL );
	}

	/*
	if((serial = X509_get_serialNumber( (X509 *) cert )) == NULL ) {
		return (NULL);
	}

	if((serial_s = i2s_ASN1_INTEGER( NULL, serial )) == NULL ) {
		return(NULL);
	}

	entry = PKI_X509_CRL_lookup_serial( x, serial_s );
	*/

	/* Now let's free the serial_s */
	// if( serial_s ) PKI_Free ( serial_s );

	return PKI_X509_CRL_lookup ( x, serial );
	// return( entry );
}

/*! \brief Lookup for a serial (long long) in a PKI_X509_CRL and returns the
 *         PKI_X509_CRL_ENTRY entry if found */

PKI_X509_CRL_ENTRY * PKI_X509_CRL_lookup_long(PKI_X509_CRL *x, long long s ) {

	PKI_INTEGER * serial = NULL;
	PKI_X509_CRL_ENTRY *entry = NULL;

	if ( !x ) return NULL;

	serial = PKI_INTEGER_new ( s );

	entry = PKI_X509_CRL_lookup ( x, serial );

	if ( serial ) PKI_INTEGER_free ( serial );

	return ( entry );
}

/*! \brief Adds an Extension to a CRL object
 */

int PKI_X509_CRL_add_extension(PKI_X509_CRL *x, PKI_X509_EXTENSION *ext) {

	if( !x || !x->value || !ext || !ext->value ) return (PKI_ERR);

	if (!X509_CRL_add_ext((X509_CRL *)x->value, ext->value, -1)) 
		return (PKI_ERR);

	return (PKI_OK);
}

/*! \brief Adds a stack of extensions to a CRL object.
 */

int PKI_X509_CRL_add_extension_stack(PKI_X509_CRL *x, 
					PKI_X509_EXTENSION_STACK *ext) {

	int i = 0;

	if( !x || !ext ) return (PKI_ERR);

	for( i = 0; i < PKI_STACK_X509_EXTENSION_elements(ext); i++ ) {
		PKI_X509_EXTENSION *ossl_ext = NULL;

		ossl_ext = PKI_STACK_X509_EXTENSION_get_num( ext, i);
		if (!ossl_ext ) continue;

		if(!X509_CRL_add_ext ( (X509_CRL *) x->value, 
							ossl_ext->value, -1 )) {
			PKI_log_err("Adding Extension::%s",
				ERR_error_string(ERR_get_error(), NULL));
			return ( PKI_ERR );
		}

	}

	return (PKI_OK);
}

/*! \brief Get Data from a CRL object
 */

void * PKI_X509_CRL_get_data ( PKI_X509_CRL *x, PKI_X509_DATA type ) {

	void *ret = NULL;
	PKI_X509_CRL_VALUE *tmp_x = NULL;
	PKI_MEM *mem = NULL;

	if( !x || !x->value ) return (NULL);

	tmp_x = x->value;

	switch( type ) {
		case PKI_X509_DATA_VERSION:
			ret = (tmp_x)->crl->version;
			// ret = X509_CRL_get_version((X509_CRL *) x);
			break;
		case PKI_X509_DATA_ISSUER:
			ret = X509_CRL_get_issuer ((X509_CRL *) tmp_x);
			break;
		case PKI_X509_DATA_NOTAFTER:
		case PKI_X509_DATA_NEXTUPDATE:
			ret = X509_CRL_get_nextUpdate ((X509_CRL *) tmp_x);
			break;
		case PKI_X509_DATA_NOTBEFORE:
		case PKI_X509_DATA_LASTUPDATE:
			ret = X509_CRL_get_lastUpdate ((X509_CRL *) tmp_x);
			break;
		case PKI_X509_DATA_EXTENSIONS:
			// ret = X509_CRL_get_extensions((X509_CRL *) x );
			PKI_log_debug("To Be implemented!");
			ret = NULL;
			// ret = X509_CRL_get_algor((X509_CRL *) x );
			break;
		case PKI_X509_DATA_SIGNATURE:
			ret = tmp_x->signature;
			// ret = X509_CRL_get_signature ((X509_CRL *) x);
			break;
		case PKI_X509_DATA_SIGNATURE_ALG1:
			if (tmp_x->crl) ret = tmp_x->crl->sig_alg;
			break;
		case PKI_X509_DATA_ALGORITHM:
		case PKI_X509_DATA_SIGNATURE_ALG2:
			ret = tmp_x->sig_alg;
			break;
		case PKI_X509_DATA_TBS_MEM_ASN1:
			if((mem = PKI_MEM_new_null()) == NULL ) break;
			mem->size = (size_t) ASN1_item_i2d ( (void *) tmp_x->crl, 
				&(mem->data), &X509_CRL_INFO_it );
			ret = mem;
			break;
		default:
			/* Not Recognized/Supported DATATYPE */
			return (NULL);
	}

	return (ret);
}

char * PKI_X509_CRL_get_parsed( PKI_X509_CRL *x, PKI_X509_DATA type ) {

	char *ret = NULL;
	PKI_ALGOR *al = NULL;

	if( !x || !x->value ) return (NULL);

	// PKI_X509_CRL_VALUE *x_val = NULL;
	// x_val = x->value;

	switch (type)
	{
		case PKI_X509_DATA_VERSION:
			ret = PKI_INTEGER_get_parsed(PKI_X509_CRL_get_data(x, type));
			if (!ret) ret = strdup ("NONE");
			break;

		case PKI_X509_DATA_ISSUER:
			ret = PKI_X509_NAME_get_parsed(PKI_X509_CRL_get_data(x, type));
			if (!ret) ret = strdup ("NONE");
			break;

		case PKI_X509_DATA_ALGORITHM:
			if ((al = PKI_X509_CRL_get_data ( x, type )) != NULL)
				ret = strdup ( PKI_OID_get_descr ( al->algorithm));
			else
				ret = strdup ("NONE");
			break;

		case PKI_X509_DATA_LASTUPDATE:
		case PKI_X509_DATA_NEXTUPDATE:
		case PKI_X509_DATA_NOTBEFORE:
		case PKI_X509_DATA_NOTAFTER:
			ret = PKI_TIME_get_parsed(PKI_X509_CRL_get_data(x, type));
			if (!ret) ret = strdup ("NONE");
			break;

		default:
			/* Not Recognized/Supported DATATYPE */
			return (NULL);
	}

	return (ret);
}

int PKI_X509_CRL_print_parsed(PKI_X509_CRL *x, PKI_X509_DATA type, int fd){

	const char *str = NULL;
	ssize_t rv = 0;

	if ( !x ) return ( PKI_ERR );

	switch (type)
	{
		default:
			/* Not Recognized/Supported DATATYPE */
			return ( PKI_ERR );
	}

	if (str)
	{
		rv = write(fd, str, (size_t) strlen(str));
		if (rv < strlen(str))
		{
			PKI_ERROR(PKI_ERR_GENERAL, "Error writing bytes (%d vs %d)", rv, strlen(str));
			return PKI_ERR;
		}
	}

	return PKI_OK;
}

/*! \brief Puts a CRL in a PKI_MEM structure */
/*
PKI_MEM *PKI_X509_CRL_put_mem ( PKI_X509_CRL *x, int format ) {

	PKI_X509_CRL_STACK *sk = NULL;
	PKI_MEM *pki_mem = NULL;

	if( !x ) return ( NULL );

	if((sk = PKI_STACK_X509_CRL_new()) == NULL ) {
		PKI_log_err ( "PKI_X509_CRL_STACK_put_mem()::Memory Error!");
		return ( NULL );
	}

	if( PKI_STACK_X509_CRL_push ( sk, x ) == PKI_ERR ) {
		PKI_log_err ( "PKI_X509_CRL_STACK_put_mem()::Memory Error!");
		if( sk ) PKI_STACK_X509_CRL_free ( sk );
		return ( NULL );
	}

	if((pki_mem = PKI_MEM_new_null()) == NULL ) {
		PKI_log_err ( "PKI_X509_CRL_STACK_put_mem()::Memory Error!");
		if( sk ) PKI_STACK_X509_CRL_free ( sk );
		return ( NULL );
	}

	if((PKI_X509_CRL_STACK_put_mem(sk, format, pki_mem, NULL, 0)) 
								== PKI_ERR) {
		if( sk ) PKI_STACK_X509_CRL_free ( sk );
		if ( pki_mem ) PKI_MEM_free ( pki_mem );
		return ( NULL );
	}

	if ( sk ) PKI_STACK_X509_CRL_free_all (sk);

	return ( pki_mem );
}
*/
