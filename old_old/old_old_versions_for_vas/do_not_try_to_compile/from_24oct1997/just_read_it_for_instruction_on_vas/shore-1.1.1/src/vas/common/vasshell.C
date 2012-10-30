/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/vasshell.C,v 1.7 1996/03/04 16:17:48 nhall Exp $
 */
#include <copyright.h>

#include "shell.misc.h"
#include "vasshell.h"
#include "interp.h"
#include "server_stubs.h"

#define VASPTR svas_base *

#ifdef USE_VERIFY
verify_t *v;
#endif


void
notfound(ClientData clientdata) {
	Vas->status.vasreason = SVAS_NotFound;
	Vas->status.vasresult = SVAS_FAILURE;
}

int statistics_mode=0;
int print_statistics(ClientData clientdata, Tcl_Interp* ip, int mode)
{
	FUNC(print_statistics);
	VASResult res;
	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

	if(!Vas) {
		tclout << "No vas." << ends;
	} else if(!Vas->connected()) {
		tclout << "Not connected." << ends;
	}else {
		DBG(<<"mode is " << mode);
		{
			// local stats:
			w_statistics_t	STATS;

			STATS << *Vas;

			tclout << "LOCAL STATS:" << endl;
			tclout << STATS << endl << endl;

			if(mode & s_remote) {
				DBG(<<"getting remote stats");
				// remote stats:
				w_statistics_t  RSTATS;
				if((res = Vas->gatherRemoteStats(RSTATS)) == SVAS_OK) {
					tclout << "REMOTE STATS:" << endl;
					tclout << RSTATS << endl << endl;
				} else {
					VASERR(res);
				}
			}
			tclout << ends;
		}
		if(mode & s_autoclear) {
			DBG(<<"clearing stats");
			Vas->cstats(); // clears them all
			shell_rusage.stop();
			shell_rusage.start();
		}
		// print to screen; don't Tcl_AppendResult
		cerr << tclout.str() << endl;
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
	}
	yield();
	return TCL_OK;
}
int clear_statistics(ClientData clientdata)
{
	if(Vas) {
		Vas->cstats(); // clears them all now
	}
}

CMDResult 
vasreply(ClientData clientdata, Tcl_Interp *ip, VASResult res, VASResult reason)
{
	FUNC(vasreply);

	if(res == SVAS_OK) {
		return tcl_ok;
	} if(reason == SVAS_RpcFailure) {
		Tcl_AppendResult(ip, "Rpc failure or server died.",0);
		delete Vas;
		((interpreter_t *)clientdata)->_vas =0;//  Vas = NULL;
	} else {
		dassert(res != SVAS_OK);
		dassert(reason != 0);
		AppendVasError(ip, res,reason);
	}
	yield();
	return tcl_error;
}

void AppendVasError(Tcl_Interp *ip, VASResult res, VASResult reason) 
{
	FUNC(AppendVasError);
	// char *kind = (res == SVAS_WARNING)?"WARNING ":"ERROR ";
	// Tcl_AppendResult(ip, "VAS returns ", kind,  ::form("0x%x", res), 0);
	// Tcl_AppendResult(ip, "; reason=", ::form("0x%x",reason), 0);
	if(res == SVAS_OK) {
		assert(SVAS_OK==0);
		assert(reason == 0);
	} else {
		assert(reason != 0);
		const char *name=0; DBG(<<"reason = " << reason );
		bool found = svas_base::err_name(reason, name); DBG(<<"found=" << found); DBG(<<"name = " << name );
		if(found) {
			Tcl_AppendResult(ip, name, 0);
		} else {
			Tcl_AppendResult(ip, svas_base::err_msg(fcNOSUCHERROR), 0);
		}
	}
	dassert(!tclout.bad());
}


