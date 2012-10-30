/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: tcl_thread.cc,v 1.84 1997/06/15 10:30:28 solomon Exp $
 */
#define TCL_THREAD_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include <stream.h>
#include <string.h>
#include <iomanip.h>
#include <limits.h>
#include <tcl.h>
#include <debug.h>
#include <sm_vas.h>
#include "tcl_thread.h"
#include "ssh.h"
#include "ssh_random.h"

#include <crash.h>

#ifdef USE_COORD
#include <sm_global_deadlock.h>
extern CentralizedGlobalDeadlockServer* globalDeadlockServer;
#endif

#include <sys/time.h>
#if !defined(SOLARIS2) && !defined(AIX41)
extern "C" {
	int gettimeofday(struct timeval *tv, struct timezone *tz);
}
#endif

#ifdef __GNUG__
template class w_list_i<tcl_thread_t>;
template class w_list_t<tcl_thread_t>;
#endif

// an unlikely naturally occuring error string which causes the interpreter loop to
// exit cleanly.
static char *TCL_EXIT_ERROR_STRING = "!@#EXIT#@!";


bool tcl_thread_t::allow_remote_command = false;
int tcl_thread_t::count = 0;
char* tcl_thread_t::inter_thread_comm_buffer = 0;

ss_m* sm = 0;

extern sm_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
#ifdef USE_COORD
extern co_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
#endif

#ifdef USE_VERIFY
extern ovt_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
#endif

extern char* tcl_init_cmd;

w_list_t<tcl_thread_t> tcl_thread_t::list(offsetof(tcl_thread_t, link));

// For debugging
extern "C" void tcl_assert_failed();
void tcl_assert_failed() {}

static int
t_debugflags(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2 && ac != 1) {
	Tcl_AppendResult(ip, "usage: debugflags [arg]", 0);
	return TCL_ERROR;
    }
    char *f;
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
static int
t_assert(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: assert [arg]", 0);
	return TCL_ERROR;
    }
    int boo = 0;
    int	res = Tcl_ExprBoolean(ip, av[1], &boo);
    if(res == TCL_OK && boo==0) {
	// assertion failure
	// A place to put a gdb breakpoint!!!
	tcl_assert_failed();
    }
    Tcl_AppendResult(ip, ::form("%d",boo), 0);
    cout << flush;
    return res;
}


static int
t_timeofday(ClientData, Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (ac > 1) {
	Tcl_AppendResult(ip, "usage: timeofday", 0);
	return TCL_ERROR;
    }
    struct timeval tv;
    if(gettimeofday(&tv, 0) < 0) {
	Tcl_AppendResult(ip, "Error calling gettimeofday", 0);
	return TCL_ERROR;
    }
    Tcl_AppendResult(ip, ::form("%d %d",tv.tv_sec, tv.tv_usec), 0);

    return TCL_OK;
}
static int
t_allow_remote_command(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: allow_remote_command [off|no|false|on|yes|true]", 
		0);
	return TCL_ERROR;
    }
    const char *c = av[1];

    if(strcmp(c, "off")==0 ||
	strcmp(c, "no")==0 ||
        strcmp(c, "false")==0) {
	tcl_thread_t::allow_remote_command = false;
    } else if(strcmp(c, "on")==0 ||
	strcmp(c, "yes")==0 ||
        strcmp(c, "true")==0) {
	tcl_thread_t::allow_remote_command = true;
    }
    return TCL_OK;
}

#if !defined(USE_SSMTEST) 
#define av
#endif
static int
t_debuginfo(ClientData, Tcl_Interp* ip, int ac, char* av[])
#if !defined(USE_SSMTEST) 
#undef av
#endif
{
    if (ac != 4) {
	Tcl_AppendResult(ip, "usage: debuginfo category v1 v2", 0);
	return TCL_ERROR;
    }
#if defined(USE_SSMTEST) 
    const char *v1 = av[2];
    int v2 = atoi(av[3]);
    debuginfo_enum d = debug_delay;
    /*
     * categories: delay, crash, abort, yield
     * v1  is string
     * v2  is int
     *
     * Same effect as setting environment variables 
     * (e.g.)CRASHTEST CRASHTESTVAL
     */
    if(strcmp(av[1], "delay")==0) {
	// v1: where, v2: time
	d = debug_delay;
    } else
    if(strcmp(av[1], "crash")==0) {
	if(! tcl_thread_t::allow_remote_command) return TCL_OK;

	// v1: where, v2: nth-time-through
	d = debug_crash;
    } else
    if(strcmp(av[1], "none")==0) {
	v1 = "none";
	v2 = 0;
	setdebuginfo(debug_delay, v1, v2);
	d = debug_crash;
    }
    setdebuginfo(d, v1, v2);
    return TCL_OK;
#else
    Tcl_AppendResult(ip, "simulate_preemption (USE_SSMTEST) not configured", 0);
    return TCL_ERROR;
#endif
}

