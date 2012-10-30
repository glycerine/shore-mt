/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/commandsg.C,v 1.14 1997/09/19 11:55:37 solomon Exp $
 */
#include <copyright.h>
#include <iomanip.h>
#include "shell.misc.h"
#include "server_stubs.h"

int
cmd_debugflags(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_debugflags);
	char *f;

	CHECK(1,2,NULL);

	f = getenv("DEBUG_FILE");
	if(ac>1) {
		if(strcmp(av[1],"off")==0) {
			av[1] = "";
		} 
		 _debug.setflags(av[1]);
	} else {
		Tcl_AppendResult(ip,  _debug.flags(), 0);
	}
	 if(f) {
		Tcl_AppendResult(ip,  "NB: written to file: ",f,".", 0);
	 }
	return TCL_OK;
}


int
cmd_shriek(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	return Tcl_Eval(ip, "history redo -1");
}


#include <errlog.h>

void
sll(ErrLog *e, void *arg)
{
	LogPriority *level = (LogPriority *) arg;

	e->setloglevel(*level);
}

void
pll(ErrLog *e, void *arg)
{
	Tcl_Interp *ip = (Tcl_Interp *) arg;

	const char *c  = e->getloglevelname();
	Tcl_AppendResult(ip, e->ident(), " ", c, " ", 0);
}

int
cmd_log(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_log);
	int i;
	ErrLog *which=0;
	bool found_which=false;
	bool found_level=false;

	LogPriority level = log_none;
	int 	all = 0, none=0;
	const char 	*ident = "unknown";

	// for now, you can only change the logging level
	// -- you cannot redirect the logging

	if(ac == 1) {
		all = 1;
		// print only
	} else  {
		// ac > 1
		{
			i=1; // log name

			const char *which_name = av[i];
			DBG(<<"i=" << i << " av[i]=" << av[i]);

			//
			// handle a few special cases
			//
			if( (strcmp(av[i], "?")==0) || 
				(strcmp(av[i], "help")==0)) {

				ErrLog::apply(pll, ip);
				return TCL_OK;
			} else 
			if(strcmp(av[i], "remote")==0) {
				which_name = "svas-remote";
			} else
			if(strcmp(av[i], "client")==0) {
				which_name = "svas-client";
			} else
			if(strcmp(av[i], "shore")==0) {
				which_name = "svas-client";
			} else
			if(strcmp(av[i], "all")==0) {
				all++;
				which_name = 0;
			} else
			if(strcmp(av[i], "none")==0) {
				none++;
				which_name = 0;
			} else
			if(strcmp(av[i], "off")==0) {
				all++;
				found_level = true;
				level = log_none;
			} else
			if(strcmp(av[i], "on")==0) {
				all++;
				found_level = true;
				level = log_info;
			} else {
				which_name = av[i];
			}
			DBG(<<"looking for log " << av[i] 
				<< " a.k.a " << which_name);

			if(which_name) {
				which = ErrLog::find(which_name);
				if(which) {
					DBG(<<"found " << which_name );
					ident = which_name;
					found_which=true;
				} else {
					DBG(<<"not found " << which_name );
					Tcl_AppendResult(ip, "No such log: ", av[i], 0);
					return TCL_ERROR;
				}
			}
		} 
		if(ac > 2)  {
			i = 2;
			DBG(<<"i=" << i << " av[i]=" << av[i]);

			bool ok=true;
			level = ErrLog::parse(av[i], &ok);
			if(ok) {
				found_level = true;
				DBG(<< " found logging level " << level);
			} else {
				Tcl_AppendResult(ip, "No such log level: ", av[i], 0);
				return TCL_ERROR;
			}
		}
	}
#ifdef DEBUG
	DBG(<<"all=" << all
		<< " none=" << none
		<< " found_which=" << found_which
		<< " which=" << ::hex((unsigned int) which)
		<< " ident=" << ident

		<< " found_level=" << found_level
		<< " level=" << level
	);
	if(found_which) {
		dassert(which !=0);
	}
	if(which !=0){
		dassert(found_which);
	}
	if(which !=0 ) {
		dassert(ident && strcmp(ident,"unknown"));
	}

