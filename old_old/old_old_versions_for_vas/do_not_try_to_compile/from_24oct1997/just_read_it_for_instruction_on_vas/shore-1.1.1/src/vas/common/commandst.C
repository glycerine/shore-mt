/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  Transaction-related commands
 *  $Header: /p/shore/shore_cvs/src/vas/common/commandst.C,v 1.2 1995/07/14 22:38:20 nhall Exp $
 */
#include <copyright.h>
#include "shell.misc.h"
#include "vasshell.h"
#include "server_stubs.h"


int
cmd_tstate(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_tstate);

	CHECK(1,1,NULL);
	CHECKCONNECTED;

	char *c;

	switch(Vas->status.txstate) {
		case Stale: c =  "stale"; break;
		case Active: c =  "active"; break;
		case Prepared: c =  "prepared"; break;
		case Aborting: c =  "aborting"; break;
		case Committing: c =  "committing"; break;
		case Ended: c =  "ended"; break;
		case NoTx: c =  "none"; break;
		default: c = "INTERNAL ERROR: Unknown state!"; break;
	}
	Tcl_AppendResult(ip,  c, 0);
	return TCL_OK;
}
int
cmd_tid(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_tid);

	CHECK(1,1,NULL);

	tid_t __t;
	CALL( trans(&__t) ); 
	if(res == SVAS_FAILURE) {
		Tcl_AppendResult(ip,  "none", 0);
	} else {
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
		tclout << __t << ends;
		Tcl_AppendResult(ip,  tclout.str(),  0);
	}
	return TCL_OK;
}

int
cmd_begin(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_begin);

	CHECK(1, 2, NULL);
	CHECKCONNECTED;
	if(ac > 1) {
		int degree = _atoi(av[1]);
		CALL(beginTrans(degree));
	} else {
		CALL(beginTrans());
	}

#ifdef USE_VERIFY
	v->begin();
#endif
	if(res != SVAS_FAILURE) {
		tid_t __t;
		CALL(trans(&__t);) 

		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
		tclout << __t << ends;

		if(res != SVAS_FAILURE) {
			Tcl_AppendResult(ip, tclout.str(), 0);
		}
	}
	VASERR(res);
}
int
cmd_interrupt(ClientData clientdata, 
	Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_interrupt);
	int 		fd;

	CHECK(2, 2, NULL);
	fd =  _atoi(av[1]);
	DBG(
		<< "interrupting client on socket  " << fd
	)
	CHECKCONNECTED;

	CALL(_interruptTrans(fd));
	VASERR(res);
}

int
cmd_trans(ClientData clientdata, 
	transGoal goal, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_trans);
	bool		have_tid = FALSE;
	int			low, high;

	CHECK(1, 2, NULL);
	if(ac==2) {
		char *l = strtok(av[1], ".");
		char *h = strtok( NULL, ".");
		low =  _atoi(l);
		high =  _atoi(h);
		DBG(
			<< "operating on tx " << low << "." << high
		)
		have_tid = TRUE;
	} else {
		low = 0;
		high = 0;
		have_tid = FALSE;
	}
	tid_t 	tid(low,high);

	CHECKCONNECTED;

	switch(goal) {
		case g_commitchain:
			CALL(commitTrans(true));
#ifdef USE_VERIFY
			v->commit();
#endif
			break;

		case g_commit:
			if(have_tid)  {
				CALL(commitTrans(tid));
			} else {
				CALL(commitTrans());
#ifdef USE_VERIFY
			v->commit();
#endif
			}
			break;
		case g_abort:
			if(have_tid) {
				CALL(abortTrans(tid));
			} else {
				CALL(abortTrans());

#ifdef USE_VERIFY
			v->abort();
#endif
			}
			break;
		case g_suspend:
			if(have_tid) {
#ifdef notdef
				CALL(suspendAction(tid));
#else
				Tcl_AppendResult(ip, "suspend/resume is not implemented.",0);
#endif
			} else {
				Tcl_AppendResult(ip, "tid required for suspend command", av[1], 0);
				CMDREPLY(tcl_error);
			}
			break;
		case g_resume:
			if(have_tid) {
#ifdef notdef
				CALL(resumeAction(tid));
#else
				Tcl_AppendResult(ip, "suspend/resume is not implemented.",0);
#endif
			} else {
				Tcl_AppendResult(ip, "tid required for resume command", av[1], 0);
				CMDREPLY(tcl_error);
			}
			break;
		default:
			assert(0);
	}
	if(res != SVAS_FAILURE) {
		tid_t __t;
		CALL(trans(&__t)); 

		tclout.seekp(ios::beg); tclout << __t << ends;
		dassert(!tclout.bad());
		if(res != SVAS_FAILURE) {
			Tcl_AppendResult(ip,  " ", tclout.str(), 0);
		} else {
			// no transaction
			res = SVAS_OK;
		}
	}
	VASERR(res);
}
int
cmd_chain(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_chain);
	CMDREPLY( cmd_trans(clientdata, g_commitchain, ip, 
	ac, av)==TCL_OK?tcl_ok:tcl_error ); 
}

int
cmd_commit(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_commit);
	CMDREPLY( cmd_trans(clientdata, g_commit, ip, ac, av)==TCL_OK?tcl_ok:tcl_error ); 
}

int
cmd_abort(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_abort);
	CMDREPLY( cmd_trans(clientdata, g_abort, ip, ac, av)==TCL_OK?tcl_ok:tcl_error ); 
}
int
cmd_suspend(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_suspend);
	CMDREPLY( cmd_trans(clientdata, g_suspend, ip, ac, av)==TCL_OK?tcl_ok:tcl_error ); 
}

int
cmd_resume(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_resume);
	CMDREPLY( cmd_trans(clientdata, g_resume, ip, ac, av)==TCL_OK?tcl_ok:tcl_error ); 
}