static int
t_write_random(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: write_random filename", 0);
	return TCL_ERROR;
    }
    if(ac == 2) {
    	generator.write(av[1]);
    }
    return TCL_OK;
}

static int
t_read_random(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: read_random filename", 0);
	return TCL_ERROR;
    }
    if(ac == 2) {
    	generator.read(av[1]);
    }
    return TCL_OK;
}

static int
t_random(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac > 2) {
	Tcl_AppendResult(ip, "usage: random [modulus]", 0);
	return TCL_ERROR;
    }
    int mod;
    if(ac == 2) {
	mod = atoi(av[1]);
    } else {
	mod = -1;
    }
    // long res = generator.mrand();
    unsigned int res = generator.lrand(); // return only unsigned
    if(mod==0) {
	/* initialize to a given, known, state */
	generator.srand(0);
    } else if(mod>0) {
	res %= mod;
    }
    Tcl_AppendResult(ip, ::form("%d",res), 0);

    return TCL_OK;
}

static int
t_fork_thread(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac < 3)  {
	Tcl_AppendResult(ip, "usage: ", av[0], " proc args", 0);
	return TCL_ERROR;
    }

    char* old_result = strdup(ip->result);
    w_assert1(old_result);

    tcl_thread_t* p = 0;
    p = new tcl_thread_t(ac - 1, av + 1, ip, false);
    if (!p) {
	Tcl_AppendResult(ip, "cannot create thread", 0);
	return TCL_ERROR;
    }

    rc_t e = p->fork();
    if (e != RCOK) {
	    delete p;
	    Tcl_AppendResult(ip, "cannnot start thread", 0);
	    return TCL_ERROR;
    }
    
    Tcl_SetResult(ip, old_result, TCL_DYNAMIC);

    char buf[20];
    ostrstream s(buf, sizeof(buf));
    s << p->id << '\0';
    Tcl_AppendResult(ip, buf, 0);
    
    return TCL_OK;
}

static int
t_sync(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 1) {
	Tcl_AppendResult(ip, "usage: ", av[0], 0);
	return TCL_ERROR;
    }

    tcl_thread_t::sync();

    return TCL_OK;
}

static int
t_sync_thread(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac == 1)  {
	Tcl_AppendResult(ip, "usage: ", av[0],
			 " thread_id1 thread_id2 ...", 0);
	return TCL_ERROR;
    }

    for (int i = 1; i < ac; i++)  {
	tcl_thread_t::sync_other(strtol(av[i], 0, 10));
    }

    return TCL_OK;
}

static int
t_join_thread(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac == 1)  {
	Tcl_AppendResult(ip, "usage: ", av[0],
			 " thread_id1 thread_id2 ...", 0);
	return TCL_ERROR;
    }

    for (int i = 1; i < ac; i++)  {
	tcl_thread_t::join(strtol(av[i], 0, 10));
    }

    return TCL_OK;
}

static int
t_yield(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 1)  {
	Tcl_AppendResult(ip, "usage: ", av[0], 0);
	return TCL_ERROR;
    }

    me()->yield();

    return TCL_OK;
}

static int
t_link_to_inter_thread_comm_buffer(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2)  {
	Tcl_AppendResult(ip, "usage: ", av[0], "variable", 0);
	return TCL_ERROR;
    }

    return Tcl_LinkVar(ip, av[1], (char*)&tcl_thread_t::inter_thread_comm_buffer, TCL_LINK_STRING);

    //return TCL_OK;
}

static int
t_exit(ClientData, Tcl_Interp *ip, int ac, char* av[])
{
    int e = (ac == 2 ? atoi(av[1]) : 0);
    cout << flush;
    if (e == 0)  {
	Tcl_SetResult(ip, TCL_EXIT_ERROR_STRING, TCL_STATIC);
	return TCL_ERROR;  // interpreter loop will catch this and exit
    }  else  {
	_exit(e);
    }
}