#endif

	if(none) {
		// turn off all logging
		all++;
		level = log_none;
		found_level = true;
	}


	if(all) {
		// inspect or change level for all logs
		if(found_level) {
			ErrLog::apply(sll, &level);
			// apply level to all logs
		}
		// inspect all logs
		DBG(<<"apply pll");
		ErrLog::apply(pll, ip);
	} else {
		/* 
		if( !(which = ErrLog::find(ident)) ) {
			Tcl_AppendResult(ip, "No such log: ", ident, 0);
			return TCL_ERROR;
		} else if (found_level) {
			which->setloglevel(level);
		}
		*/

		if(found_which) {
			if(found_level) {
				DBG(<<"setloglevel");
				which->setloglevel(level);
			}
			DBG(<<"apply pll");
			pll(which, ip);
		} 
	}
	return TCL_OK;
}

int
cmd_getuid(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_getuid);

	CHECK(1,2,NULL);
	uid_t u;
	if(ac == 1) {
		u = getuid();
	} else {
		u = uname2uid(av[1]);
	}
	Tcl_AppendResult(ip,  ::form("%d",u), 0);
	return TCL_OK;
}

int
cmd_getgid(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_getgid);

	CHECK(1,2,NULL);
	gid_t g;
	if(ac == 1) {
		g = getgid();
	} else {
		g = gname2gid(av[1]);
	}
	Tcl_AppendResult(ip,  ::form("%d",g), 0);
	return TCL_OK;
}


extern unix_stats &shell_rusage;

int 
cmd_cstat(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_cstat);

	CHECK(1,1,NULL);

	clear_statistics(clientdata); 
	shell_rusage.stop();
	shell_rusage.start();

	return TCL_OK;
}

int 
cmd_pstat(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_pstat);

	CHECK(1,1,NULL);

	print_statistics(clientdata, ip, statistics_mode); // print them now

	return TCL_OK;
}


// int cmd_expect() has to be done on a per-shell basis

int 
cmd_permit(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_permit);

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

	Tcl_ResetResult(ip);
	
	int 		i, result;
	CMDResult 	tclres = tcl_ok;;

	CHECK(4,4,NULL);
	if( (strcmp(av[0],"permit")==0) 
		&&
		(strcmp(av[2],"from")==0) 
	) {
	} else {
		tclout << "Usage: permit {value} from {command}" << ends;
		Tcl_AppendResult(ip, tclout.str(), 0);
		return TCL_ERROR;
	}

	// todo: suppress printing of vas errors for now
	DBG(<<"evaluating " << av[3]);
	result =  Tcl_Eval(ip, av[3]);

	DBG(<<"permit: result = " << result << ends);
	tclres = tcl_ok;
	if(result != TCL_OK) {
		//
		// strstr(s1,s2) return NULL if s2 does not occur in s1
		//
		if(strstr(av[1], ip->result)==0) {
			// always an error if yields unexpected results
			tclout
				<< "COMMAND: " <<  av[3] << endl
				<< "YIELDED UNEXPECTED: " <<  ip->result << endl
				<< "EXPECTED: " << av[1]  << endl;

			expecterrors++; 

			// for gdb:
			// expect(av[3], ip->result, av[1]);

			// if this doesn't work- something's really wrong
			tclres =  tcl_error; 
		}
		// want result to be passed along
		// Tcl_ResetResult(ip);
	}
	tclout << ends;
	// Tcl_AppendResult(ip, tclout.str(), 0);
	DBG(<<"result from permit is " << ip->result);
	CMDREPLY(tclres);
} /* cmd_permit */

int
cmd_not(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_not);
	int 	boo = FALSE;
	int		res;

	CHECK(2,2,NULL);

	DBG(<<"about to eval " << av[1]);
	res = Tcl_ExprBoolean(ip, av[1], &boo);
	if(res == TCL_OK) {
		// replace the result with its reverse sense
		Tcl_ResetResult(ip);
		Tcl_AppendResult(ip, boo?"0":"1",0);
		DBG(<< "cmd_not returns " 
			<< (int)(boo?0:1));
	}
	CMDREPLY(res==TCL_OK?tcl_ok:tcl_error);
} /* cmd_not */

