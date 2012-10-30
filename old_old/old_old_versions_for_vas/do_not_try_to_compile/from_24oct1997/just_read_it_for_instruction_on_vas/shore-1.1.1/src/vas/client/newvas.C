/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/newvas.C,v 1.14 1995/04/24 19:43:29 zwilling Exp $
 */
#include "vas_internal.h"
#include <ctype.h>
#include <errlog.h>
#include <server_stubs.h>
#include <svas_layer.h>


w_rc_t 
new_svas(
	svas_base **result,	// == NULL
	const char *host, 	// == NULL -> uses option
	int nsmall, 		// == -1 -> uses option (0 is legit)
	int nlarge 			// == -1 -> uses option (0 is legit)
)
{
	FUNC(new_svas_client);
	VASResult 	_res = SVAS_OK;
	mode_t 		mask;

#define shorelog ShoreVasLayer.log
	if(!shorelog) {
		const char * logfile=0;
		if(ShoreVasLayer.opt_log) {
			logfile = ShoreVasLayer.opt_log->value();
		}
		if(logfile==NULL)  {
			shorelog = new ErrLog("shore", log_to_stderr, 0);
			logfile = "stderr";
		} else {
			shorelog = new ErrLog("shore", log_to_unix_file, (void *)logfile);
		}
		if(!shorelog) {
			// catastrophy
			perror(logfile);
			catastrophic("cannot initialize shore log:");
		}
		shorelog->clog << info_prio << "new svas" << flushl;
	}

	// constructor looks up svas_host option if host==NULL:
	svas_client 		*v = new svas_client(host, shorelog);

	if(!v) {
		_res = SVAS_MallocFailure;
		shorelog->clog << error_prio 
			<< "Malloc failure in new_svas_client()" << flushl;
		goto bad;
	}

	if((_res = v->status.vasreason) != SVAS_OK) {
		shorelog->clog << error_prio << 
			"Server failure in new_svas_client(), reason=" << 
			::hex(v->status.vasreason) << flushl;
		goto bad;
	}
	
	mask = umask(0);
	(void) umask(mask);

	if(nsmall<0) {
		if(ShoreVasLayer.opt_shm_small_obj) {
			nsmall = (int) strtol(ShoreVasLayer.opt_shm_small_obj->value(),0,0);
		} 
		if(nsmall<0) {
			shorelog->clog << error_prio << "Bad value for option shm_small_obj" << flushl;
			goto bad;
		}
	}
	if(nlarge<0) {
		if(ShoreVasLayer.opt_shm_large_obj) {
			nlarge = (int) strtol(ShoreVasLayer.opt_shm_large_obj->value(),0,0);
		}
		if(nlarge<0) {
			shorelog->clog << error_prio << "Bad value for option shm_large_obj" << flushl;
			goto bad;
		}
	}

	DBG(<< "small_kbytes="  << nsmall
		<< " large_kbytes="  << nlarge );

	if( v->_init( mask, nsmall, nlarge ) != SVAS_OK ) {
		_res = v->status.vasreason;
		shorelog->clog << error_prio << "Server failure in init(), reason=" << 
			::hex(_res) << flushl;
		goto bad;
	}

// good:
	if(result) {
		*result = v;
	}
	return RCOK;

bad:
	if(v) delete v;
	if(result) {
		*result = 0;
	}
	return RC(_res);
#undef shorelog 
}