/*
 * This is a hacked version of the tcl7.4 'time' command.
 * It uses the shore-native 'stime' support to print
 * "friendly" times, instead of
 *      16636737373 microseconds per iteration
 */

static int t_time(ClientData, Tcl_Interp *interp,int argc, char **argv)
{
    int count, i, result;
    stime_t start, stop;

    if (argc == 2) {
	count = 1;
    } else if (argc == 3) {
	if (Tcl_GetInt(interp, argv[2], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" command ?count?\"", (char *) NULL);
	return TCL_ERROR;
    }
    start = stime_t::now();

    for (i = count ; i > 0; i--) {
	result = Tcl_Eval(interp, argv[1]);
	if (result != TCL_OK) {
	    if (result == TCL_ERROR) {
		char msg[60];
		sprintf(msg, "\n    (\"time\" body line %d)",
			interp->errorLine);
		Tcl_AddErrorInfo(interp, msg);
	    }
	    return result;
	}
    }

    stop = stime_t::now();

    Tcl_ResetResult(interp);	/* The tcl version does this. ??? */
    char	buf[128];
    ostrstream s(buf, sizeof(buf));

    if (count > 0) {
	    sinterval_t timePer(stop - start);
	    sinterval_t timeEach(timePer / count);;
	    s << timeEach;
    }
    else
	    s << "0";
    s << " seconds per iteration" << ends;

    Tcl_AppendResult(interp, buf, 0);

    return TCL_OK;
}

static int
t_echo(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    for (int i = 1; i < ac; i++) {
	cout << ((i > 1) ? " " : "") << av[i];
	Tcl_AppendResult(ip, (i > 1) ? " " : "", av[i], 0);
    }
    cout << endl << flush;

    return TCL_OK;
}

#ifdef UNDEF
static int
t_verbose(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    extern int verbose;
    
    if (!verbose)
	return TCL_OK;
    for (int i = 1; i < ac; i++) {
	cout << ((i > 1) ? " " : "") << av[i];
	Tcl_AppendResult(ip, (i > 1) ? " " : "", av[i], 0);
    }
    cout << endl << flush;

    return TCL_OK;
}
#endif /*UNDEF*/


/*
 * This is from Henry Spencer's Portable string library, which
 * is in the public domain.  Bolo changed it to have a per-thread
 * context, so safe_strtok() can be used safely in a multi-threaded
 * environment.
 *
 * To use safe_strtok(), set with *scanpoint = NULL when starting off.
 * After that, it will be maintained normally.
 *
 * Get next token from string s (NULL on 2nd, 3rd, etc. calls),
 * where tokens are nonempty strings separated by runs of
 * chars from delim.  Writes NULs into s to end tokens.  delim need not
 * remain constant from call to call.
 *
 * XXX this may be something to insert into the fc or common
 * directories, as it is a problem wherever threads use strtok.
 */

static char *safe_strtok(char *s, const char *delim, char *&scanpoint)
{
	char *scan;
	char *tok;
	const char *dscan;

	if (s == NULL && scanpoint == NULL)
		return(NULL);
	if (s != NULL)
		scan = s;
	else
		scan = scanpoint;

	/*
	 * Scan leading delimiters.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0'; dscan++)
			if (*scan == *dscan)
				break;
		if (*dscan == '\0')
			break;
	}
	if (*scan == '\0') {
		scanpoint = NULL;
		return(NULL);
	}

	tok = scan;

	/*
	 * Scan token.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0';)	/* ++ moved down. */
			if (*scan == *dscan++) {
				scanpoint = scan+1;
				*scan = '\0';
				return(tok);
			}
	}

	/*
	 * Reached end of string.
	 */
	scanpoint = NULL;
	return(tok);
}


static void grab_vars(Tcl_Interp* ip, Tcl_Interp* pip)
{
    Tcl_Eval(pip, "info globals");

    char	*result = strdup(pip->result);
    w_assert1(result);

    char	*last = result + strlen(result);
    char	*context = 0;
    char	*p = safe_strtok(result, " ", context);

    while (p)  {
	char* v = Tcl_GetVar(pip, p, TCL_GLOBAL_ONLY);
	if (v)  {
	    Tcl_SetVar(ip, p, v, TCL_GLOBAL_ONLY);
	    p = safe_strtok(0, " ", context);
	} else {
	    Tcl_VarEval(pip, "array names ", p, 0);
	    char* s = safe_strtok(pip->result, " ", context);
	    while (s)  {
		v = Tcl_GetVar2(pip, p, s, TCL_GLOBAL_ONLY);
		if (v)  {
		    Tcl_SetVar2(ip, p, s, v, TCL_GLOBAL_ONLY);
		}
		s = safe_strtok(0, " ", context);
		if (s)  *(s-1) = ' ';
	    }
	    p[strlen(p)] = ' ';
	    *last = '\0';
	    p = safe_strtok(p, " ", context);
	    if (p != result) *(p-1) = ' ';
	    p = safe_strtok(0, " ", context);
	}
	if (p) {
	    assert(*(p-1) == '\0');
	    *(p-1) = ' ';
	}
    }
    free(result);
}