int
cmd_errmsg(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_errmsg);
	unsigned int y;
	CHECK(2,2,NULL);

	if(isdigit(*av[1])) {
		y = (unsigned int) _atoi(av[1]);
		DBG(<<"y is " << y);
		Tcl_AppendResult(ip, svas_base::err_msg(y), 0);
	} else  { // it's a character string not looking like a number
		Tcl_AppendResult(ip, svas_base::err_msg(av[1]), 0);
	}
	CMDREPLY(tcl_ok);
}

int
cmd_errname(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_errname);
	unsigned int 	y;
	bool			found;
	const char		*name;
	CHECK(2,2,NULL);

	if( ! isdigit(*av[1])) {
		y = svas_base::err_code(av[1]);
	} else {
		y = (unsigned int) _atoi(av[1]);
	}
	DBG(<<"y is " << y);
	found = svas_base::err_name(y, name);
	DBG(<<"found is " << found);
	if(found) {
		Tcl_AppendResult(ip, name, 0);
	} else {
		Tcl_AppendResult(ip, av[1], 
			" is not a legitimate vas error code.",0);
	}
	CMDREPLY(tcl_ok);
}

int
cmd_errcode(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_errcode);
	unsigned int y;
	CHECK(2,2,NULL);

	if(isdigit(*av[1])) {
		y = (unsigned int) _atoi(av[1]);
	} else  {
		y = svas_base::err_code(av[1]);
	}
	DBG(<<"y is " << y);
	Tcl_AppendResult(ip, ::form("0x%x", y),  0);
	CMDREPLY(tcl_ok);
}

int
cmd_cerr(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
// err puts the args to stderr
{
	FUNC(cmd_cerr);
    for (int i = 1; i < ac; i++) {
		cerr << av[i] << " ";
    }
	cerr << endl;
    return(TCL_OK);
}

int
cmd_cout(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
// cout puts the args to "the tty"/"the screen"/stdout
{
	FUNC(cmd_cout);
    for (int i = 1; i < ac; i++) {
		cout << av[i] << " ";
    }
	cout << endl;
    return(TCL_OK);
}

int
cmd_echo(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
// echo puts the args into the result 
{
	FUNC(cmd_echo);
    for (int i = 1; i < ac; i++) {
		Tcl_AppendResult(ip, av[i], " ", 0);
    }
    return(TCL_OK);
}

int
cmd_sleep(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    CHECK(1, 2, NULL);
	int secs=1;
	if(ac > 1) {
		secs = _atoi(av[1]);
	}
	sleep(secs);
	Tcl_AppendResult(ip, ::form("%d",secs), 0);
	return TCL_OK;
}
#if !defined(Linux)
extern "C" {
	long random();
	int	 srandom(unsigned long);
	char *initstate(unsigned long, char *, int);
	char *setstate(char *);
}
#endif
int rseed=1;
char rstate[32]= {
    0x03, 0xab, 0x38, 0xd0,
    0x76, 0x4, 0x24, 0x2c,
    0x76, 0x40, 0x24, 0x2c,
    0x03, 0xab, 0x38, 0xd0,
    0xab, 0xed, 0xf1, 0x23,
    0xab, 0xed, 0xf1, 0x23,
    0x03, 0x00, 0x08, 0xd0,
    0x01, 0x00, 0x38, 0xd0
};


int
cmd_random(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_random);
	int mod = -1;
	if(ac == 2) {
		mod = _atoi(av[1]);
		DBG(<<"random modulus " << mod);
	}
	if(mod == 0) {
		Tcl_AppendResult(ip, "Bad argument to random:", av[1],  0);
		return TCL_ERROR;
	}
	long res = (int) random();
	if(mod ==0) {
		(void) initstate(rseed, rstate, sizeof(rstate));
	}
	if(mod>0) {
		res %= mod;
	}
	Tcl_AppendResult(ip, ::form("%d",res), 0);
	return TCL_OK;
}



int
cmd_assert(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_assert);
	int 	res;
	int		boo = FALSE;

	if(ac == 2) {
		res = Tcl_ExprBoolean(ip, av[1], &boo);
		if(res == TCL_OK) {
			if(boo) {
				return TCL_OK;
			} else {
				Tcl_AppendResult(ip,  "ASSERTION FAILURE: ",
					av[1], 0);
			}
		} else {
			Tcl_AppendResult(ip, 
				"FAILURE EVALUATING ASSERT: ", av[1], 0);
		}
	} else {
		Tcl_AppendResult(ip, 
			"Wrong number of arguments to assert:",
			::form("%d",ac), "Expected one.", 0);
		for(int i=0; i<ac; i++) {
			Tcl_AppendResult(ip, 
			"argv[", 
			::form("%d",i),
			"]=", av[i], 0);
		}
	}
	return TCL_ERROR; 
}

int
cmd_bye(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_bye);
#ifdef QUANTIFY
	if(quantify_is_running()) {
		quantify_save_data();
	}
#endif
	noprompt();
	quit_shell(ip);
	// never get here
	return SVAS_OK; 
}


