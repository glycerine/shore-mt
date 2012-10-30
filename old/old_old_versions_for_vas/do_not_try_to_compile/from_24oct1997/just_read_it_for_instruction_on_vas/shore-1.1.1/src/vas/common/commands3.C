/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/commands3.C,v 1.19 1995/10/06 16:44:58 nhall Exp $
 */
#include <copyright.h>
#include "shell.misc.h"
#include "vasshell.h"
#include "server_stubs.h"

int
cmd_start(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_start);
	int			qlen;

	CHECK(1, 2, "qlen");
	CHECKCONNECTED;

	if(ac == 2) {
		qlen = _atoi(av[1]);
		if(qlen<0) {
			qlen = 0;
		}
		CALL( start_batch(qlen) );
	} else {
		CALL(start_batch());
	}
	VASERR(res);
}
int
cmd_send(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_send);

	lvid_t lvid;
	lrid_t dir;
	CHECK(1, 1, "");
	CHECKCONNECTED;

	batched_results_list results;
	results.attempts=0;
	results.results=0;
	results.list=0;
	CALL(send_batch(results));

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());
	if(res==SVAS_OK) {
		int i=0;
		DBG(<<"attempts="<<results.attempts);
		DBG(<<"results="<<results.results);
		for(;i<results.results; i++) {
			DBG(<<i <<" request="<<results.list[i].req);
			DBG(<<i <<" status="<<results.list[i].status.vasresult);
			DBG(<<i <<" oid="<<results.list[i].oid);
			tclout << "Req(" << results.list[i].req <<") OK oid="
				<< results.list[i].oid;

		}
		DBG(<<(int)(results.attempts - i)<<" failed");
		for(;i<results.attempts;i++) {
			DBG(<<i <<" status="<<results.list[i].status.vasresult);
			DBG(<<i <<" oid="<<results.list[i].oid);
			tclout << "Req(" << results.list[i].req <<") FAILED oid="
				<< results.list[i].oid 
				<< ::form("%d/%d/%d/%d",
					results.list[i].status.vasresult,
					results.list[i].status.vasreason,
					results.list[i].status.smresult,
					results.list[i].status.smreason);
		}
	}
	tclout << ends;
	dassert(!tclout.bad());
	Tcl_AppendResult(ip, tclout.str(), 0);
	VASERR(res);
}

int
cmd_volroot(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_volroot);

	// newvid 

	lvid_t lvid;
	lrid_t dir;
	CHECK(2, 2, "volid");
	CHECKCONNECTED;

	GET_VID(ip, av[1], lvid);
	CALL(volroot(lvid, &dir));
	Tcl_AppendLoid(ip, res, dir);
	VASERR(res);
}
int
cmd_newvid(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_newvid);

	// newvid 

	lvid_t lvid;
	CHECK(1, 1, "");
	CHECKCONNECTED;
	CALL(newvid(&lvid));
	Tcl_AppendLVid(ip, res, lvid);
	VASERR(res);
}

int
cmd_rmfs(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_rmfs);
	lvid_t lvid;

	// mkfs <lvid>

	CHECK(2, 2, "<lvid>");

	GET_VID(ip, av[1], lvid);

	CHECKCONNECTED;

	CALL(rmfs(lvid));
	VASERR(res);
}

int
cmd_volumes(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_volumes);
    Path 		dev = 0;

	// volumes dev

	CHECK(2, 2, "<device pathname>");

	dev = av[1];
	CHECKCONNECTED;

	int		n	= 30;
	lvid_t	list[n];
	int		nreturned;
	int 	total;
	CALL(volumes(dev, n, list, &nreturned, &total));
	if(res == SVAS_OK) {
		if(nreturned==0) {
			Tcl_AppendResult(ip, "No volumes on ", dev, 0);
		} else  {
			dassert(nreturned <= total);
			dassert(nreturned <= n);
			int i;
			for(i=0; i<nreturned; i++) {
				Tcl_AppendLVid(ip, res, list[i]);
			}
			if(i < total) {
				Tcl_AppendResult(ip, "and more... ", dev, 0);
			}
		}
	}
	VASERR(res);
}