static void grab_procs(Tcl_Interp* ip, Tcl_Interp* pip)
{
    Tcl_DString buf;
    Tcl_DStringInit(&buf);
    
    Tcl_Eval(pip, "info procs");
    Tcl_Eval(ip, "info procs");
    if (pip->result[0])  {
        char *context = 0;
	char* procs = strdup(pip->result);
	w_assert1(procs);

	for (char* p = safe_strtok(procs, " ", context);
	     p;   p = safe_strtok(0, " ", context))  {
	    char line[1000];
	    {
		ostrstream s(line, sizeof(line));
		s << "info proc " << p << '\0';
		Tcl_Eval(ip, line);
		if (!ip->result || strcmp(ip->result, p) == 0)  {
		    // already have this proc
		    continue;
		}
	    }

	    Tcl_DStringAppend(&buf, "proc ", -1);
	    Tcl_DStringAppend(&buf, p, -1);
	    {
		ostrstream s(line, sizeof(line));
		s << "info args " << p << '\0';
		Tcl_Eval(pip, line);
		Tcl_DStringAppend(&buf, " { ", -1);
		Tcl_DStringAppend(&buf, pip->result, -1);
		Tcl_DStringAppend(&buf, " } ", -1);
	    }
	    {
		ostrstream s(line, sizeof(line));
		s << "info body " << p << '\0';
		Tcl_Eval(pip, line);
		Tcl_DStringAppend(&buf, " { ", -1);
		Tcl_DStringAppend(&buf, pip->result, -1);
		p = Tcl_DStringAppend(&buf, " } \n", -1);
		assert(p);
		assert(Tcl_CommandComplete(p));
		
		int result = Tcl_RecordAndEval(ip, p, 0);
		if (result != TCL_OK)  {
		    cerr << "grab_procs(): Error" ;
		    if (result != TCL_ERROR) cerr << " " << result;
		    if (ip->result[0]) 
			cerr << ": " << ip->result;
		    cerr << endl;
		}

		assert(result == TCL_OK);
	    }
        }
	free(procs);
    }
    Tcl_DStringFree(&buf);
}

static void create_stdcmd(Tcl_Interp* ip)
{
    Tcl_CreateCommand(ip, "debugflags", t_debugflags, 0, 0);
    Tcl_CreateCommand(ip, "__assert", t_assert, 0, 0);
    Tcl_CreateCommand(ip, "timeofday", t_timeofday, 0, 0);
    Tcl_CreateCommand(ip, "random", t_random, 0, 0);
    Tcl_CreateCommand(ip, "read_random", t_read_random, 0, 0);
    Tcl_CreateCommand(ip, "write_random", t_write_random, 0, 0);
    Tcl_CreateCommand(ip, "debuginfo", t_debuginfo, 0, 0);
    Tcl_CreateCommand(ip, "allow_remote_command", t_allow_remote_command, 0, 0);

    Tcl_CreateCommand(ip, "fork_thread", t_fork_thread, 0, 0);
    Tcl_CreateCommand(ip, "join_thread", t_join_thread, 0, 0);
    Tcl_CreateCommand(ip, "sync", t_sync, 0, 0);
    Tcl_CreateCommand(ip, "sync_thread", t_sync_thread, 0, 0);
    Tcl_CreateCommand(ip, "yield", t_yield, 0, 0);

    Tcl_CreateCommand(ip, "link_to_inter_thread_comm_buffer", t_link_to_inter_thread_comm_buffer, 0, 0);
    Tcl_CreateCommand(ip, "echo", t_echo, 0, 0);
    // Tcl_CreateCommand(ip, "verbose", t_verbose, 0, 0);
    Tcl_CreateCommand(ip, "exit", t_exit, 0, 0);
    Tcl_CreateCommand(ip, "time", t_time, 0, 0);
}

