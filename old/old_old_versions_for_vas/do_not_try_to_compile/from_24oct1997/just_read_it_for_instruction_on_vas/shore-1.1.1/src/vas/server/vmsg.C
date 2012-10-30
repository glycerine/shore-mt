/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/vmsg.C,v 1.22 1995/07/14 22:40:30 nhall Exp $
 */
#include <copyright.h>

// GROT __MSG_C__-ism
typedef int sm_save_point_t;
typedef int	stid_t;

#define __MSG_C__
#define __malloc_h
#include <msg.h>
#include <vas_internal.h>
#include <debug.h>
#include "vaserr.h"
#define __log__ ShoreVasLayer.remote_service->log
#define MY_MSG_STATS vmsg_stats
#include "funcproto.h"

static const int vmsg_values[] = {
#include "vmsg_stats.i"
	-1
};
static const char *vmsg_strings[] = {
#include "vmsg_names.i"
	(char *) 0
};
struct msg_info vmsg_names = {
	vmsg_values,
	vmsg_strings
};

_msg_stats MY_MSG_STATS("Remote SVAS RPCs", 0x000a0000,
	(int)path2loid , (int)openfile, vmsg_names);

void
cvmsg_stats() {
	MY_MSG_STATS.clear();
}
void_reply *
vzero_1(
	vzero_arg *argp,
	void *clnt
)
{ 
	FSTART(void,vzero);
	REPLY;
}

lrid_t_reply *
path2loid_1( path2loid_arg *argp, void *unused)
{ return NULL; }

void_reply *
openfile_1( openfile_arg *argp, void *unused)
{ return NULL; }

#include <time.h>

extern "C" void shutting_down_signal();
extern "C" void shutting_down_wait();
extern "C" void shutting_down_listeners();

void_reply *
shutdown1_1( shutdown1_arg *argp, void *clnt)
{ 
	FSTART(void,shutdown1);
	bool ok=true;
	if(
		(vas->uid() == svas_layer_init::RootUid ||
		vas->uid() == ShoreVasLayer.ShoreUid) &&
		(vas->gid() == svas_layer_init::RootGid ||
		vas->gid() == ShoreVasLayer.ShoreGid) ) {
		ok = true; 
	} else {
		ok = false;
	}

	if(ok) {
		time_t  now = svas_server::Now().tv_sec;

		__log__->log(log_emerg, "Shutdown %s at %s by client uid=%d",
				argp->crash?"-crash":argp->wait?"-wait":"-nowait",
				asctime( localtime(&now) ),
				vas->uid());

		DBG(<<"shutdown by remote client");
		if(argp->crash) {
			if(vas->privileged()) {
				DBG(<<"CRASH");
				exit(1); // crash from remote 
			} else {
				ok = false;
			}
		} 
	}

	if(ok) {
		DBG(<<"shutting down listeners");
		shutting_down_listeners();

		DBG(<<"listeners gone");
		if(!argp->wait) {
			DBG(<<"Interrupting running clients");
			if(vas->_interrupt_all()!= SVAS_OK) {
				ok = false;
			}
			DBG(<<"interrupted ");
		}
	}

	if(ok) {
		// wait for them to finish
		DBG(<<"waiting... ");
		shutting_down_wait();

		DBG(<<"signalling GOING DOWN NOW !!");

		shutting_down_signal();
	} else {
		vas->set_error_info(OS_PermissionDenied,  SVAS_FAILURE,
						svas_base::ET_USER,
						RCOK,
						_fname_debug_, __LINE__, __FILE__);
	}
	REPLY;
}