int
cmd_unserve(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_unserve);
    Path 		dev = 0;

	// serve dev

	CHECK(2, 2, "<device path name>");

	dev = av[1];
	CHECKCONNECTED;

	CALL(unserve(dev));
	VASERR(res);
}

int
cmd_serve(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_serve);
    Path 		dev = 0;

	// serve dev

	CHECK(1, 2, "<device pathname>");

	if(ac==1) {
		// just list those served
		// and return
		//
		char 			buf[1000]; // place to put the character strings for 
						// the path names
		typedef char * 	charptr;
#define		LISTLEN 10
		charptr 		*list = new charptr[LISTLEN];
		devid_t	 		*devlist = new devid_t[LISTLEN];
		int 			count;
		bool			more = true;

		while(more) {
			count = LISTLEN;
			CALL(devices(buf, sizeof(buf), list, devlist, &count, &more));
			if(res == SVAS_OK) {
				for(int i=0; i<count; i++) {
					// tclout.seekp(ios::beg);
					// dassert(!tclout.bad());
					//  tclout << devlist[i] << " is " << list[i];
					// tclout << ends;
					// dassert(!tclout.bad());
					// Tcl_AppendElement(ip, tclout.str());

					tclout.seekp(ios::beg);
					dassert(!tclout.bad());
					tclout << devlist[i] << ends;

					char *_argv[2] = { tclout.str(), list[i] };
					char *temp = Tcl_Merge(2, _argv);
					if(temp) {
						Tcl_AppendElement(ip, temp);
						free(temp);
					} else {
						Tcl_AppendElement(ip, "Error in Tcl_Merge");
					}
				}
			}
		}
	} else {
		dev = av[1];
		CHECKCONNECTED;

		CALL(serve(dev));
		if(res != SVAS_OK) {
			Tcl_AppendResult(ip, "Could not serve device ", dev, 0);
		}
	}
	VASERR(res);
}

int
cmd_format(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_format);
    Path 		dev = 0;
	uint4		kb;

	// format dev kb [force|true]

	bool		force = false;
	CHECK(3, 4, "<device pathname> <kilobytes> [force|true] ");

	dev = av[1];
	kb = _atoi(av[2]);
	if(ac==4) {
		if(
			(strcmp(av[3],"force")==0)|| 
			(strcmp(av[3],"true")==0)
		) {
			force = true;
		}
	}

	CHECKCONNECTED;

	CALL(format(dev, kb, force));
	// because this is such a common error,
	// we just print to cerr
	if(res != SVAS_OK) {
		// treat it like it's not a user error
		Vas->perr(cerr, 0, -1, 0, svas_base::ET_VAS);
	}
	VASERR(res);
}

int
cmd_mkfs(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkfs);
    Path 		dev = 0;
	uint4		kb;
	lvid_t		lvid;

	// mkfs dev kb (vid)

	CHECK(3, 4, "<device pathname> <kilobytes> [<lvid>] ");

	dev = av[1];
	kb = _atoi(av[2]);
	if(ac>3) {
		GET_VID(ip, av[3], lvid);
	}

	CHECKCONNECTED;

	DBG(<<"mkfs " << dev <<" kb="<< kb <<" lvid="<< lvid );
	CALL(mkfs(dev, kb, lvid, &lvid));
	if(res == SVAS_OK) {
		Tcl_AppendLVid(ip, res, lvid);
	}
	VASERR(res);
}

int
cmd_dismount(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_dismount);

	CHECK(2, 2, NULL);
	CHECKCONNECTED;
	CALL(dismount(av[1]));
	VASERR(res);
}

int
cmd_patch(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_patch);
	lvid_t		lvid;

	CHECK(2, 2, NULL);
	CHECKCONNECTED;

	if(isdigit(*av[1])) {
		GET_VID(ip, av[1], lvid);
		CALL(punlink(lvid));
	} else {
		CALL(punlink(av[1]));
	}
	VASERR(res);
}