int 
cmd_time(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_time);
	int res;
	int iter = 1, i;
	unix_stats *s = new unix_stats;

	CHECK(2, 3, NULL);

	if(ac>2) {
		iter = _atoi(av[2]);
	}
	tclout.seekp(ios::beg);
	tclout <<  "Timing command " << av[1] << " " <<  iter << " times." << endl;

	clear_statistics(clientdata);
	s->start();
	for(i=0; i<iter; i++) {
		if((res = Tcl_Eval(ip, av[1])) != TCL_OK) break;
	}
	if(i>0) {
		s->stop(i);

		tclout << *s  << endl;

#define MILLION 1000000.0
		int j;
		j = s->clocktime();
		if(j>0) {
			tclout << ::form("%f", MILLION*i/j) << " iter/sec clock " << endl;
		}
		j = s->systime()+ s->usertime();
		if(j>0) {
			tclout << ::form("%f", MILLION*i/j) << " iter/sec u+sys " << endl;
		}
	} else {
		s->stop(1);
		tclout << "error : i=" << i << " iter=" << iter << endl;
	}
	tclout << ends;
	Tcl_AppendResult(ip, tclout.str(), 0);

	CMDREPLY(tcl_ok);
}

int
cmd_info(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	FUNC(cmd_info);
	CHECK(2, 2, NULL);

	const char *x = av[1];

	if(Tcl_VarEval(ip, "_info ", x, 0)!=TCL_OK) {
		// just let it return Tcl's error
		return TCL_OK;
		// catastrophic( ::form("Fatal error invoking Tcl info command (renamed _info)."));
	}
	DBG(<<"info " << x);
	if(	strlen(x)>1 
		&& 
		(strncmp(x,"commands",strlen(x))==0)
	) {
		DBG(<<"have to sort " << x);
		// Sort the result
		char 	**cmdlist;
		int 	numcmd=0;
		if(Tcl_SplitList(ip, ip->result, &numcmd, &cmdlist)!=SVAS_OK) {
			free((char *)cmdlist);
			Tcl_AppendResult(ip, 
				"Could not sort result of \"_info command\"", 0);
			return TCL_ERROR;
		}
#ifdef notdef

#ifdef DEBUG
		cerr <<"Before split, result=" <<  ip->result << endl;
		int i;
		cerr <<"After split, cmdlist is" 
			<<  ::hex((unsigned int)cmdlist) 
			<< " *cmdlist is " 
			<<  ::hex((unsigned int)*cmdlist) << endl;
		for(i=0; i<numcmd; i++) {
			cerr << "cmdlist[" << i << "] @ "
				<<  ::hex((unsigned int)cmdlist[i]) 
				<< " = " << cmdlist[i] << endl;
		}
#endif

		// TODO get this working
		// qsort(*cmdlist,numcmd,sizeof(char *),cmpstr);

#ifdef DEBUG
		cerr <<"After sort, cmdlist is" 
			<<  ::hex((unsigned int)cmdlist) 
			<< " *cmdlist is " 
			<<  ::hex((unsigned int)*cmdlist) << endl;
		for(i=0; i<numcmd; i++) {
			cerr << "cmdlist[" << i << "] @ " 
				<<  ::hex((unsigned int)cmdlist[i]) 
				<< " = " << cmdlist[i] << endl;
		}
#endif
		char *merged = Tcl_Merge(numcmd, cmdlist);
		if(!merged) {
			Tcl_AppendResult(ip, 
				"Could not merge result of sort", 0);
			return TCL_ERROR;
		}
		Tcl_ResetResult(ip);
		Tcl_AppendResult(ip, merged, 0);
		free(merged);
#endif /*notdef*/
		free((char *)cmdlist);
	}
	return TCL_OK;
}
