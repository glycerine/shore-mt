/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sthread.cc,v 1.238 1997/09/30 22:17:57 solomon Exp $
 */
#define STHREAD_C

#include <strstream.h>
#include <w_workaround.h>
#include <w_signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <new.h>
#include <sys/uio.h>
#include <sys/stat.h>

#if defined(Sparc) && !defined(SOLARIS2)
#include <vfork.h>
#endif

#if defined(Mips) && !defined(Irix)
extern "C" int vfork();
#endif

#if defined(Mips) && defined(Irix)
#define vfork fork
#endif

#if defined(AIX32) && !defined(AIX41)
#define	vfork	fork
#endif

#if defined(AIX32) || defined(AIX41)
static void hack_aix_signals();
#endif

/* XXX In general signal handling needs to be improved.  Until then,
   we do the best we can to ignore it. */ 
static	void	hack_signals();


#ifdef __GNUC__
#pragma implementation "sthread.h"
#endif

#define DBGTHRD(arg)
#define DBG(arg)
#define FUNC(arg)

#define W_INCL_LIST
#define W_INCL_SHMEM
#define W_INCL_TIMER
#include <w.h>

#include <w_statistics.h>

#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif

#include "sthread.h"
#include "sthread_stats.h"
#include "spin.h"
#include "diskrw.h"

#ifdef __GNUC__
template class w_list_t<sthread_t>;
template class w_list_i<sthread_t>;
template class w_descend_list_t<sthread_t, sthread_t::priority_t>;
template class w_keyed_list_t<sthread_t, sthread_t::priority_t>;
#endif

#if !defined(AIX41) && !defined(SOLARIS2) && !defined(Linux)
extern "C" {
	extern int writev(int, const struct iovec *, int);
	extern int readv(int, const struct iovec *, int);
}
#endif


//////////////////////////////////////////////
// YOU MUST MAKE  
// W_FASTNEW_STATIC_DECL
// the first thing in the file so that it 
// gets constructed before any threads do!
//
//////////////////////////////////////////////
W_FASTNEW_STATIC_PTR_DECL(sthread_name_t);

class sthread_stats SthreadStats;

const
#include "st_einfo.i"

#ifdef I860
extern "C" {
extern char *strerror(int error);
}
#endif

extern "C" void dumpthreads();
extern "C" void threadstats();

static int gotenv = 0;
static bool do_fastpath = false;

#ifdef PIPE_NOTIFY
#define	FD_NONE	-1
static int		master_chan[2] = { FD_NONE, FD_NONE};


class pipe_handler : public sfile_hdl_base_t {
public:
	pipe_handler(int fd) : sfile_hdl_base_t(fd, rd) { }
	~pipe_handler() { }
	void	read_ready();
	void	write_ready() { }
	void	exception_ready() { }
};

static pipe_handler	chan_event(FD_NONE);


#if 0
static void
FD_CLOSE_ON_EXEC(int fd)
{
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
	:: perror("fcntl(close_on_exec)");
}
#endif /*0*/

static void 
FD_NODELAY(int fd)
{
    if (fcntl(fd, F_SETFL, O_NDELAY) == -1)
	::perror("fcntl(O_NDELAY)");
}

void	pipe_handler::read_ready()
{
	ChanToken token;
	int	pending = 0;

	while  (::read(fd, (char *)&token, sizeof(token)) == sizeof(token)) {
		pending++;
	}
	if (pending)
		sthread_t::_polldisk();
}
#endif /* PIPE_NOTIFY */



/*********************************************************************
 *
 *  class sthread_timer_t
 *
 *  Times out threads that are blocked on a timeout.
 *
 *********************************************************************/

class sthread_timer_t : public w_timer_t, public sthread_base_t {
public:
	sthread_timer_t(const stime_t &when, sthread_t &t)
		: w_timer_t(when), _thread(t)    {}

	void		trigger();

private:
	sthread_t	&_thread;
};


void sthread_timer_t::trigger()
{
	if (_thread.status() == sthread_t::t_blocked)  {
		W_COERCE( _thread.unblock(RC(stTIMEOUT)) );
	}
}



/*********************************************************************
 *
 *  class ready_q_t
 *
 *  The ready queue --- initialized by sthread_init_t.
 *  The queue is really made up of multiple queues, each serving
 *  a priority level. A get() operation dequeues threads from the
 *  highest priority queue that is not empty.
 *
 *********************************************************************/
class ready_q_t : public sthread_base_t {
public:
    NORET			ready_q_t();
    NORET			~ready_q_t()   {};

    sthread_t* 			get();
    void			put(sthread_t* t);
    void			change_priority(sthread_t*, sthread_t::priority_t p);

    bool			is_empty() const { return count == 0; }
    bool			contains_only(sthread_t *t) const;

private:
    uint4_t			count; 
    enum { nq = sthread_t::max_priority + 1 };
    sthread_t*			head[nq];
    sthread_t*			tail[nq];
};



/*********************************************************************
 *
 *  Shared memory and static declarations
 *
 *********************************************************************/
static w_shmem_t shmem_seg;
static diskport_t* diskport = 0;
static int diskport_cnt = 0;
static svcport_t* svcport = 0;


/*********************************************************************
 *
 *  Dummy core for use when valid core is unavailable during
 *  initialization and thread death
 *
 *********************************************************************/
static sthread_core_t dummy_core;

/* is the process protected from signals? */
static bool in_cs = false;

/*********************************************************************
 *
 *  Class static variable intialization
 *
 *********************************************************************/
w_base_t::uint4_t	sthread_init_t::count = 0;


const char* 		sthread_t::_diskrw = "diskrw";
sthread_t* 		sthread_t::_me;
sthread_t* 		sthread_t::_idle_thread = 0;
sthread_t*		sthread_t::_main_thread = 0;
w_base_t::uint4_t	sthread_t::_next_id = 0;
pid_t			sthread_t::_dummy_disk = 0;
ready_q_t*		sthread_t::_ready_q = 0;
sthread_list_t*		sthread_t::_class_list = 0;
w_timerqueue_t		sthread_t::_event_q;
sfile_handler_t*	sthread_t::_io_events;
int			sthread_t::_io_in_progress = 0;
unsigned char		sthread_t::_io_flags[sthread_open_max];

/*
  The "idle" thread is really more than a "run when nothing else is"
  thread.  It is also responsible for ALL event generation in the
  system, including IO completions (diskrw), descriptor I/O ready
  (sfile), and timeouts (stimer).  If high priority threads are
  always scheduled, events which they are waiting upon may never be
  generated.

  To combat this problem, the context switcher will automagically
  increase the priority of the idle thread, which guarantees that it
  will eventually have a chance to run.  Once the idle thread starts,
  it will reduce its' priority to idle_time, so that it doesn't
  consume thread time continuously.

  The maximum priority which the idle thread will climb to is
  'idle_priority_max'.  'idle_priority_push' and 'idle_priority_phase'
  implement a mechanism to boost the priority of the idle thread by a
  priority level after every 'idle_priority_push' context switches.  */

int	sthread_t::_idle_priority_phase = 0;
int	sthread_t::_idle_priority_push = 10;
int	sthread_t::_idle_priority_max = max_priority;


#ifdef DEBUG

#ifdef extreme_problems
// This function is used for debugging to verify that signals are blocked.
static int sthread_signals_are_blocked()
{
    sigset_t checkset;

    sigprocmask(SIG_SETMASK, 0, &checkset);
    return sigismember(&checkset, SIGUSR2);
}
#endif	

#if !defined(HPUX8) && !defined(Irix)
#define	HAVE_SYS_SIGLIST
#endif

#ifdef HAVE_SYS_SIGLIST
/* funny names? */
#ifdef SOLARIS2
#define	sys_siglist	_sys_siglist
#endif

/* create a prototype if necessary */
#if !defined(SOLARIS2) && !defined(Linux)
extern const char *sys_siglist[];
#endif

void print_sigmask()
{
	int		i;
	sigset_t	mask;
	bool		blocked = false;
	
	sigprocmask(SIG_BLOCK, 0, &mask);
	for (i = 0; i < NSIG; i++) {
		if (sigismember(&mask, i)) {
			blocked = true;
			break;
		}
	}
	if (blocked) {
		cout << "blocked signals:";
		for (i = 1; i < NSIG; i++)
			if (sigismember(&mask, i)) {
#ifdef HAVE_SYS_SIGLIST
				cout << " '" << sys_siglist[i] <<  "'";
#else
				cout.form(" 'signal_%d'", i); 
#endif
			}
	}
	else
		cout << "no signals blocked";
	cout << endl;
}
#endif /* HAVE_SYS_SIGLIST */
#endif


/*
 *  sthread_t::_ctxswitch(status)
 *
 *  Set the status of the current thread and switch to the
 *  next ready thread.
 */

