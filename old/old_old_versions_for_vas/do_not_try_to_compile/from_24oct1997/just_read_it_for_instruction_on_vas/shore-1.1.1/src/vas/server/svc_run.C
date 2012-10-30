/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/svc_run.C,v 1.51 1995/09/01 20:33:45 zwilling Exp $
 */
#include <copyright.h>
#include <externc.h>
#include <sthread.h>
#include <debug.h>

// in stubs.C
extern "C" void shutting_down_wait();

BEGIN_EXTERNCLIST
	void	svc_run();
END_EXTERNCLIST

/*
 *  shutting_down condition
 *	svc_run() waits on shutting_down while the system services
 *	client requests. To allow svc_run() to proceed,
 *	signal shutting_down.
 */

smutex_t shutdown_mutex("shut");
scond_t shutting_down("shut");

/*
 *  set up (initialize) everything and wait on shutting_down condition
 *  all the sockets created with svc[PROTO]_create are xprt_register-ed
 */
void 
svc_run()
{
	FUNC(svc_run);
	w_rc_t e;

	/* MAIN THREAD */

    /*
     *  This thread goes to sleep here while the server services requests.
     */
	DBG( << "shutting_down.wait" );
	W_COERCE(shutdown_mutex.acquire());
    if(e=shutting_down.wait(shutdown_mutex)) {
		cerr << e << endl; assert(0);
	}
	shutdown_mutex.release();
    
	DBG( << "shutting_down signaled" );
	// shutting_down_wait(); //shouldn't have to do this
}

// for use by vmsg.C
extern "C" void shutting_down_signal();
void shutting_down_signal() { 
	FUNC(shutting_down_signal);
	shutting_down.broadcast(); 
}
