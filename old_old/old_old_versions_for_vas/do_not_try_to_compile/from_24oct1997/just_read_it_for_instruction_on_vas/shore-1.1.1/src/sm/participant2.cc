/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


/*
 *  $Id: participant2.cc,v 1.3 1997/06/15 03:14:17 solomon Exp $
 *  
 *  Stuff common to both coordinator and subordnates.
 */

#define SM_SOURCE
#define COORD_C /* yes - COORD_C */


#include <sm_int_0.h>
#include <coord.h>

rc_t
participant::register_ep(
    CommSystem *	_comm,
    NameService *	_ns,
    const char *	uniquename,
    Endpoint &		ep
)
{
    rc_t 	rc;

    w_assert1(_ns);
    w_assert1(_comm);

    W_IGNORE(_ns->cancel((char *)uniquename));
    W_DO(_comm->make_endpoint(ep));

    w_assert3(ep.mep()->refs() == 1);

    if(smlevel_0::errlog->getloglevel() >= log_info) {
	smlevel_0::errlog->clog <<info_prio 
	    <<"REGISTERING " << uniquename <<" <--> ";
	    ep.print(smlevel_0::errlog->clog);
	    smlevel_0::errlog->clog << flushl;
    }

    W_DO(_ns->enter((char *)uniquename, ep));

    // GROT: enter increases # refs
    w_assert3(ep.mep()->refs() == 2);

    return RCOK;
}

rc_t
participant::un_register_ep(
    NameService *	_ns,
    const char *	uniquename
)
{
    rc_t 	rc;

    w_assert1(_ns);

    W_IGNORE(_ns->cancel((char *)uniquename));

    if(smlevel_0::errlog->getloglevel() >= log_info) {
	smlevel_0::errlog->clog <<info_prio 
	    <<"DEREGISTERED " << uniquename <<" <--> "
	    << flushl;
    }
    return RCOK;
}