void sthread_t::_ctxswitch(status_t s)
{
    sthread_t *doner = _me;		/* giving up CPU */
    sthread_t *taker = _ready_q->get();	/* taking up CPU */
    sthread_t *dieing = 0;

    ++SthreadStats.ctxsw;

    /*
       Don't bother changing the priority of the idle thread if it
       has just run, or is just about to run.
       1) It has/will run recently.
       2) It won't be in the ready_q, and the change_priority() will fail.
     */  
    if (_idle_thread
	&&  doner != _idle_thread && taker != _idle_thread
	&&  _idle_priority_phase++ > _idle_priority_push) {

	int old_pri = _idle_thread->_priority;
	int new_pri = old_pri + 1;

	if (old_pri > _idle_priority_max)
		new_pri = old_pri;
	else if	(new_pri > _idle_priority_max)
		new_pri = _idle_priority_max;

	if (new_pri != old_pri) {
#if 0
		cout.form("== Change priority of idle thread from %d to %d ==\n",
			  _idle_thread->_priority, new_pri); 
#endif
		_ready_q->change_priority(_idle_thread, (priority_t) new_pri);
	}
	_idle_priority_phase = 0;
    }

    /* If there are no other threads in the ready queue, schedule
       the current thread.  If the current thread is exiting, this
       must be undone.  Another problem occurs if the thread is not
       runnable.  This isn't checked for! XXX */
    if (!taker)
	taker = doner;

    switch (s) {
    case t_defunct:
	/* The current thread is exiting.  There MUST be another thread to
	   switch to */
	w_assert1(taker != _me);	

	if (_me) {
	    /* XXX if a context switch occurs here, threads break */
	    W_COERCE( _me->_terminate->post() );
	}

	dieing = _me;
	doner = _me = 0;
	break;

    case t_ready:
	/* Insert the current thread into the ready queue. */
	doner->_status = s;
	if (taker != doner) _ready_q->put(doner);
	break;

    case t_blocked:
	/* Set status. Don't insert thread into ready queue. */
	doner->_status = s;
	break;

    case t_boot:
	/* bootstrap the main thread */
	w_assert1(taker);
	w_assert1(doner == 0);
	w_assert1(taker->_status == t_ready);
	doner = taker;
	break;

    default:
	W_FATAL(stINTERNAL);
    }

    /*
     *  Context switch --- change identity
     */
    (_me = taker)->_status = t_running;

    if (doner != taker)  {
	    sthread_core_t *from_core = doner ? &doner->_core : &dummy_core;

#if defined(DEBUG)  
	    if (doner && !sthread_core_stack_ok(from_core, 1)) {
		    cerr << "*** Stack overflow ***" << endl;
		    cerr << *doner << endl;
		    W_FATAL(fcINTERNAL);
	    }
#endif /*DEBUG*/

	    sthread_core_switch(from_core, &taker->_core);
    }
    
    /* Current thread resumes */
    w_assert3(_me == doner && _me->_status == t_running);
}


/*
 * sthread_t::startup()
 *
 * Initialize system threads from cold-iron.  The main thread will run
 * on the stack startup() is called on.
 */

w_rc_t	sthread_t::startup()
{
	_ready_q = new ready_q_t;
	if (_ready_q == 0)
		W_FATAL(fcOUTOFMEMORY);

	_class_list = new sthread_list_t(offsetof(sthread_t, _class_link));
	if (_class_list == 0)
		W_FATAL(fcOUTOFMEMORY);

	_io_events = new sfile_handler_t();
	if (_io_events == 0)
		W_FATAL(fcOUTOFMEMORY);

	/* XXX magic number */
	/* XXX must delete dummy core, or rid ourselves of it! */
	if (sthread_core_init(&dummy_core, 0, 0, 1024*4) == -1)
		W_FATAL(stINTERNAL);
	
	/*
	 * Boot the main thread onto the current (system) stack.
	 */
	sthread_main_t *main = new sthread_main_t;
	if (!main)
		W_FATAL(fcOUTOFMEMORY);
	W_COERCE( main->fork() );
	_ctxswitch(t_boot);
	if (_me != main)
		W_FATAL(stINTERNAL);
	_main_thread = main;

#if defined(PURIFY)
	/* The main thread is different from all other threads. */
	purify_name_thread(me()->name());
#endif	

	/*
	 *  Bring the idle thread to life.
	 */
	sthread_t *idle = new sthread_idle_t;
	if (!idle)
		W_FATAL(fcOUTOFMEMORY);
	W_COERCE( idle->fork() );
	_idle_thread = idle;

	yield();	// force the idle thread to start
	w_assert3(in_cs);

	return RCOK;
}

/*
 * sthread_t::shutdown()
 *
 * Shutdown the thread system.  Must be called from the context of
 * the main thread.
 *
 * XXX notyet
 */

w_rc_t sthread_t::shutdown()
{
	if (_me != _main_thread) {
		cerr << "sthread_t::shutdown(): not main thread!" << endl;
		return RC(stINTERNAL);
	}

	/* wait for other threads */

	/* shutdown disk i/o */

#ifdef notyet
	/* Clean up and exit. */
	/* XXX needs work */
	if (shmem_seg.base()) {
		kill(_dummy_disk, SIGHUP);
		for (int i = 0; i < open_max; i++)  {
			if (diskport[i].pid && diskport[i].pid != -1)
				kill(diskport[i].pid, SIGHUP);
		}
		if (svcport) {
			svcport->~svcport_t();
			svcport = 0;
		}
		shmem_seg.destroy();
	}
	unname();	// clean up to avoid assertion in fastnew
#endif

	return RCOK;
}


/*
 * sthread_t::setup_signals()
 *
 * Initialize signal handlers and set priority level / signal mask.
 */

w_rc_t	sthread_t::setup_signals(sigset_t &lo_spl, sigset_t &hi_spl)
{
	sigset_t nset, oset;

	sigemptyset(&oset);
	sigemptyset(&nset);
#ifndef PIPE_NOTIFY
	sigaddset(&nset, SIGUSR2);
#endif
	
#ifdef POSIX_SIGNALS
	/* POSIX way -- expects handler to take no parameters */
	
	struct sigaction sact;
	sact.sa_handler = W_POSIX_HANDLER _caught_signal;
	sact.sa_mask = nset;
	sact.sa_flags = 0;

#ifndef PIPE_NOTIFY
	sigaction(SIGUSR2, &sact, 0);
#endif
#if 0 /* not used */
	sigaction(SIGINT, &sact, 0);  // catch cntrl-C
#endif
#else
	/* ANSI STANDARD C way -- allows int arg on handler */

#ifndef PIPE_NOTIFY	
	if (signal(SIGUSR2, W_ANSI_C_HANDLER _caught_signal) 
	    == W_ANSI_C_HANDLER SIG_ERR) {
		W_FATAL(stOS);
	}
#endif
#endif
	
	if (sigprocmask(SIG_BLOCK, &nset, &oset) == -1) {
		w_rc_t	e = RC(stOS);
		cerr << "sigprocmask(SIG_BLOCK):" << endl << e << endl;
		W_COERCE(e);
	}

	lo_spl = oset;	// lo priority ... interrupts allowed
	hi_spl = nset;	// hi priority ... no interrupts
	in_cs = true;

	return RCOK;
}


/*********************************************************************
 *
 *  sthread_init_t::sthread_init_t()
 *
 *  Initialize the sthread environment. The first time this method
 *  is called, it sets up the environment and returns with the
 *  identity of main_thread.
 *
 *********************************************************************/
sthread_init_t::sthread_init_t()
{
    if (++count == 1) {

	if (!gotenv) {
	    gotenv++;	// checked the environment variable
	    if(getenv("DEBUGTHREADS")) {
		gotenv++; // environment variable was set
	    }

	    if(getenv("FASTPATH")) {
		do_fastpath = true;
	    }

	}

	/*
	 *  Register error codes.
	 */
	if (! w_error_t::insert(
		"Threads Package",
		error_info, 
		sizeof(error_info) / sizeof(error_info[0])))   {
	    
	    cerr << "sthread_init_t::sthread_init_t: "
		 << " cannot register error code" << endl;
	    
	    W_FATAL(stINTERNAL);
	}

#if defined(AIX31) || defined(AIX41)
	hack_aix_signals();
#endif

	hack_signals();

	/*
	 * There is a chance that the static _w_fastnew member of
	 * sthread_name_t has not been constructed yet.
	 */
	sthread_name_t::_w_fastnew =  new w_fastnew_t(sizeof(sthread_name_t),
						      100, __LINE__, __FILE__);
	if (sthread_name_t::_w_fastnew == 0)
		W_FATAL(fcOUTOFMEMORY);

	W_COERCE(sthread_t::startup());
    }
}



/*********************************************************************
 *
 *  sthread_init_t::~sthread_init_t()
 *
 *  Destructor. Does not do much.
 *
 *********************************************************************/
NORET
sthread_init_t::~sthread_init_t()
{
    w_assert1(count > 0);
    if (--count == 0)  {

	// sthread_t::_class_list.pop();
	// delete sthread_t::_class_list;
	// delete sthread_t::_ready_q;

	W_COERCE(sthread_t::shutdown());
    }
}


/*
 *  sthread_main_t::sthread_main_t()
 *
 * Create the main thread.  It is a placeholder for the
 * thread which uses the system stack.
 */

sthread_main_t::sthread_main_t()
: sthread_t(t_regular, false, false, "main_thread", 0)
{
}


/*
 *  sthread_main_t::run()
 *
 *  This is never called.  It's an artifact of the thread architecture.
 */

void sthread_main_t::run()
{
}



/*********************************************************************
 *
 *  sthread_t::set_bufsize(sz)
 *
 *  Allocate and return a shared memory segment of (sz + delta) bytes
 *  for asynchronous disk I/O. The extranous delta bytes in the 
 *  shared memory is used to hold the control structures for
 *  communicating between main process and diskrw processes (e.g.
 *  the queues to submit disk requests).
 *
 *  If sz is 0, the previously allocated shared memory segment is 
 *  freed.
 *
 *********************************************************************/