static void process_stdin(Tcl_Interp* ip)
{
    FUNC(process_stdin);
    char line[1000];
    int partial = 0;
    Tcl_DString buf;
    Tcl_DStringInit(&buf);
    int tty = isatty(0);
    sfile_read_hdl_t stdin_hdl(0);

    while (1) {

	cin.clear();
	if (tty) {
	    char* prompt = Tcl_GetVar(ip, (partial ? "tcl_prompt2" :
					   "tcl_prompt1"), TCL_GLOBAL_ONLY);
	    if (! prompt) {
		if (! partial)  cout << "% " << flush;
	    } else {
		if (Tcl_Eval(ip, prompt) != TCL_OK)  {
		    cerr << ip->result << endl;
		    Tcl_AddErrorInfo(ip,
				     "\n    (script that generates prompt)");
		    if (! partial) cout << "% " << flush;
		} else {
		    fflush(stdout);
		}
	    }

	    // wait for stdin to have data
	    {
		W_IGNORE( stdin_hdl.wait(-1) );
		DBG(<< "stdin is ready");
	    }
	}
	cin.getline(line, sizeof(line) - 2);
	line[sizeof(line)-2] = '\0';
	strcat(line, "\n");
	if ( !cin) {
	    if (! partial)  {
		break;
	    }
	    line[0] = '\0';
	}
	char* cmd = Tcl_DStringAppend(&buf, line, -1);
	if (line[0] && ! Tcl_CommandComplete(cmd))  {
	    partial = 1;
	    continue;
	}
	partial = 0;
	int result = Tcl_RecordAndEval(ip, cmd, 0);
	Tcl_DStringFree(&buf);
	if (result == TCL_OK)  {
	    if (ip->result[0]) {
		cout << ip->result << endl;
	    }
	} else {
	    if (result == TCL_ERROR && !strcmp(ip->result, TCL_EXIT_ERROR_STRING))  {
		break;
	    }  else  {
	        cerr << "process_stdin(): Error";
	        if (result != TCL_ERROR) cerr << " " << result;
	        if (ip->result[0]) 
		    cerr << ": " << ip->result;
	        cerr << endl;
	    }
	}
    }
}

void tcl_thread_t::run()
{
    W_COERCE( thread_mutex.acquire() );
    if (args) {
	int result = Tcl_Eval(ip, args);
	if (result != TCL_OK)  {
	    cerr << "error in tcl thread: " << ip->result << endl;
	}
    } else {
	process_stdin(ip);
    }
    if (xct() != NULL) {
	cerr << "Dying thread is running a transaction: aborting ..." << endl;
	w_rc_t rc = sm->abort_xct();
	if(rc) {
	    cerr << "Cannot abort tx : " << rc << endl;
	    if(rc.err_num() == ss_m::eTWOTRANS)  {
		me()->detach_xct(xct());
	    }
	} else {
	    cerr << "Transaction abort complete" << endl;
	}
    }
    thread_mutex.release();
}

void tcl_thread_t::join(unsigned long id)
{
    w_list_i<tcl_thread_t> i(list);
    tcl_thread_t* r;
    while ((r = i.next()) && (r->id != id));
    if (r)  {
	W_COERCE( r->wait() );
	delete r;
    }
}

void tcl_thread_t::sync_other(unsigned long id)
{
    w_list_i<tcl_thread_t> i(list);
    tcl_thread_t* r;
    while ((r = i.next()) && (r->id != id));
    if (r)  {
	if (r->status() != r->t_defunct)  {
	    
	    // cout << "thread " << me()->id << " waiting for " << id
	    //	 << endl;
	    
	    W_COERCE( r->sync_point.wait() );
	    W_IGNORE( r->sync_point.reset(0) );
	    
	    // cout << "thread " << me()->id << " awake " << id <<
	    // endl;
	    
	    assert(r->proceed.is_hot());

	    W_COERCE( r->lock.acquire() );
	    r->proceed.signal();
	    r->lock.release();

	    assert(!r->proceed.is_hot());
	    smthread_t::yield();
	}
    }
}

void tcl_thread_t::sync()
{
    tcl_thread_t* r = ((tcl_thread_t*)me());
    assert(r);
    W_IGNORE( r->sync_point.post() );
    
    // cout << "thread " << me()->id << " wait" << endl;

    W_COERCE( r->lock.acquire() );
    W_COERCE( r->proceed.wait(r->lock) );
    r->lock.release();
    
    // cout << "thread " << me()->id << " woke" << endl;
}

