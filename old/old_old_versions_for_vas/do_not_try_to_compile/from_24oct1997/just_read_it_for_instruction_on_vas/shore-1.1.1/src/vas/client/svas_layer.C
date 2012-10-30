/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/svas_layer.C,v 1.22 1995/09/07 17:58:18 nhall Exp $
 */

#include "svas_layer.h"
#include "sm_app.h"

#include <debug.h>
#include <errors.h>
#include <uname.h>

#include <os_error_def.h>
#include <svas_error_def.h>
#include <opt_error_def.h>
#include <fc_error.h>
#include <e_error.h>

const
#include "../../sm/e_einfo.i"

/*
// exclude thread errors -- should never be sent to the client
#include "../../sthread/st_error.h"
*/

extern w_error_info_t svas_error_info[];
extern w_error_info_t os_error_info[];
extern w_error_info_t opt_error_info[];

svas_layer_init ShoreVasLayer; // the only one.

w_rc_t 
svas_layer::setup_options (option_group_t* options)
{
	FUNC(svas_layer::setup_options);
	DBG(<<"CLIENT layer setup options");
	{
		ShoreVasLayer.options = options;

#ifdef W_DO
#undef W_DO
#endif
#define W_DO(x) {  				\
		w_rc_t __e = (x);		\
		if (__e) { return __e; }\
	}
		W_DO(options->add_option("svas_host", "string",
			"localhost",
			"\"localhost\" or name of host",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_host));

		W_DO(options->add_option("svas_port", "1024 < integer < 65535", 
			"2999", "port at which to contact server",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_port));

		W_DO(options->add_option("svas_log", "string",
			"-",
			"- (stderr), syslogd (to syslog daemon), or <filename>",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_log));

		W_DO(options->add_option("svas_shm_small_obj", "integer", 
			"16", 
		"KB shared memory to use for transferring pages of small anonymous objects to the client",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_shm_small_obj));

		W_DO(options->add_option("svas_shm_large_obj", "integer", 
			"256", 
		"KB shared memory to use for transferring registered and large objects, and for updates",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_shm_large_obj));

		W_DO(options->add_option("svas_use_pmap", "yes/no", 
		"no",
	"yes causes the client library to use the portmapper to locate the server",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_use_pmap));

        W_DO(options->add_option("svas_print_user_errors", "yes/no", "no",
            "yes causes server to print error messages to stdout",
            false,
            option_t::set_value_bool, ShoreVasLayer.opt_print_user_errors));

#ifdef notdef
// auditing is now obsolete, but we're not throwing this away
// quite yet.
#ifdef DEBUG
		W_DO(options->add_option("svas_audit", "yes/no", "no",
			"yes causes all sorts of internal audits (debugging system only)",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_audit));
#endif
#endif
	}
	return RCOK;
}

svas_layer_init:: svas_layer_init():
		options(0), log(0), 
		opt_host(0), opt_port(0), opt_log(0),
		opt_shm_small_obj(0), opt_shm_large_obj(0), 
		opt_audit(0),
		_pusererrs(0), 
		_sm_page_size(smlevel_0::page_sz), 
		_sm_sm_data_size(ssm_constants::max_small_rec), 
		_sm_lg_data_size(ssm_constants::lg_rec_page_space) 
{
	// Initialize the error codes
#define INITD(_nn, _aa,_bb,_cc)\
	if (! (w_error_t::insert( _nn, _aa, _bb - _cc + 1))) { \
		catastrophic("Cannot initalize error codes."); \
	} \

	INITD("SVAS", svas_error_info,SVAS_ERRMAX,SVAS_ERRMIN);
	INITD("SVAS", os_error_info,OS_ERRMAX,OS_ERRMIN);
	INITD("OPTIONS", opt_error_info,OPT_ERRMAX,OPT_ERRMIN);
	INITD("SSM", smlevel_0::error_info,smlevel_0::eERRMAX,smlevel_0::eERRMIN);
	INITD("FC", w_error_t::error_info,w_error_t::fcERRMAX,w_error_t::fcERRMIN);

#ifdef DEBUG
	// test_errors();
#endif
}

svas_layer_init:: ~svas_layer_init()
{
	cleanup_pwstuff();
	cleanup_netdb();
	if(log) {
		delete log;
		log = 0;
	}
	// options are deleted by higher layer
}


option_t	*
svas_layer_init:: svas_shellrc() const { return opt_shellrc; }
option_t	*
svas_layer_init:: svas_tcllib() const { return opt_tcllib; }
void svas_set_shellrc(option_t *x) { ShoreVasLayer.opt_shellrc = x; }

int
svas_layer_init:: sm_page_size()  const{ return _sm_page_size; }
int
svas_layer_init:: sm_lg_data_size()  const{ return _sm_lg_data_size; }
int
svas_layer_init:: sm_sm_data_size()  const{ return _sm_sm_data_size; }
bool
svas_layer_init:: pusererrs()  const{ return _pusererrs; }
option_group_t		 *
svas_layer_init::option_group()  { return options; }