char*
sthread_t::set_bufsize(int size)
{
    if (size == 0)  {
	/*
	 *  Free shared memory previous allocated.
	 */
	if (shmem_seg.base())  {
	    if (kill(_dummy_disk, SIGUSR2) == -1 ||
		waitpid(_dummy_disk, 0, 0) == -1)  {
		cerr << "warning: error killing dummy disk pid " 
		     << _dummy_disk << endl
		     << RC(stOS) << endl;
	    }
	    for (int i = 0; i < open_max; i++)  {
		if (diskport[i].pid && diskport[i].pid != -1) {
		    if (kill(diskport[i].pid, SIGTERM) == -1 ||
			waitpid(diskport[i].pid, 0, 0) == -1)  {
			cerr << "warning: error killing disk pid " 
			     << diskport[i].pid << endl
			     << RC(stOS) << endl;
		    }
#ifdef PIPE_NOTIFY
		    ::close(diskport[i].rw_chan[PIPE_OUT]);
#endif
		}
	    }
	    svcport->~svcport_t();
	    W_COERCE( shmem_seg.destroy() );
	}
	diskport = 0;
	svcport = 0;
	return 0;
    }

    if (shmem_seg.base())  {
	/*
	 *  Cannot re-allocate shared memory.
	 */
	return 0;	// disable for now
    }


    /*
     *  Calculate the extra bytes we need for holding control info.
     */
    int extra = sizeof(svcport_t) + sizeof(diskport_t)*open_max;
    if (extra & 0x7)
	extra += 8 - (extra & 0x7);

    /*
     *  Allocate the shared memory segment
     */
    w_rc_t rc = shmem_seg.create(size + extra);
    if (rc)  {
	cerr << "fatal error: cannot create shared memory" << endl;
	W_COERCE(rc);
    }

    /*
     *  Initialize server port and disk ports in the shared memory
     *  segment allocated.
     */
    svcport = new (shmem_seg.base()) svcport_t;
    
    diskport = (diskport_t*) (shmem_seg.base() + sizeof(svcport_t));
    for (int i = 0; i < open_max; i++)  {
	new (diskport + i) diskport_t;
    }
#ifdef PIPE_NOTIFY
    svcport->sleep = 1;
#endif /* PIPE_NOTIFY */


    /*
     *  Fork a dummy diskrw process that monitors longetivity of
     *  this process.
     */
    char arg[10];
    ostrstream s(arg, sizeof(arg));
    s << shmem_seg.id() << '\0';
    _dummy_disk = ::vfork();
    if (_dummy_disk == -1) {
	W_COERCE( shmem_seg.destroy() );
	cerr << "vfork  " << ::strerror(::errno) << endl;
	::exit(-1);
    } else if (_dummy_disk == 0) {
	execlp(_diskrw, _diskrw, arg, 0);
	cerr << "could not start disk I/O process, " << _diskrw 
	     << ", because: " << ::strerror(::errno) << endl;
	kill(getppid(), SIGKILL);
	W_COERCE( shmem_seg.destroy() );
	::exit(-1);
    }

    /*
     *  Return pointer to the user space (above control info space).
     */
    return shmem_seg.base() + extra;
}



/*
 *  sthread_t::_caught_signal(sig)
 *
 *  Handle signals.
 */
 
void sthread_t::_caught_signal(int sig)
{
    // cerr << "_caught_signal " << dec << sig << endl;
#ifndef PIPE_NOTIFY
    if(sig==SIGUSR2) {
	ShmcStats.notify++;
    }
#else
    /* cure for a bogus gcc-2.7.2 warning message */
    sig = sig;
#endif

#if 0 /* not used */
    if (sig == SIGINT && _dummy_disk != 0) {
	// cntrl-C, so signal dummy disk process so it can free memory
	cerr << "ctrl-C detected -- killing disk processes" 
	     << endl << flush;

	// signal dummy diskrw process so it can kill the others
	// and free shared memory.
	if (kill(_dummy_disk, SIGINT) == -1) {
	    cerr << 
	    "error: cannot kill disk process that cleans up shared memory(" 
		 << _dummy_disk << ')' << endl
		 << RC(stOS) << endl;
	    cerr << "You might have to clean up shared memory segments by hand."
		<< endl;
	    cerr << "See ipcs(1) and ipcrm(1)."<<endl;

	    _exit(-1);
	} else {
	    cerr << "ctrl-C detected -- exiting" << endl;
	    _exit(0);
	}
    }
#endif

    w_assert3(!in_cs);
    /* nothing more process */
}



/*********************************************************************
 * 
 *  sthread_t::_polldisk()
 *
 *  Poll all open disk ports to check for I/O acknowledgement.
 *
 *********************************************************************/
void 
sthread_t::_polldisk()
{
    int count = 0;
    if(gotenv>1) {
	cerr << "_polldisk()" << endl;
    }
    for (register i = 0; i < open_max; i++) {
	if (diskport[i].pid && diskport[i].pid != -1)  {
	    while (diskport[i].recv_ready())  {
		/*
		 *  An I/O has completed. Dequeue the ack and unblock
		 *  the thread that initiated the I/O.
		 */
		diskmsg_t m;
		diskport[i].recv(m);
		W_COERCE( m.thread->unblock() );
		++count;
	    }
	}
    }
    svcport->decr_incoming_counter(count);
}


/*
 * The idle thread <insert description from somewhere else>
 *
 * The thread is created at normal priority so system
 * startup will allow the idle thread cpu time for its setup
 * before other threads are run.
 */

sthread_idle_t::sthread_idle_t()
: sthread_t(t_regular, 0, 0, "idle_thread")
{
}



/*
 *  sthread_idle_t::run()
 *
 *  Body of idle thread.
 */
void 
sthread_idle_t::run()
{
    const stime_t DEFAULT_TIMEOUT(stime_t::sec(10));	// 10 seconds
    const stime_t IMMEDIATE_TIMEOUT;			// 0 seconds

    sigset_t lo_spl, hi_spl;
    W_COERCE(setup_signals(lo_spl, hi_spl));

    /*
     *  Idle thread only runs at minimum priority
     */
    W_COERCE( _me->set_priority(t_idle_time) );

    /*
     *  Main loop
     */
    for (;;)	{
	bool	timeout_immediate = false;

	/*
	 *  Check if any disk I/O is done
	 */
	if (svcport)  { // && svcport->peek_incoming_counter()) { /*}*/
#ifdef PIPE_NOTIFY
	    w_assert3(svcport->sleep == 1);
#else
	    svcport->sleep = 0;
#endif /* PIPE_NOTIFY */
	    _polldisk();
	}

	yield();
	SthreadStats.idle_yield_return++;

	if (svcport && svcport->peek_incoming_counter())
		_polldisk();

	if(_priority > t_idle_time)  {
	    // we might wake up to find that our priority was changed
	    DBG(<<"priority was changed");
	    timeout_immediate = true;

	    /* XXX ??? reduce priority AFTER scheduling I/O
	       completions, to allow idle thread as I/O tasker to
	       complete first. */

	    // set it back
	    W_COERCE( _me->set_priority(t_idle_time) );
	}

	if (svcport) {
	    if (svcport->peek_incoming_counter()) {
		/*
		 *  Some I/O has finished. Do not go to sleep.
		 */
#ifndef PIPE_NOTIFY
		svcport->sleep = 0; 
#endif /* !PIPE_NOTIFY */

		continue;
	    }
	    svcport->sleep = 1;
        }

	/*
	 * Determine the scheduling delay, if any.
	 */
	stime_t	then;		// when the next event occurs
	stime_t	before;		// time before waiting
	stime_t	after;		// time after waiting
	stime_t	timeout;	// selected timeout	

	bool events_pending = false;		// timer events have occured
	bool have_before = false;		// lazy time retrieval
	bool events_scheduled = _event_q.nextEvent(then);

	if (timeout_immediate)
		timeout = IMMEDIATE_TIMEOUT;
	else if (! _ready_q->is_empty())
		timeout = IMMEDIATE_TIMEOUT;
	else if (events_scheduled) {
		// lazy system call
		before = stime_t::now();
		have_before = true;
		if (then <= before) {
			events_pending = true;
			timeout = IMMEDIATE_TIMEOUT;
		}
		else
			timeout = then - before;
	}
	else
		timeout = DEFAULT_TIMEOUT;
	
	/*
	 *  Wait for external events or interrupts
	 */
	static continuous_idle = 0;
	w_assert1(!svcport || svcport->sleep == 1);

	W_COERCE(_io_events->prepare(timeout, false));

	/* allow interrupts */
	int kr;
	in_cs = false;	/* system call can trigger a signal */
	kr = sigprocmask(SIG_SETMASK, &lo_spl, 0);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "sigprocmask(SIG_SETMASK)" << endl << e << endl;
		::_exit(1);
	}

#ifdef EXPENSIVE_STATS
	/* A more accurate select-time estimate */
	before = stime_t::now();
#else
	// lazy system calls :-)
	if (!have_before)
		before = stime_t::now();
#endif

	w_rc_t rc = _io_events->wait();

	after = stime_t::now();
	
	/* block interrupts */
	kr = sigprocmask(SIG_SETMASK, &hi_spl, 0);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "sigprocmask(SIG_SETMASK)" << endl << e << endl;
		::_exit(1);
	}
	in_cs = true;	/* only now is signal delivery blocked */

	SthreadStats.idle_time += (float) (after - before);

	/*
	 * Dispatch any events which are pending, from a menu
	 * of file, and timer events
	 */
	
	/* file events */
	if (rc == RCOK)
		_io_events->dispatch();

	/* timer events */
	// reevaluate if needed
	if (!events_pending && events_scheduled)
		events_pending = (then <= after);

	if (events_pending)
		_event_q.run(after /* + stime_t::msec(20) ??? */ );

	if (rc)  {
		if (rc.err_num() == stTIMEOUT)  {
			SthreadStats.idle++;

			/* If we have been idled for too long, print
			 status to stdout for debugging.  */

			if (++continuous_idle == 40) {
				if (gotenv > 1)
					dumpthreads();
				continuous_idle = 0;
			}
		}
		else
			continuous_idle = 0;
	}
    }
}



