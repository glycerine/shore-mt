/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/stubs.C,v 1.19 1995/09/01 19:52:43 nhall Exp $
 */
#include <copyright.h>
#include <vas_internal.h>
#include <tcl.h>
#include <interp.h>
#include "server_stubs.h"

#include "cltab.h"
extern "C" void shutting_down_wait();
extern "C" void shutting_down_listeners();

void 
pclients(void *unused, int vb) 
{
	CLTAB->dump(vb);
}

void 
pconfig(ostream &out)
{
	ShoreVasLayer.pconfig(out);
}

char *defaultHost()  { return ShoreVasLayer.ShoreUser; }

int 	
mkunixfile(void *unused, void *v, char *c,mode_t m,vec_t &h,lrid_t *l) {
		return ((svas_server *)v)->mkUnixFile(c, m, h, l);
}

EXTERNC void dumpthreads(void);
EXTERNC void dumplocks(void);

void
_dumpthreads(void *unused) 
{
	cerr << "THREADS:"<<endl; dumpthreads(); // dump
}

void
_dumplocks(void *unused) 
{
	w_rc_t	e;
	cerr << "LOCKS:" << endl;  // dump
	e = ss_m::dump_locks();
	if(e) {
		cerr << e << endl; // dump
	}
}

#include "vaserr.h"

VASResult		
svas_server::_interruptTrans(
	int sock
)
{
	VFPROLOGUE(svas_server::interruptTrans); 

	PRIVILEGED_OP;

	FSTART
	struct client_t *c = CLTAB->find(sock);
	if(c) {
		// set its state to interrupted; next
		// operation that requires a tx will abort
		if(c->_server->status.txstate == Active) {
			c->_server->status.txstate = Interrupted;
		} else {
			VERR(SVAS_TxRequired);
		}
	} else {
		VERR(SVAS_NotFound);
		FAIL;
	}

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

void 
shutting_down_listeners()
{
	DBG( << "shutting down listeners")
	w_rc_t e;
	e = ShoreVasLayer.shutdown_listeners();
	if(e) { return; } // avoid "Error not checked"
}

void 
shutting_down_wait()
{
	DBG( << "shutting_down_wait")
	if(CLTAB) {
		int k;
		// this number depends on
		// what is configured
		me()->yield(true);
		while((k = CLTAB->count()) > 1) {
			// have to allow me() to remain in the
			// client table
			DBG(<< k << "clients still have to self-destruct; yielding");
			CLTAB->dump(false);
			sleep(1);
			me()->yield(true);
		}
	}
	DBG( << "prepared to shut down ")
}

VASResult		
svas_server::_interrupt_all()
{
	VFPROLOGUE(svas_server::interrupt_all); 

	if(!privileged()) { RETURN SVAS_FAILURE; }

	if(CLTAB) {
		struct client_t *c;
		int i, j;
		int k = CLTAB->count();
		for(i=0; i < ShoreVasLayer.OpenMax; i++) {
			if (_interruptTrans(i) == SVAS_OK) {
				// client found even if no tx running
				j++;
			}
			if(j == k) break;
		}
	}
	clr_error_info();
	RETURN SVAS_OK;
}

VASResult		
svas_server::gatherRemoteStats(w_statistics_t &out)
{
	VFPROLOGUE(svas_server::gatherRemoteStats); 

	// meaningless on server side

	VERR(SVAS_NotImplemented);
FFAILURE:
	RETURN SVAS_FAILURE;
}

void
yield() 
{
	me()->yield(true);
}

void
crash(void *unused)
{
	// first -- scribble on the stack to see if
	// stack check works
	// We'll do that by putting lots of stack frames
	// on, and checking after each frame. In that
	// way, we hope to avoid scribbling on any list
	// structures, etc. that are used by the thread-
	// switching code.  We want to keep them intact
	// so that we can catch the stack overflow cleanly.
#ifdef PURIFY
	if(purify_is_running()) {
		DBG(<<"CALLED crash()");
		exit(1); // called crash()
	}
#endif
	static  int frame_count = 0;
	frame_count++;
	int 	junk[8];
	memset(junk,'a', sizeof(junk));
	me()->yield(); // check the stack...
	crash(unused); // ad nauseum
}