int
cmd_quota(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_quota);
	lvid_t		lvid;

	CHECK(2, 2, NULL);
	CHECKCONNECTED;
	smksize_t	q, u;

	if(isdigit(*av[1])) {
		GET_VID(ip, av[1], lvid);
		CALL(quota(lvid, &q, &u));
		if(res==SVAS_OK) {
			tclout.seekp(ios::beg);
			dassert(!tclout.bad());
			Tcl_AppendResult(ip, ::form("%d %d", q, u), 0);
		}
	} else {
		Tcl_AppendResult(ip, "Usage: quota volid", 0);
	}
	VASERR(res);
}

int
cmd_getoption(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_getoption);
	CHECK(2, 2, NULL);
	CHECKCONNECTED;
	const char *x;
	CALL(option_value(av[1], &x));
	if(res== SVAS_OK) {
		Tcl_AppendResult(ip, x, 0);
	} 
	VASERR(res);
}

int
cmd_suppress(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_suppress);
	// optional argument is on|off
	// no arg means just print the value
	CHECK(1, 2, NULL);
	CHECKCONNECTED;
	if(ac==2) {
		const char *x = av[1];
		if(strcmp(x,"no")==0 || strcmp(x,"off")==0) {
			(void) Vas->un_suppress_p_user_errors();
		} else if(strcmp(x,"on")==0 || strcmp(x,"yes")==0) {
			(void) Vas->suppress_p_user_errors();
		}
	} else {
		bool b = Vas->suppress_p_user_errors();

		if(b) {
			// was on, still on
			Tcl_AppendResult(ip, "on",0);
		} else {
			// was off but turned on, so turn it back off
			(void) Vas->un_suppress_p_user_errors();
			Tcl_AppendResult(ip, "off",0);
		}
	}
	VASERR(res);
}

int	
cmpstr(const void *a, const void *b) 
{
	FUNC(cmpstr);
	DBG(<<"cmpstr a= " << ::hex((unsigned int)a)
		<< "string is " << (char *)a
		<< " b= " << ::hex((unsigned int)b)
		<< "string is " << (char *)b
		);
	return strcasecmp((char *)a,(char *)b);
}

int
cmd_lock_timeout(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_lock_timeout);

	CHECK(1, 2, NULL);
	CHECKCONNECTED;

	locktimeout_t l,orig,prior;
	if(ac>1) {
		if(isalpha(*av[1])) {
			if(strcmp(av[1],"immediate")==0) {
				l = 0;
			} else if(strcmp(av[1],"forever")==0) {
				l = -1;
			} else {
				Tcl_AppendResult(ip, "bad value for timeout", 0);
				return TCL_ERROR;
			}
		} else {
			l = _atoi(av[1]);
		}
	} else {
		l = 0;
	}
	DBG(<<"setting to " << l );
	CALL(lock_timeout(l, &orig));
	DBG(<<"old value was " << orig );
	if(res == SVAS_OK) {
		if(ac==1) {
			// return the original value
			switch(orig) {
			case -1:
				Tcl_AppendResult(ip, "forever", 0);
				break;
			case 0:
				Tcl_AppendResult(ip, "immediate", 0);
				break;
			default:
				Tcl_AppendResult(ip, ::form("%d", orig), 0);
				break;
			}

			// set it back to whatever it was,
			// if necessary
			if(orig != l) {
				DBG(<<"setting to " << orig );
				CALL(lock_timeout(orig, &prior));
				DBG(<<"value was " << prior );
				dassert(l == prior);
			}
#ifdef DEBUG
			CALL(lock_timeout(orig, &prior));
			assert(prior == orig);
#endif
		} else {
			// return the new value
			switch(l) {
			case -1:
				Tcl_AppendResult(ip, "forever", 0);
				break;
			case 0:
				Tcl_AppendResult(ip, "immediate", 0);
				break;
			default:
				Tcl_AppendResult(ip, ::form("%d", orig), 0);
				break;
			}
#ifdef DEBUG
			CALL(lock_timeout(l, &prior));
			assert(prior == l);
#endif
		}
		return TCL_OK;
	}

	VASERR(res);
}