/*
 *  sthread_t::set_use_float(flag)
 *
 *  Current thread do not make floating point calculations if
 *  flag is TRUE. FALSE otherwise.
 * 
 *  Optimization. We do not need to save some floating point 
 *  registers when context switching.
 */

w_rc_t sthread_t::set_use_float(int flag)
{
	sthread_core_set_use_float(&_core, flag ? 1 : 0);
	return RCOK;
}



/*
 *  sthread_t::set_priority(priority)
 *
 *  Sets the priority of current thread.  The thread must not
 *  be in the ready Q.
 */

w_rc_t sthread_t::set_priority(priority_t priority)
{
	_priority = priority;

	// in this statement, <= is used to keep gcc -Wall quiet
	if (_priority <= min_priority) _priority = min_priority;
	if (_priority > max_priority) _priority = max_priority;

	if (_status == t_ready)  {
		cerr << "sthread_t::set_priority()  :- "
			<< "cannot change priority of ready thread" << endl;
		W_FATAL(stINTERNAL);
	}

	return RCOK;
}



/*
 *  sthread_t::sleep(timeout)
 *
 *  Sleep for timeout milliseconds.
 */

void sthread_t::sleep(long timeout, char *reason)
{
	reason = (reason && *reason) ? reason : "sleep";

	W_IGNORE(block(timeout,  0, reason, this));
}

/*
 *  sthread_t::wakeup()
 *
 *  Cancel sleep 
 */

void sthread_t::wakeup()
{
    if(_tevent) _tevent->trigger();
}


/*
 *  Wait for this thread to end. This method returns when this thread
 *  end or when timeout expires.
 */  

w_rc_t
sthread_t::wait(long timeout)
{
	/* A thread that hasn't been forked can't be wait()ed for.
	   It's not a thread until it has been fork()ed. */
	if (_status == t_virgin)
		return RC(stOS);

	/*
	 *  Wait on _terminate sevsem. This sevsem is posted when 
	 *  the thread ends.
	 */
	return _terminate->wait(timeout);
}


/*
 * sthread_t::fork()  
 *
 * Turn the "chunk of memory" into a real-live thread.
 */

w_rc_t	sthread_t::fork()
{
	/* can only fork a new thread */
	if (_status != t_virgin)
		return RC(stOS);

	_status = t_ready;

	/* Add us to the list of threads */
	_class_list->append(this);

	/* go go gadget ... schedule it */
	_ready_q->put(this);

	return RCOK;
}


/*
 *  sthread_t::sthread_t(priority, block_immediate, auto_del, name)
 *
 *  Create a thread.  Until it starts running, a created thread
 *  is just a memory object.
 */

sthread_t::sthread_t(priority_t		pr,
		     bool		block_immediate,
		     bool		auto_delete,
		     const char 	*nm,
		     unsigned		stack_size)
: sthread_named_base_t(nm),
  user(0),
  id(_next_id++),
  trace_level(0),
  _blocked(0),
  _terminate(new sevsem_t(0, "terminate")),
  _status(t_virgin),
  _priority(pr), 
  _ready_next(0),
  _tevent(0)
{
	if (!_terminate)
		W_FATAL(fcOUTOFMEMORY);
	_core.thread = (void *)this;
	
	/*
	   Both of these features have problems.  So ... they have been
	   removed!   These warning messages allow people to know
	   something is broken so they can fix thread creation/deletion.
	   */  
	
	if (auto_delete)
		cerr.form("sthread_t(%#lx): \"%s\": requesting auto-delete!\n",
			  (long)this, nm ? nm : "<noname>");
	if (block_immediate)
		cerr.form("sthread_t(%#lx): \"%s\": requesting block-immediate!\n",
			  (long)this, nm ? nm : "<noname>");
	
	/*
	 *  Set a valid priority level
	 */
	if (_priority > max_priority) 
		_priority = max_priority;
	else if (_priority <= min_priority) 
		_priority = min_priority;
	
	/*
	 *  Initialize the core.
	 */
	if (sthread_core_init(&_core, __start, this, stack_size) == -1) {
		cerr << "sthread_t: cannot initialize thread core" << endl;
		W_FATAL(stINTERNAL);
	}
}



/*
 *  sthread_t::~sthread_t()
 *
 *  Destructor. Thread must have already exited before its object
 *  is destroyed.
 */

sthread_t::~sthread_t()
{
	/* Valid states for destroying a thread are ...
	   1) It was never started
	   2) It ended.
	   3) There is some braindamage which imply that blocked threads
	   can be deleted.  This is sick and wrong, and it
	   can cause race conditions.  It is enables for compabilitiy,
	   and hopefully the warning messages will tell you if
	   something is wrong. */
	w_assert1(_status == t_virgin
		  || _status == t_defunct
		  || _status == t_blocked);

	/* XXX make sure the thread isn't on the ready Q, or
	   it isn't on a blocked list */

	if (_ready_next) {
		cerr.form("sthread_t(%#lx): \"%s\": destroying a thread on the ready queue!",
			  (long)this, name());
	}

	if (_link.member_of()) {
		cerr.form("sthread_t(%#lx): \"%s\": destroying a thread on a list!",
			  (long)this, name());
	}



	if (_blocked) {
		cerr.form("sthread_t(%#lx): \"%s\": destroying a blocked thread!",
			  (long)this, name());
		cerr << *_blocked << endl;

		_trace.release(_blocked);
		_blocked = 0;
	}

	sthread_core_exit(&_core);

	/* invalidate the core, attempt to ensure death if someone
	   tries to schedule a dead thread */
	memset(&_core, '\0', sizeof(_core));
	_core.thread = (void *) 0xdeadbeef;

	if (_rc)  {/*nothing*/;}
	delete _terminate;
}


/* A springboard from "C" function + argument into an object */
void	sthread_t::__start(void *arg)
{
	sthread_t &t = *(sthread_t *)arg;
	t._start();
}


/*
 *  sthread_t::_start(t)
 *
 *  All threads start and end here.
 */

void sthread_t::_start()
{
	w_assert1(_me == this);

	/* the context switch should have changed the status ... */
	w_assert3(_status == t_running);

#if defined(PURIFY)
	/* threads should be named in the constructor, not
	   in run() ... this is mostly useless if that happens */
	purify_name_thread(name());
#endif

	/* Save an exit jmpbuf and call run().  Either run() returns
	   or sthread_t::end() longjmp()'s here to terminate the thread */
	if (thread_setjmp(_exit_addr) == 0) {
		/* do not save sigmask */
		run();
	}

	/* Returned from run(). Current thread is ending. */
	w_assert3(_me == this);
	_status = t_defunct;
	_link.detach();
	_class_link.detach();

	/* An ordinary thread is ending, kill deschedule ourself
	   and run any ready thread */
	w_assert3(this == _me);
	_ctxswitch(t_defunct);

	W_FATAL(stINTERNAL);	// never reached
}



/*********************************************************************
 *
 *  sthread_t::block(timeout, list, caller)
 *
 *  Block the current thread and puts it on list. 
 *
 *********************************************************************/
w_rc_t
sthread_t::block(
    int4_t		timeout,	// 
    sthread_list_t*	list,		// list for thread after blocking
    const char* const	caller,		// for debugging only
    const void		*id)
{

    /*
     *  Sanity checks
     */
    w_assert3(_me->_link.member_of() == 0); // not in other list
    w_assert1(timeout != WAIT_IMMEDIATE);   // not 0 timeout

    /*
     *  Save caller name
     */
    _me->_blocked = _me->_trace.get(caller, id);

    /*
     *  Put on list
     */
    if (list)  {
	list->put_in_order(_me);
    }

    /*
     *  See if timeout is < 10; 10 is too small for some
     *  slow machine.
     *  remember timeout could be < 0(WAIT_FOREVER, etc)
     */
    if (timeout > WAIT_IMMEDIATE && timeout < 10)  {
	timeout = 10;        // make it 10
    }

    /*
     *  Allocate the timer on the stack.
     *  XXX This is a hack to avoid the cost
     *  of the constructor, which WAS magic, but no longer is.
     */
    char buf[ SIZEOF(sthread_timer_t) ];
    sthread_timer_t*		tevent = _me->_tevent;

    /*
       * WAIT_IMMEDIATE, WAIT_FOREVER are not times, and
       should be treated differently than time-outs.
       Does WAIT_IMMEDIATE imply a context switch?

       * What exactly do relative timeouts mean?
     */

    if (timeout >= 10)  {
	    stime_t when(stime_t::now() + stime_t::msec(timeout));

	    tevent = _me->_tevent = new (buf) sthread_timer_t(when, *_me);
	    _event_q.schedule(*tevent);
    }
    
    /*
     *  Give up CPU
     */
    _ctxswitch(t_blocked);

    /*
     *  Resume
     */
    if ( (tevent = _me->_tevent) )  {
	    if (!tevent->fired()) {
		    /* Someone called sthread_t::wakeup() before
		     * timer went off 
		     */
		    _event_q.cancel(*tevent);
	    }
	    tevent->sthread_timer_t::~sthread_timer_t();

	    // when buf goes out of scope soon, taking *_me->_tevent
	    _me->_tevent = 0;
    }

    return _me->_rc;
}



/*********************************************************************
 *
 *  sthread_t::unblock(rc)
 *
 *  Unblock the thread with an error in rc.
 *
 *********************************************************************/
w_rc_t
sthread_t::unblock(const w_rc_t& rc)
{
    w_assert3(_me != this);
    w_assert3(_status == t_blocked);

    _link.detach();
    _status = t_ready;

    if (_blocked) {
	    _trace.release(_blocked);
	    _blocked = 0;
    }

    /*
     *  Save rc (will be returned by block())
     */
    if (_rc)  {;}
    if (&rc) 
	_rc = rc;
    else
	_rc = RCOK;

    /*
     *  Thread is again ready.
     */
    _ready_q->put(this);

    return RCOK;
}



