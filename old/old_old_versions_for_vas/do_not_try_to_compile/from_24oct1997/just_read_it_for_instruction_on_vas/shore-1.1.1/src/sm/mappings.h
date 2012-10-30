/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: mappings.h,v 1.11 1997/05/19 19:47:35 nhall Exp $
 */

#ifndef MAPPINGS_H
#define MAPPINGS_H
#ifdef USE_COORD

/*
 * This file describes two abstract classes that must be implemented
 * by a value-added server (VAS) that uses the Coordinator and Subordinate
 * classes for two-phase commit (2PC).
 *
 * Both classes implement 1:1 mappings: 
 * class name_ep_map 
 *    maps  names (opaque values) to Endpoints (a class in the object-comm
 *    package), and Endpoints to names.  This mapping is used by the
 *    Coordinator and by Subordinates.  It is the responsibility of the
 *    VAS to keep this mapping current:  if a subordinate VAS crashes
 *    and resumes with a different endpoint, the architecture of the
 *    VAS must provide for the mapping to be brought up-to-date.
 *
 * class tid_gtid_map
 *    maps a local transaction ID (tid) to a global transaction ID (gtid)
 *    (an opaque value), and a gtid to a tid.
 *    This is used by Subordinates to locate a local transaction to which
 *    to apply a command containing a global tid.   
 *
 * A coordinating VAS (master) must, in some sense, "know" what
 *    subordinate servers are involved in a distributed transaction,
 *    but the Coordinator and Subordinate classes to not require any
 *    such mapping to be maintained by the VAS.  
 */


#ifndef TID_T_H
#include <tid_t.h>
#endif
#ifndef STHREAD_H
#include <sthread.h>
#endif
#ifndef _SCOMM_HH_
#include <scomm.hh>
#endif
#ifndef __NS_CLIENT__
#include <ns_client.hh>
#endif


#define VIRTUAL(x) virtual x = 0;

class name_ep_map {
public: 
    virtual NORET  	~name_ep_map() {};
    virtual rc_t  	name2endpoint(const server_handle_t &, Endpoint &)=0;
     /*
      *	NB: regarding reference counts on endpoints:
      *          In order to avoid race conditions, it's necessary 
      *          for the implementation of this class to call ep.acquire()
      *          in name2endpoint(), and the caller must do ep.release().
      */          
    virtual rc_t  	endpoint2name(const Endpoint &, server_handle_t &)=0;

    /*
     * NB: both methods must return RC(nsNOTFOUND) if entry not found,
     * may return RC(scDEAD) if ep known to be dead.
     */
};


class tid_gtid_map {
public: 
    virtual NORET ~tid_gtid_map() {};
    virtual rc_t  add(const gtid_t &, const tid_t &)=0;
    virtual rc_t  remove(const gtid_t &, const tid_t &)=0;
    virtual rc_t  gtid2tid(const gtid_t &, tid_t &)=0;
    /*
     * NB: must return RC(fcNOTFOUND) if entry not found
     * (yes this is different from the above mapping)
     */

};

#endif
#endif /*MAPPINGS_H*/