void
copy_interp(Tcl_Interp *ip, Tcl_Interp *pip)
{
    int result = Tcl_Eval(ip, tcl_init_cmd);
    if (result != TCL_OK)  {
	cerr << "tcl_thread_t(): Error";
	if (result != TCL_ERROR) cerr << " " << result;
	if (ip->result[0])  cerr << ": " << ip->result;
	cerr << endl;
	w_assert3(0);;
    }

    Tcl_CreateCommand(ip, "sm", sm_dispatch, 0, 0);
#ifdef USE_COORD
    Tcl_CreateCommand(ip, "co", co_dispatch, 0, 0);
#endif

#ifdef USE_VERIFY
    Tcl_CreateCommand(ip, "ovt", ovt_dispatch, 0, 0);
#endif
    create_stdcmd(ip);

    grab_vars(ip, pip);
    grab_procs(ip, pip);
    Tcl_ResetResult(ip);
    Tcl_ResetResult(pip);
}

tcl_thread_t::tcl_thread_t(int ac, char* av[],
			   Tcl_Interp* pip,
			   bool block_immediate, bool auto_delete)
: smthread_t(t_regular, block_immediate, auto_delete, "tcl_thread"),  
  sync_point(0,"tcl_th.sync"),
  proceed("tcl_th.go"),
  args(0),
  ip(Tcl_CreateInterp()),
  thread_mutex("tcl_th") // m:tcl_th
{
    /* give thread & mutex a unique name, not just "tcl_thread" */
    {
	char	buf[40];
	strstream	o(buf, sizeof(buf)-1);
	o.form("tcl_thread(%d)", id);
	o << ends;
	rename(o.str());
	o.form("tcl_thread_mutex(%d)", id);
	o << ends;
	thread_mutex.rename(o.str());
    }

    w_assert1(ip);
    
    W_COERCE( thread_mutex.acquire() );

    if (++count == 1)  {
	assert(sm == 0);
	sm = new ss_m ();
	if (! sm)  {
	    cerr << __FILE__ << ':' << __LINE__
		 << " out of memory" << endl;
	    thread_mutex.release();;
	    W_FATAL(fcOUTOFMEMORY);
	}
    }
    unsigned int len;
    int i;
    for (i = 0, len = 0; i < ac; i++)
	len += strlen(av[i]) + 1;

    if (len) {
	args = new char[len];
	w_assert1(args);

	args[0] = '\0';
	for (i = 0; i < ac; i++) {
		if (i)
			strcat(args, " ");
		strcat(args, av[i]);
	}
	w_assert1(strlen(args)+1 == len);
    }

    copy_interp(ip, pip);

    list.push(this);

    thread_mutex.release();
}

tcl_thread_t::~tcl_thread_t()
{
    W_COERCE( thread_mutex.acquire() );
    if (--count == 0)  {
	//COERCE(sm->dismount_all());
	delete sm;
	sm = 0;
#ifdef USE_COORD
	delete globalDeadlockServer;
	globalDeadlockServer = 0;
#endif
    }

    if (ip) {
	Tcl_DeleteInterp(ip);
	ip = 0;
    }
    
    delete [] args;

    // cout << "thread " << id << " died" << endl;
    
    link.detach();

    thread_mutex.release();
}

void tcl_thread_t::initialize(Tcl_Interp* ip, const char* lib_dir)
{
    static int first_time = 1;
    
    if (first_time)  {
	first_time = 0;
	
	char buf[_POSIX_PATH_MAX + 1];
	ostrstream s(buf, sizeof(buf));
	s << lib_dir << "/ssh.tcl" << '\0';
	if (Tcl_EvalFile(ip, buf) != TCL_OK)  {
	    cerr << __FILE__ << ':' << __LINE__ << ':'
		 << " error in \"" << buf << "\" script" << endl;
	    cerr << ip->result << endl;
	    W_FATAL(fcINTERNAL);
	}

#ifdef USE_VERIFY
	s.seekp(0);
	s << lib_dir << "/ovt.tcl" << '\0';
	if (Tcl_EvalFile(ip, buf) != TCL_OK)  {
	    cerr << __FILE__ << ':' << __LINE__ << ':'
		 << " error in \"" << buf << "\" script" << endl;
	    cerr << ip->result << endl;
	    W_FATAL(fcINTERNAL);
	}
#endif
    }

    create_stdcmd(ip);
}