/*********************************************************************
 *
 *  sthread_t::yield()
 *  if do_select==true, we'll allow a select w/ 0 timeout
 *  to occur if the ready queue is empty
 *
 *  Give up CPU. Maintain ready status.
 *
 *********************************************************************/
void sthread_t::yield(bool do_select) // default is false
{
    w_assert3(_me->_status == t_running);

    if(do_select) {
	// we put the idle thread on the ready 
	// queue ahead of us
	w_assert3(_idle_thread != 0);
	_ready_q->change_priority(_idle_thread, _me->_priority);
    }
    _ctxswitch(t_ready);
    return;
}


/*
 *  sthread_t::end()
 *
 *  End thread prematurely (vis-a-vis naturally by returning 
 *  from run()). 
 */
void sthread_t::end()
{
	/* The main thread is special; for now, end()ing it is
	   equivalent to process termination.  Once shutdown() is
	   implemented, this could be wait for all other threads to
	   terminate before exiting. */
	if (_me->id == 0) {
		cout << "sthread_t::end(): process exit()ing via"
			<< " main thread end()ing." << endl;
		::exit(0);
	}
	
	/* return to cleanup code in start() */
	thread_longjmp(_me->_exit_addr, -1);
}


/* Attach and detach IO event handlers */
w_rc_t	sthread_t::io_start(sfile_hdl_base_t &event)
{
	if (!_io_events)
		return RC(stINTERNAL);
	return _io_events->start(event);
}

w_rc_t	sthread_t::io_finish(sfile_hdl_base_t &event)
{
	if (!_io_events)
		return RC(stINTERNAL);
	_io_events->stop(event);
	return RCOK;
}

bool	sthread_t::io_active(int fd)
{
	return _io_events ? _io_events->is_active(fd) : false;
}


/*********************************************************************
 *
 *  sthread_t::open(path, flags, mode, ret_fd)
 *
 *  Open the file "path". Return the file descriptor in ret_fd. 
 *  I/O on this file will block thread but not the process.
 *
 *********************************************************************/
w_rc_t
sthread_t::open(const char* path, int flags, int mode, int& ret_fd)
{
    ret_fd = -1;
    w_assert3(svcport && diskport);

    /* extract sthread I/O specific flags */
    bool	open_local = (flags & OPEN_LOCAL) == OPEN_LOCAL;
    flags &= ~OPEN_LOCAL;

    bool	open_keep = (flags & OPEN_KEEP) == OPEN_KEEP;
    flags &= ~OPEN_KEEP;

    bool	open_fast = (flags & OPEN_FAST) == OPEN_FAST;
    flags &= ~OPEN_FAST;

    /* Local I/O overrides oportunistic fastpath I/O */
    open_fast = open_fast || do_fastpath;
    if (open_local)
	    open_fast = false;

    bool keep_open = open_local || open_keep || open_fast;

    /*
     *  Let unix check for permission and such, or creation.
     */
    int fd = ::open(path, flags, mode);
    if (fd < 0) 
	return RC(stOS);

    if (!keep_open)
	    ::close(fd);
    
    flags &= ~O_CREAT;

    /* XXX should be in thread package statics */
    static smutex_t mutex("forkdisk");

    /*
     *  Enter critical section in manipulating diskport
     */
    W_COERCE( mutex.acquire() );

    if (diskport_cnt >= open_max) {
	mutex.release();
	if (keep_open)
		::close(fd);
	return RC(stTOOMANYOPEN);
    }

    /*
     *  Find a free diskport and instantiate an object on it
     */
    diskport_t *p;
    for (p = diskport; p->pid != 0; p++);
    new (p) diskport_t;
    diskport_cnt++;

    p->_fd = keep_open ? fd : 0;

    /* XXX code duplication to minimize changes.
       Don't start a diskrw for local-only I/O  */
    if (open_local) {
	    p->pid = -1;	// port in-use, but no process
	    ret_fd = p - diskport;
	    _io_flags[ret_fd] = IOF_KEEP | IOF_LOCAL;
	    mutex.release();

	    ret_fd = fd_base + ret_fd;
	    return RCOK;
    }

    /*
     *  Set up arguments for diskrw
     */
    char arg1[100];
    char arg2[200];
    ostrstream s1(arg1, sizeof(arg1));
    s1 << shmem_seg.id() << '\0';
    ostrstream s2(arg2, sizeof(arg2));
    s2	<< (char*)svcport - (char*)shmem_seg.base() << " " 
	<< (char*)p - (char*)shmem_seg.base() << " "
	<< flags << " "
	<< mode << '\0';
#ifdef PIPE_NOTIFY
    if (master_chan[0] == -1) {
	if (pipe(master_chan) == -1) {
		::perror("master chan");
		mutex.release();
		return RC(errno);
	}
	/* CLOSE_ON_EXEC(master_chan[PIPE_IN]);  */
	FD_NODELAY(master_chan[PIPE_IN]);
	W_COERCE(chan_event.change(master_chan[PIPE_IN]));
	W_COERCE(io_start(chan_event));
	chan_event.enable();
    }
    p->sv_chan[PIPE_IN] = master_chan[PIPE_IN];
    p->sv_chan[PIPE_OUT] = master_chan[PIPE_OUT];

    if (pipe(p->rw_chan) == -1) {
	w_rc_t e = RC(fcOS);
	::perror("diskrw chan");
	mutex.release();
	return e;
    }
    /* CLOSE_ON_EXEC(p->rw_chan[PIPE_OUT]); */
#endif /* PIPE_NOTIFY */
	    
    /*
     *  Fork 
     */
    pid_t pid = ::vfork();
    if (pid == -1)  {
	// failure

	cerr << "vfork  " << ::strerror(::errno) << endl;
	::exit(-1);
    } else if (pid == 0) {
	::close(p->_fd); // close original fd 
	/*
	 *  Child is running here. Exec diskrw.
	 */
	execlp(_diskrw, _diskrw, arg1, arg2, path, 0);
	cerr << "could not start disk I/O process, " << _diskrw
	     << ", because: " << ::strerror(::errno) << endl;
	kill(getppid(), SIGKILL);
	::exit(-1);
    } else {
      /*
       *  Parent is running here. 
       */
#ifdef PIPE_NOTIFY
      ::close(p->rw_chan[PIPE_IN]);
#endif
      p->pid = pid;

      ret_fd = p - diskport;
      /* keep_open always follows fastpath or OPEN_KEEP */
      _io_flags[ret_fd] = (open_fast ? (int)IOF_FASTPATH : 0)
	      | (keep_open ? (int)IOF_KEEP : 0);

      mutex.release();

      ret_fd = fd_base + ret_fd;
      w_assert3(ret_fd >= fd_base && ret_fd - fd_base < open_max);
    }

    return RCOK;
}



/*********************************************************************
 *
 *  sthread_t::close(fd)
 *
 *  Close a file previously opened with sthread_t::open(). Kill the
 *  diskrw process for this file.
 *
 *********************************************************************/
w_rc_t
sthread_t::close(int fd)
{
    // sync before close
    W_DO(sthread_t::fsync(fd));

    fd -= fd_base;
    if (fd < 0 || fd >= open_max)  {
	return RC(stBADFD);
    }
    bool close_local = _io_flags[fd] & IOF_KEEP;

    diskport_t* p = diskport + fd;

    if ((_io_flags[fd] & IOF_LOCAL) == 0) {

	    /*
	     *  Kill diskrw and wait for it to end.
	     */
	    w_assert3(p->outstanding() == 0);
	    if (kill(p->pid, SIGHUP) == -1) {
		    cerr << "cannot kill pid " << p->pid << endl
			    << RC(stOS) << endl;
		    ::exit(-1);
	    }
	    if (waitpid(p->pid, 0, 0) == -1) {
		    cerr << "cannot waitpid " << p->pid << endl
			    << RC(stOS) << endl;
		    ::exit(-1);
	    }
#ifdef PIPE_NOTIFY
	    ::close(p->rw_chan[PIPE_OUT]);
#endif
    }

    if (close_local)
	    ::close(p->_fd);

    /*
     *  Call destructor manually.
     */
    p->diskport_t::~diskport_t();
    _io_flags[fd] = 0;
    diskport_cnt--;

    if (diskport_cnt == 0) {
	// no diskrw running
#ifdef PIPE_NOTIFY
	W_COERCE(io_finish(chan_event));
	W_COERCE(chan_event.change(FD_NONE));
	::close(master_chan[PIPE_IN]);
	::close(master_chan[PIPE_OUT]);
	master_chan[PIPE_IN] = FD_NONE;
	master_chan[PIPE_OUT] = FD_NONE;
#endif /* PIPE_NOTIFY */
    }

    return RCOK;
}




/*********************************************************************
 *
 *  sthread_t::_io(fd, op, vec, cnt, other)
 *
 *  Perform an I/O of type "op" on the file "fd" previously
 *  opened with sthread_t::open(). 
 *
 *********************************************************************/