int 
get_objsize( // Tcl_Interp* ip, VASPTR v, 
	char *av, 
	ObjectSize 	*c, 
	ObjectSize	*h, 
	ObjectOffset *tst,
	int			*nindexes
)
{
	FUNC(get_objsize);
	//
	// csize:hsize:tstart:nindexes
	// csize:hsize:tstart  --> nindexes=0
	// csize:hsize --> tstart=NoText, nindexes=0
	// csize --> hsize=0, tstart=NoText, nindexes=0
	//
	char *nxt;
	*h = 0;
	*tst = NoText;
	*nindexes = 0;

	nxt = strchr(av, ':');
	// set core size
	*c = _atoi(av);
	if(nxt==0) goto done;
	nxt++;

	// set heap size
	*h = _atoi(nxt);
	nxt = strchr(nxt, ':');
	if(nxt==0) goto done;
	nxt++;

	// set text start
	if(strncasecmp(nxt,"NoText",6)==0) {
		*tst = NoText;
	} else {
		*tst = _atoi(nxt);
	}

	nxt = strchr(nxt, ':');
	if(nxt==0) goto done;
	nxt++;

	*nindexes = _atoi(nxt);

done:
	DBG(<<"csize=" << *c
		<<" hsize=" << *h
		<<" tstart=" << *tst
		<<" nindexes=" << *nindexes
		);
	return TCL_OK;
}

int str2oid(
	ClientData clientdata,
	Tcl_Interp* ip, 
	VASPTR v, const char *av, lrid_t &target,
	bool do_lookup,
	bool follow_links,
	bool append_error_if_not,
	bool *isReg
)
{
	CMDFUNC(str2oid);
	// first we do a lookup so that even if it
	// starts with a digit, we want to see if there's
	// a filename with that name
	if(isReg) *isReg = FALSE;

	if(do_lookup) {
		bool found;
		VASResult res;

		DBG(<<"lookup:");

		res = v->lookup(av, &target, &found, Permissions::op_none,
			follow_links);
		if(res!= SVAS_OK) {
			DBG(<<"lookup failed");
			return TCL_ERROR;
		} else if(found) {
			if(isReg) *isReg = TRUE;
			DBG(<<"lookup: is registered");
			return SVAS_OK;
		} else if(!isdigit(*av)) {
			DBG(<<"lookup: not digit");
			// Fake out NotFound error
			notfound(clientdata);
			return TCL_ERROR;
		}
		// if it's a digit, at least try to interp as
		// an oid
	}
	DBG(<<"lookup: ");

	// next see if it could possibly be an oid
	if(!isdigit(*av)) {
		DBG(<<"not a digit ");
// bad:
		if(append_error_if_not) {
			Tcl_AppendResult(ip, "Expecting an oid ",
				do_lookup?" or pathname":"", "; got \"", 
				av, "\".", 0);
		}
		return TCL_ERROR;
	}

	DBG(<<"it's a digit - try to interpret as oid");
	// is it more than just a serial #?
	if(strchr(av, '.') != 0) {
		istrstream(av, strlen(av)) >> target;
	} else {
		// just a serial # -- get the volume id from cwd
		target = v->cwd();

		// overwrite with given serial
		istrstream(av, strlen(av)) >> target.serial;
	}
	DBG(<<"got oid " << target);

	yield();
	// ok- we converted it to a serial # -- no guarantee
	// that an object with this serial # exists, but
	// then, that's not a requirement.
	return TCL_OK;
}

