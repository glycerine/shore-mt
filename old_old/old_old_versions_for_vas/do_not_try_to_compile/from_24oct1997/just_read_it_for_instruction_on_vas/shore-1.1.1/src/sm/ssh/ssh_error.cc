/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <w.h>
#include <e_error.h>
#include "e_error_def.h"
#include "st_error_def.h"
#include "opt_error_def.h"
#include "fc_error_def.h"
#ifdef USE_COORD
#include "sc_error_def.h"
#include "ns_error_def.h"
#endif

#include "e_einfo_bakw.i"
#include "opt_einfo_bakw.i"
#include "st_einfo_bakw.i"
#include "fc_einfo_bakw.i"
#ifdef USE_COORD
#include "sc_einfo_bakw.i"
#include "ns_einfo_bakw.i"
#endif
#include <debug.h>
#include "ssh_error.h"

const char *
ssh_err_msg(const char *str)
{
    FUNC(ssh_err_msg);

    return w_error_t::error_string(ssh_err_code(str));
}

unsigned int
ssh_err_code(const char *x)
{
    FUNC(ssh_err_code);
    w_error_info_t  *v;
    int             j;


#define LOOK(a,b,c) \
    v = (a);\
    j = (b);\
    while( (v != 0) && j++ <= (c) ) {\
	    if(strcmp(v->errstr,x)==0) { \
		    return  v->err_num;\
	    }\
	    v++;\
    }
    LOOK(e_error_info_bakw,E_ERRMIN,E_ERRMAX);
    LOOK(st_error_info_bakw,ST_ERRMIN,ST_ERRMAX);
    LOOK(opt_error_info_bakw,OPT_ERRMIN,OPT_ERRMAX);
    LOOK(fc_error_info_bakw,FC_ERRMIN,FC_ERRMAX);

#undef LOOK
    return fcNOSUCHERROR;
}

// returns error name given error code
// return false if the error code is not in SVAS_* or OS_*
const char *
ssh_err_name(unsigned int x)
{
    FUNC(ssh_err_name);

    DBG(<<"ssh_err_name for " << x);

    w_error_info_t  *v;
    int j;

#define LOOK(a,b,c) \
    v = (a);\
    j = (b);\
    while( (v != 0) && j++ <= (c) ) {\
	    DBG(<<"checking " << ::hex(v->err_num)); \
	    if(x == v->err_num) {\
		    return  v->errstr;\
	    }\
	    v++;\
    }
    
    LOOK(e_error_info_bakw,E_ERRMIN,E_ERRMAX);
    LOOK(st_error_info_bakw,ST_ERRMIN,ST_ERRMAX);
    LOOK(opt_error_info_bakw,OPT_ERRMIN,OPT_ERRMAX);
    LOOK(fc_error_info_bakw,FC_ERRMIN,FC_ERRMAX);
#ifdef USE_COORD
    LOOK(sc_error_info_bakw,SC_ERRMIN,SC_ERRMAX);
    LOOK(ns_error_info_bakw,NS_ERRMIN,NS_ERRMAX);
#endif

#undef LOOK

    return "bad error value";
}