w_rc_t
sthread_t::_io(int fd, int op, const struct iovec* v, int iovcnt, off_t other)
{
    if (_io_in_progress++) SthreadStats.ccio++;
    
    SthreadStats.io++;

    fd -= fd_base;
    w_assert3(fd >= 0 && fd < open_max);
    if (fd < 0 || fd >= open_max)  {
	return RC(stBADFD);
    }
    
    diskv_t diskv[iovec_max];
    if (iovcnt > iovec_max)
	return RC(stBADIOVCNT);

    diskport_t& p = diskport[fd];
    int total = 0;
    int cc=0;

    bool local_io = (_io_flags[fd] & IOF_LOCAL);
    bool take_fastpath =  local_io || (_io_in_progress == 1  &&
				       (_io_flags[fd] & IOF_FASTPATH) &&
				       _ready_q->contains_only(_idle_thread));

#if 1	    
    /* Fastpath writes cause a consistency problem with the write
       cache in diskrw.  Account for the missed opportunity, and do
       a normal write. */
    if (!local_io && take_fastpath && op == diskmsg_t::t_write)
	    SthreadStats.nofastwrite++;
    else
#endif
    /* Only do fastpath I/O if nothing else is happening.
       OR if the I/O should be kept "local" */
    if (take_fastpath) {
	w_rc_t	e;
	int	totalcc = 0;
	off_t	pos;

	switch (op) {
	case diskmsg_t::t_write:
	case diskmsg_t::t_read:
		for (int i = 0; i < iovcnt; i++)
			totalcc += v[i].iov_len;
		break;
	}

	if (local_io)
		SthreadStats.local_io++;
	else
		SthreadStats.fastpath++;

#ifdef EXPENSIVE_STATS
	stime_t start_time(stime_t::now());
#endif

	switch(op) {
	case diskmsg_t::t_sync:
		cc = ::fsync(p._fd);
		/* fsyncs to r/o files and devices can fail ok */
		if (cc == -1 && !(errno == EBADF || errno == EINVAL)) 
			e = RC(stOS);
		break;

	case diskmsg_t::t_trunc:
		cc = ::ftruncate(p._fd, other);
		if (cc == -1)
			e = RC(stOS);
		break;

	case diskmsg_t::t_read:
		pos = ::lseek(p._fd, p.pos, SEEK_SET);
		if (pos != p.pos) {
			e = (pos == -1) ? RC(stOS) : RC(stSHORTSEEK);
			break;
		}

#if defined(SOLARIS2) /* and other OS's with non-const iovs */
		/* XXX bad solution, but need to copy it otw */
		cc = ::readv(p._fd, (struct iovec *)v, iovcnt);
#else
		cc = ::readv(p._fd, v, iovcnt);
#endif

		if (cc == -1)
			e = RC(stOS);
		else {
			p.pos += cc;
			if (cc != totalcc)
				e = RC(stSHORTIO);
		}

		break;

	case diskmsg_t::t_write: 
		pos = ::lseek(p._fd, p.pos, SEEK_SET);
		if (pos != p.pos) {
			e = (pos == -1) ? RC(stOS) : RC(stSHORTSEEK);
			break;
		}

		cc = ::writev(p._fd, v, iovcnt);

		if (cc == -1)
			e = RC(stOS);
		else {
			p.pos += cc;
			if (cc != totalcc)
				e = RC(stSHORTIO);
		}
		break;

        default:
		e = RC(stBADADDR);
		break;
        }

	_io_in_progress--;

#ifdef EXPENSIVE_STATS
	double delta = (float) (stime_t::now() - start_time);
	SthreadStats.io_time +=  delta;
	/* It's idle time because no threads can run :-( */     
	SthreadStats.idle_time += delta;
#endif

	yield();

        return e;
    }

    w_assert3((_io_flags[fd] & IOF_LOCAL) == 0);

    off_t pos = p.pos;

    switch(op) {
    case diskmsg_t::t_sync:
	break;
    case diskmsg_t::t_trunc:
	break;

    case diskmsg_t::t_read:
    case diskmsg_t::t_write: {
            /*
             *  Convert pointer to offset relative to shared memory
             *  segment.
             */
            for (int i = 0; i < iovcnt; i++)  {
                diskv[i].bfoff = ((char *)v[i].iov_base) - shmem_seg.base();
                diskv[i].nbytes = v[i].iov_len;
                total += v[i].iov_len;

                if (diskv[i].bfoff < 0 ||
                    (uint)(diskv[i].bfoff + diskv[i].nbytes) > shmem_seg.size())   {
                    return RC(stBADADDR);
                }
            }
        }

	break;
    default: 
	return RC(stBADADDR);
    }

    /*
     *  Send message to diskrw process to perform the I/O. Current
     *  thread will block in p.send() until I/O completes.
     */

#ifdef EXPENSIVE_STATS
    stime_t start_time(stime_t::now());
#endif

    p.send(diskmsg_t((diskmsg_t::op_t)op, 
	(op == diskmsg_t::t_trunc ? other : pos), diskv, iovcnt));

#ifdef EXPENSIVE_STATS
    SthreadStats.io_time +=  (float) (stime_t::now() - start_time);
#endif

    --_io_in_progress;
    p.pos = pos + total;

    return RCOK;
}



/*********************************************************************
 *
 *  sthread_t::write(fd, buf, n)
 *  sthread_t::writev(fd, iov, iovcnt)
 *  sthread_t::read(fd, buf, n)
 *  sthread_t::readv(fd, iov, iovcnt)
 *  sthread_t::fsync(fd)
 *  sthread_t::ftruncate(fd, len)
 *
 *  Call _io to perform I/O for current thread.
 *
 *********************************************************************/
w_rc_t
sthread_t::writev(int fd, const struct iovec* iov, size_t iovcnt)
{
    return _io(fd, diskmsg_t::t_write, iov, iovcnt);
}

w_rc_t
sthread_t::write(int fd, const void* buf, int n)
{
    iovec iov;
    iov.iov_base = (caddr_t) buf;
    iov.iov_len = n;
    return _io(fd, diskmsg_t::t_write, &iov, 1);
}

w_rc_t
sthread_t::read(int fd, const void* buf, int n)
{
    iovec iov;
    iov.iov_base = (caddr_t) buf;
    iov.iov_len = n;
    return _io(fd, diskmsg_t::t_read, &iov, 1);
}

w_rc_t
sthread_t::fsync(int fd)
{
    return _io(fd, diskmsg_t::t_sync, 0, 0);
}

w_rc_t
sthread_t::ftruncate(int fd, off_t n)
{
    return _io(fd, diskmsg_t::t_trunc, 0,0, n);
}


/*********************************************************************
 *
 *  sthread_t::lseek(fd, offset, whence, ret)
 *
 *  Perform a seek on the file previous opened with sthread_t::open().
 *
 *********************************************************************/
w_rc_t
sthread_t::lseek(int fd, off_t offset, int whence, off_t& ret)
{
    fd -= fd_base;
    if (fd < 0 || fd >= open_max)  {
	return RC(stBADFD);
    }
    if(whence == SEEK_CUR) {
	diskport[fd].pos +=  offset;
    } else if(whence == SEEK_SET) {
	diskport[fd].pos = offset;
    } else {
	// cannot support SEEK_END;
	return RC(stINVAL);
    }
    ret = diskport[fd].pos;
    return RCOK;
}

/* currently lseek can't ever seek short, just pass through. */
w_rc_t
sthread_t::lseek(int fd, off_t offset, int whence)
{
	off_t	unused;
	return lseek(fd, offset, whence, unused);
}

w_rc_t	sthread_t::fstat(int fd, struct stat &st)
{
	fd -= fd_base;
	if (fd < 0 || fd >= open_max)
		return RC(stBADFD);

	if ((_io_flags[fd] & IOF_KEEP) == 0)
		return RC(stINVALIDIO);

	int n;
	n = ::fstat(diskport[fd]._fd, &st);
	return n == -1 ? RC(stOS) : RCOK;
}

w_rc_t	sthread_t::fgetfile(int fd, void *to)
{
	fd -= fd_base;
	if (fd < 0 || fd >= open_max)
		return RC(stBADFD);

	if ((_io_flags[fd] & IOF_KEEP) == 0)
		return RC(stINVALIDIO);

	*(int *)to = diskport[fd]._fd;
	return RCOK;
}

w_rc_t	sthread_t::fgetgeometry(int fd, struct disk_geometry &dg)
{
	fd -= fd_base;
	if (fd < 0 || fd >= open_max)
		return RC(stBADFD);

	if ((_io_flags[fd] & IOF_KEEP) == 0)
		return RC(stINVALIDIO);

	return sdisk_getgeometry(diskport[fd]._fd, dg);
}

w_rc_t	sthread_t::fisraw(int fd, bool &raw)
{
	w_rc_t	e;
	struct	stat st;

	e = fstat(fd, st);
	if (e)
		return e;

	raw = ((st.st_mode & S_IFMT) == S_IFCHR);
	return RCOK;
}



/*********************************************************************
 *
 *  sthread_t::push_resource_alloc(const char* n, void *id, bool isLatch)
 *  sthread_t::pop_resource_alloc(void *id)
 *  sthread_t::dump(str, o)
 *
 *  For debugging.
 *
 *********************************************************************/

#if 0
/* this left here in case a non-inline version is needed */

void
sthread_t::push_resource_alloc(const char* n, void *id, bool isLatch)
{
	_trace.push(n, id, isLatch);
}

void
sthread_t::pop_resource_alloc(void *id)
{
	_trace.pop(id);
}

#endif


/* print all threads */
void sthread_t::dump(const char *str, ostream & o)
{
    w_list_i<sthread_t> i(*_class_list);

    if(str) {
	o << str << ": " << endl;
    }
    while (i.next())  {
	o << "******* " ;
	if(_me == i.curr()) {
	    o << " --->ME<---- ";
	}
	o << endl;
	i.curr()->_dump(o);
    }

    if (_io_events) {
	    if (str)
		    o << str << ": " << endl;
	    _io_events->print(o);
    }

}


/* XXX individual thread dump function... obsoleted by print method */
void sthread_t::_dump(ostream &o)
{
    o << *this << endl;
}

void
sthread_t::dump_stats(ostream & o)
{
    o << SthreadStats << endl << endl;
}