int str2iid(
	ClientData clientdata,
	Tcl_Interp* ip, 
	VASPTR v, const char *av, 
	IndexId &target
)
{
	CMDFUNC(str2iid);

	// is it more than just a serial #?
	const char *comma = strchr(av, ',');
	if(comma== 0) {
		Tcl_AppendResult(ip, "Expecting an index id; got \"", 
				av, "\".", 0);
		return TCL_ERROR;
	} 

	//*comma = '\0';  // cannot change, is const
	comma++;
	istrstream(comma, strlen(comma)) >> target.i;

	if(!isdigit(*av)) {
		bool isReg;
		if( !PATH_OR_OID_2_OID(ip, av, target.obj, &isReg, TRUE)) {
			Tcl_AppendResult(ip, "Expecting an index id; got \"", 
				av, "\".", 0);
			return TCL_ERROR;
		}
		// don't care if it's registered 
	} else {
		if(strchr(av,'.') != 0) {
			istrstream(av, strlen(av)) >> target.obj;
		} else {
			// first part is just a serial # -- get the volume id from cwd
			target.obj = v->cwd();

			// overwrite with given serial
			istrstream(av, strlen(av)) >> target.obj.serial;
		}
		DBG(<<"got oid " << target.obj << ","  << target.i);
	}

	yield();
	return TCL_OK;
}
int 
cmd_expect(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_expect);

	DBG(<<"expecting..." << av[1]);
	TxStatus original = Vas->status.txstate;

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

	Tcl_ResetResult(ip);
	
	int i, result, yieldarg=0, expectarg=0;
	CMDResult tclres = tcl_ok;;
	bool fail = false;
	bool aborted = false; // until savepoints
	// expect arg from arg [aborted|noaborted|fail|nofail]

	CHECK(4,5,NULL);
	DBG(<<"expecting..." << av[1]);
	for(i=0; i<ac; i++) {
		if(strcmp(av[i],"from")==0) {
			yieldarg = ++i;
		} else if(strcmp(av[i],"expect")==0) {
			expectarg = ++i;
		} else if(strcmp(av[i],"aborted")==0) {
			aborted=true;
		} else if(strcmp(av[i],"noaborted")==0) {
			aborted=false;
		} else if(strcmp(av[i],"nofail")==0) {
			fail=false;
		} else if(strcmp(av[i],"fail")==0) {
			fail=true;
		}
	}
	DBG(<<"expecting..." << av[1]);
	if((expectarg==0)||(yieldarg==0)) {
		tclout << "Usage: expect {value} from {command} or" 
			<< " from {command} expect {value}" << ends;
		Tcl_AppendResult(ip, tclout.str(), 0);
		return TCL_ERROR;
	}

	DBG(<<"expecting..." << av[1]);
	// todo: suppress printing of vas errors for now
	result =  Tcl_Eval(ip, av[yieldarg]);

	DBG(<<"expecting..." << av[1]);
	tclres = tcl_ok;
	DBG(<<"expecting..." << av[1]);
	if(strcmp(av[1], ip->result)) {
		// always an error if yields unexpected results
		tclout
			<< "COMMAND: " <<  av[yieldarg] << endl
			<< "YIELDED UNEXPECTED: " <<  ip->result << endl
			<< "EXPECTED: " << av[1]  << endl
			<< ends;

		expecterrors++; 
		expect(av[yieldarg], ip->result, av[1]);

		// if this doesn't work- something's really wrong
		tclres =  fail ? tcl_error : tcl_ok;
	}
	// blow away the result -- don't want any output from "expect"
	Tcl_ResetResult(ip);

	if(aborted) {
		if(Vas->status.txstate != Aborting) {
			tclout << "SERVER DIDN'T ABORT AS EXPECTED; command= "
				<<av[yieldarg] <<ends;
		}
		res = cmd_trans(clientdata, g_abort, ip, ac, av);
		if(res!=SVAS_OK) {
			tclout << "ABORT FAILED in EXPECT; command= "
			<< av[yieldarg] << ends;
		}
	} else {
		/* leave it alone */
		if(Vas->status.txstate != original) {
			tclout << "UNEXPECTED TX STATE after " << av[yieldarg] << ends;
		}
		if(original == Active) {
			tid_t __t;
			CALL(trans(&__t));
			if(res!=SVAS_OK) {
				tclout << "UNEXPECTED ABORT from " << av[yieldarg] << ends;
			}
		}
	}
	if(res!=SVAS_OK) {
		expecterrors++; 
		expect(av[yieldarg], ip->result, av[1]);
	}
	Tcl_AppendResult(ip, tclout.str(), 0);
	CMDREPLY(tclres);
} /* cmd_expect */


// for gdb:
extern "C" 
void
expect(
	const char *command,
	const char *result,
	const char *wanted
) 
{
}