void
sthread_t::reset_stats()
{
    SthreadStats.clear();
}


char *sthread_t::status_strings[] = {
	"defunct",
	"virgin",
	"ready",
	"running",
	"blocked",
	"boot"
};

char *sthread_t::priority_strings[]= {
	"idle_time",
	"fixed_low",
	"regular",
	"time_critical"
};


ostream& operator<<(ostream &o, const sthread_t &t)
{
	return t.print(o);
}


/*
 *  sthread_t::print(stream)
 *
 *  Print thread status to an stream
 */
ostream &sthread_t::print(ostream &o) const
{
	o << "thread id = " << id ;

	if (name()) {
		o << ", name = " << name();
	};

	o
	<< ", addr = " <<  (void *) this
	<< ", core = " <<  (void *) &_core << endl;
	o
	<< "priority = " << sthread_t::priority_strings[priority()]
	<< ", status = " << sthread_t::status_strings[status()];
	o << endl;	      

	o << _core << endl;

	if (user)
		o << "user = " << user << endl;
	
	if (status() == t_blocked && _blocked)
		o << "blocked on: " << *_blocked;
	
	_trace.print(o);
	
	if (status() != t_defunct  &&  !sthread_core_stack_ok(&_core, 0))
		cerr << "***  warning:  Thread stack overflow  ***" << endl;
	
	return o;
}



/*********************************************************************
 *
 *  smutex_t::for_each_thread(ThreadFunc& f)
 *
 *  For each thread in the system call the function object f.
 *
 *********************************************************************/
void sthread_t::for_each_thread(ThreadFunc& f)
{
    w_list_i<sthread_t> i(*_class_list);

    while (i.next())  {
	f(*i.curr());
    }
}

void print_timeout(ostream& o, const long timeout)
{
    if (timeout > 0)  {
	o << timeout;
    }  else if (timeout >= -5)  {
	const char* names[] = {"WAIT_IMMEDIATE", "WAIT_FOREVER", "WAIT_ANY", "WAIT_ALL",
			"WAIT_SPECIFIED_BY_THREAD", "WAIT_SPECIFIED_BY_XCT"};
	o << names[-timeout];
    }  else  {
	o << "UNKNOWN_VALUE(" << timeout << ")";
    }
}



/*********************************************************************
 *
 *  smutex_t::smutex_t(name)
 *
 *  Construct a mutex. Name is used for debugging.
 *
 *********************************************************************/
smutex_t::smutex_t(const char* nm)
    : holder(0), waiters()
{
    if (nm) rename(nm?"m:":0, nm);
}



/*********************************************************************
 *
 *  smutex_t::~smutex_t()
 *
 *  Destroy the mutex. There should not be any holder.
 *
 *********************************************************************/
smutex_t::~smutex_t()
{
    w_assert1(!holder);
}



/*********************************************************************
 *
 *  smutex_t::acquire(timeout)
 *
 *  Acquire the mutex. Block and wait for up to timeout milliseconds
 *  if some other thread is holding the mutex.
 *
 *********************************************************************/
w_rc_t 
smutex_t::acquire(int4_t timeout)
{
    FUNC(smutex_t::acquire);
    sthread_t* self = sthread_t::me();


    w_rc_t ret;
    if (! holder) {
	/*
	 *  No holder. Grab it.
	 */
	w_assert3(waiters.num_members() == 0);
	holder = self;
    } else {
	/*
	 *  Some thread holding this mutex. Block and wait.
	 */
	if (self == holder)  {
	    cerr << "fatal error -- deadlock while acquiring mutex";
	    if (name()) {
		cerr << " (" << name() << ")";
	    }
	    cerr << endl;
	    W_FATAL(stINTERNAL);
	}

	if (timeout == WAIT_IMMEDIATE)
	    ret = RC(stTIMEOUT);
	else  {
	    DBGTHRD(<<"block on mutex " << (long)this);
	    SthreadStats.mutex_wait++;
	    ret = sthread_t::block(timeout, &waiters, name(), this);
	}
    }
#if defined(DEBUG) || defined(SHORE_TRACE)
    DBGTHRD(<<"acquired mutex " << (long)this);
    if (! ret.is_error()) {
	self->push_resource_alloc(name(), this);
    }
#endif 
    return ret;
}

#ifndef DEBUG
/* Fast path version of acquire(). */

w_rc_t smutex_t::acquire()
{
    if (holder) {
	SthreadStats.mutex_wait++;
	w_rc_t e = sthread_t::block(WAIT_FOREVER, &waiters, name(), this);
#if defined(SHORE_TRACE)
	if (e == RCOK)
		sthread_t::me()->push_resource_alloc(name(), this);
#endif
	return e;
    } else {
	holder = sthread_t::me();
#if defined(SHORE_TRACE)
        sthread_t::me()->push_resource_alloc(name(), this);
#endif
	return RCOK;
    }
}
#endif /* !DEBUG */




/*********************************************************************
 *
 *  smutex_t::release()
 *
 *  Release the mutex. If there are waiters, then wake up one.
 *
 *********************************************************************/
void
smutex_t::release()
{
    w_assert3(holder == sthread_t::me());

    DBGTHRD(<<"releasing mutex " << (long)this);

#if defined(DEBUG) || defined(SHORE_TRACE) 
    holder->pop_resource_alloc(this);
#endif 
   
    holder = waiters.pop();
    if (holder) {
	W_COERCE( holder->unblock() );
    }
    DBGTHRD(<<"releasED mutex " << (long)this);
}


/*********************************************************************
 *
 *  scond_t::scond_t(name)
 *
 *  Construct a conditional variable. "Name" is used for debugging.
 *
 *********************************************************************/
scond_t::scond_t(const char* nm)
: _waiters()
{
    if (nm) rename(nm?"c:":0, nm);
}


/*
 *  scond_t::~scond_t()
 *
 *  Destroy a condition variable.
 */
scond_t::~scond_t()
{
}


/*
 *  scond_t::wait(mutex, timeout)
 *
 *  Wait for a condition. Current thread release mutex and wait
 *  up to timeout milliseconds for the condition to fire. When
 *  the thread wakes up, it re-acquires the mutex before returning.
 *  
 */
w_rc_t
scond_t::wait(smutex_t& m, int4_t timeout)
{
    w_rc_t rc;

    if (timeout == WAIT_IMMEDIATE)  {
	rc = RC(stTIMEOUT);
    } else {
	m.release();
	rc = sthread_t::block(timeout, &_waiters, name(), this);
	W_COERCE(m.acquire());
	SthreadStats.scond_wait++;
    }
    
    return rc;
}


/*
 *  scond_t::signal()
 *
 *  Wake up one waiter of the condition variable.
 */
void scond_t::signal()
{
    sthread_t* t;
    t = _waiters.pop();
    if (t)  {
	W_COERCE( t->unblock() );
    }
}


/*
 *  scond_t::broadcast()
 *
 *  Wake up all waiters of the condition variable.
 */
void scond_t::broadcast()
{
    sthread_t* t;
    while ((t = _waiters.pop()) != 0)  {
	W_COERCE( t->unblock() );
    }
}



/*********************************************************************
 *
 *  sevsem_t::sevsem_t(is_post, name)
 *
 *  Construct an event semaphore. If "is_post" is true, semaphore
 *  is immediately posted. "Name" is used for debugging.
 *
 *********************************************************************/
sevsem_t::sevsem_t(int is_post, const char* name)
: _mutex(name), _cond(name), _post_cnt(0)
{
    if (is_post) _post_cnt = 1;
}


/*********************************************************************
 *
 *  sevsem_t::~sevsem_t()
 *
 *  Destroy an event semaphore. There should be no waiters.
 *
 *********************************************************************/
sevsem_t::~sevsem_t()
{
    if ((_mutex.acquire(WAIT_IMMEDIATE) != RCOK) || 
	_cond.is_hot())  {
	cerr << "evsem_t::~evsem_t:  fatal error --- semaphore busy\n";
	W_FATAL(stINTERNAL);
    }

    _mutex.release();
} 



/*********************************************************************
 *
 *  sevsem_t::post()
 *
 *  Post an event semaphore. All waiters are awakened and the posted
 *  state is saved until the next reset()---all subsequent wait()
 *  calls will succeed and return immediately until the next reset().
 *
 *  Return stSEMPOSTED if semaphore is already posted when post()
 *  is called.
 *
 *********************************************************************/
w_rc_t
sevsem_t::post()
{
    W_COERCE( _mutex.acquire() );
    if (_post_cnt++)  {
	_cond.broadcast();
        _mutex.release();
	return RC(stSEMPOSTED);
    }
    _cond.broadcast();
    _mutex.release();
    return RCOK;
}



/*********************************************************************
 *
 *  sevsem_t::reset(pcnt)
 *
 *  Save the semaphore post count in pcnt and reset the post count.
 *  Return stSEMALREADYRESET if post count is 0 when reset() is called.
 *
 *********************************************************************/
w_rc_t
sevsem_t::reset(int* pcnt)
{
    W_COERCE( _mutex.acquire() );
    if (_post_cnt) {
	if (pcnt) *pcnt = CAST(int,_post_cnt);
	_post_cnt = 0;
	_mutex.release();
	return RCOK;
    } 
    _mutex.release();
    return RC(stSEMALREADYRESET);
}


/*********************************************************************
 *
 *  sevsem_t::wait(timeout)
 *
 *  Wait up to timeout milliseconds for an event to occur.
 *
 *********************************************************************/
w_rc_t
sevsem_t::wait(long timeout)
{
    W_COERCE( _mutex.acquire() );
    if (_post_cnt == 0)  {
	w_rc_t rc = _cond.wait(_mutex, timeout);
	SthreadStats.sevsem_wait++;
	_mutex.release();
	return rc.reset();
    }
    _mutex.release();
    return RCOK;
}



/*********************************************************************
 *
 *  sevsem_t::query(pcnt)
 *
 *  Return the post count in pcnt.
 *
 *********************************************************************/
void
sevsem_t::query(int& pcnt)
{
    pcnt = CAST(int, _post_cnt);
}


sthread_name_t::sthread_name_t() {
    memset(_name, '\0', sizeof(_name));
}

sthread_name_t::~sthread_name_t() {
}

void
sthread_name_t::rename(
    // can't have n2 or n3 without n1
    // can have n1,0,n3 or n1,n2,0
    const char*		n1,
    const char*		n2,
    const char*		n3)
{
	const int sz = sizeof(_name) - 1;
	size_t len = 0;
	_name[0] = '\0';
	if (n1)  {
#ifdef DEBUG
		len = strlen(n1);
		if(n2) len += strlen(n2);
		if(n3) len += strlen(n3);
		len++;
		if(len>sizeof(_name)) {
			cerr << "WARNING-- name too long for sthread_named_t: "
				<< n1 << n2 << n3;
		}
#endif

		// only copy as much as will fit
		strncpy(_name, n1, sz);
		len = strlen(_name);
		if (n2 && len < sz)  {
			strncat(_name, n2, sz - len);
			len = strlen(_name);
			if (n3 && len < sz)
				strncat(_name, n3, sz - len);
		}

		_name[sz] = '\0';
	}
}

void
sthread_named_base_t::unname() { 
	if(_name) { 
		delete _name; _name=0; 
	} 
}

void
sthread_named_base_t::rename(
    // can't have n2 or n3 without n1
    // can have n1,0,n3 or n1,n2,0
    const char*		n1,
    const char*		n2,
    const char*		n3)
{
    unname();
    if(n1) {
	_name = new sthread_name_t;
	SthreadStats.namemallocs++;
	DBG(<<"malloc for " << n1 << n2 << n3);
	_name->rename(n1,n2,n3);
    }
}

sthread_named_base_t::~sthread_named_base_t()
{
	unname();
}


#include "sthread_stats_op.i"
const char *sthread_stats::stat_names[] = {
#include "sthread_stats_msg.i"
};

#include "shmc_stats_op.i"
const char *shmc_stats::stat_names[] = {
#include "shmc_stats_msg.i"
};



/*********************************************************************
 *
 *  sthread_priority_list_t::sthread_priority_list_t()
 *
 *  Construct an sthread list sorted by priority level.
 *
 *********************************************************************/
NORET
sthread_priority_list_t::sthread_priority_list_t()
    : w_descend_list_t<sthread_t, sthread_t::priority_t> (
	offsetof(sthread_t, _priority),
	offsetof(sthread_t, _link))  
{
}


/*********************************************************************
 *
 *  ready_q_t::ready_q_t()
 *
 *  Construct a ready queue.
 *
 *********************************************************************/
NORET
ready_q_t::ready_q_t() : count(0)
{
    for (int i = 0; i < nq; i++)  {
	head[i] = tail[i] = 0;
    }
}

bool			
ready_q_t::contains_only(sthread_t *t) const	
{ 
     return (count == 0) ||
	((count == 1) && t && (t->status() == sthread_t::t_ready));
}


/*********************************************************************
 *
 *  ready_q_t::get()
 *
 *  Dequeue and return a thread from the highest priority queue
 *  that is not empty. If all queues are empty, return 0.
 *
 *********************************************************************/
sthread_t*
ready_q_t::get()
{
    sthread_t* t = 0;
    /*
     *  Find the highest queue that is not empty
     */
    register i;
    for (i = nq-1; i >= 0 && !head[i]; i--);
    if (i >= 0)  {
	/*
	 *  We found one. Dequeue the first entry.
	 */
	t = head[i];
	w_assert1(count > 0);
	--count;
	head[i] = t->_ready_next;
	if(head[i]==0) tail[i] = 0;
	t->_ready_next = 0;
    }
    return t;
}


/*********************************************************************
 *
 *  ready_q_t::put(t)
 *
 *  Insert t into the queue of its priority.
 *
 *********************************************************************/
#ifdef DEBUG
void stop() {}
#endif
void
ready_q_t::put(sthread_t* t)
{
    t->_ready_next = 0;
    int i = t->priority();	// i index into the proper queue
    w_assert3(i >= 0 && i < nq);
    ++count;
#ifdef DEBUG
    if(count > 1) {
	// a place to stop in dbg
	stop();
    }
#endif
    if (head[i])  {
	w_assert3(tail[i]->_ready_next == 0);
	tail[i] = tail[i]->_ready_next = t;
    } else {
	head[i] = tail[i] = t;
    }
}

/*********************************************************************
 *
 *  ready_q_t::change_priority(t,p)
 *
 *  dequeue t, change its priority to p, and re-queue it
 *  that is not empty. If all queues are empty, return 0.
 *
 *********************************************************************/
void
ready_q_t::change_priority(sthread_t* target, sthread_t::priority_t p)
{
    sthread_t* t = 0, *tp=0;
    /*
     *  Find the first (highest) queue that is not empty
     */
    for (register int i = nq-1 ;i >= 0; i--) {
	/*
	 *  We found a nonempty queue.
	 *  Search it for our target thread.
	 */
	for(tp=0, t=head[i]; t; tp=t ,t=t->_ready_next) {
	    if(t==target) {
		/*
		 * FOUND
		 * tp points to previous item on the list
		 * dequeue it.
		 */
		if(tp) {
		    tp->_ready_next = t->_ready_next;
		    if (t == tail[i])
			    tail[i] = tp;
		} else {
		    w_assert3(t == head[i]);
		    head[i] = t->_ready_next;
		    if(head[i]==0) tail[i] = 0;
		}
		--count;
		t->_ready_next = 0;

		// Set the new priority level
		{
		    // set status so that set_priority doesn't complain
		    sthread_t::status_t old_status = t->status();
		    t->_status = sthread_t::t_defunct;
		    W_COERCE( t->set_priority(p) );
		    t->_status = old_status;
		}

		/*
		 * put it back, at the new priority
		 */
		put(t);
		return;
	    }
	}
    }
#ifdef DEBUG
    /* This isn't an error, but something may be mixed up if the
       thread is expected to be in the ready Q */
    cerr.form("change_priority can't find thread %#lx(%s)\n",
	      target, target->name());
#endif
}


// if you really are a sthread_t return 0
smthread_t* sthread_t::dynamic_cast_to_smthread()
{
	return 0;
}


const smthread_t* sthread_t::dynamic_cast_to_const_smthread() const
{
	return 0;
}


/*********************************************************************
 *
 *  dumpthreads()
 *  For debugging, but it's got to be
 *  present in servers compiled without debugging.  
 *
 *********************************************************************/
void dumpthreads()
{
    sthread_t::dump("dumpthreads()", cerr);

    // Took out temporarily in order to circumvent
    // some gcc/purify problem (with printing doubles)
    // (Probably an alignment problem.)
    // cerr << SthreadStats  << endl;
    cerr << ShmcStats  << endl;
}

void threadstats()
{
	cerr << SthreadStats << endl;
	cerr << ShmcStats  << endl;
}


/* If a write to a bogus pipe/socket occurs, we get a SIGPIPE, which
   normally ends the process.  This is bad because it kills the
   process!  The system call will return the proper "can't be used
   anymore" error, and so the error is detectable.  Ignoring the
   signal ensures that we don't need to die from problems like
   that. */

static	void	hack_signals()
{
	struct	sigaction	sa;
	int	kr;

	kr = sigaction(SIGPIPE, 0, &sa);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "Warning: can't ignore SIGPIPE:" << endl << e << endl;
		return;
	}
	sa.sa_handler = W_POSIX_HANDLER SIG_IGN;
	kr = sigaction(SIGPIPE, &sa, 0);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "Warning: can't ignore SIGPIPE:" << endl << e << endl;
		return;
	}
	
}


#if defined(AIX32) || defined(AIX41)
/* Change signal masks so core dumps occur */
/* XXX other signal handlers may need flag changes */

static int signals_to_hack[] = {
	SIGSEGV, SIGILL, SIGBUS,
	SIGABRT, SIGEMT,
	0 };

static void hack_aix_signals()
{
	struct sigaction sa;
	int	i;
	int	kr;
	int	sig;

	for (i = 0; sig = signals_to_hack[i]; i++) {
		kr = sigaction(sig, 0, &sa);
		if (kr == -1) {
			w_rc_t	e = RC(sthread_base_t::stOS);
			cerr.form("sigaction(%d, FETCH):\n", sig);
			cerr << e << endl;
			continue;
		}

		sa.sa_flags |= SA_FULLDUMP;

		kr = sigaction(sig, &sa, 0);
		if (kr == -1) {
			w_rc_t e = RC(sthread_base_t::stOS);
			cerr.form("sigaction(%d, SET): AIX core hack:\n",
				  sig);
			cerr << e << endl;
		}
	}
}
#endif /* AIX signal hacks */

#ifdef DEBUG
/*
   Search all thread stacks s for the target address
 */
extern bool address_in_stack(sthread_core_t &core, void *address);

void sthread_t::find_stack(void *addr)
{
	w_list_i<sthread_t> i(*sthread_t::_class_list);
	
	while (i.next())  {
		if (address_in_stack(i.curr()->_core, addr)) {
			cout.form("*** address %#lx found in ...\n",
				  (long)addr);
			i.curr()->print(cout);
		}
	}
}

extern "C" void find_thread_stack();

void find_thread_stack(void *addr)
{
	sthread_t::find_stack(addr);
}
#endif
